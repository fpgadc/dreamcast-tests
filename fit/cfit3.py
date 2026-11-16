"""Fit the perspective color path: (color * depth) interpolated, divided by depth, rounded.

Data: zblock pages 0 and 2 (flat red Gouraud quad with a steep depth slope: the console shows 254 in places)
and shade page 0 (Gouraud ramps at depth 1).
"""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zgroup as g

f32 = lambda v: float(np.float32(v))
R = g.R
rows = []       # tri xy(6), z(3), c(3), parity, px, py, want, tag


def hw_rgb(test, page):
    a = np.fromfile(Path("//wsl.localhost/Ubuntu/tmp/pvr_tests") / test / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)


for page, (zl, zr) in ((0, (f32(32.0), f32(0.52))), (2, (f32(1.0), f32(32.77)))):
    im = hw_rgb("zblock", page)
    for c in range(0, 240, 7):
        x, y = (c % 20) * 32.0, (c // 20) * 32.0
        for t, par in z3.strip_tris(x, y, 32, 32):
            for dx in range(0, 28, 1):
                for dy in range(0, 32, 3):
                    px, py = x + dx + 0.5, y + dy + 0.5
                    if z3.inside(t, px, py):
                        rows.append(([v for p in t for v in p], [zr if vx > x else zl for vx, vy in t], [255.0] * 3, par, px, py,
                                     im[int(py), int(px), 0], page))
import cfit_data
rows += cfit_data.rows

T = np.array([r[0] for r in rows]); Z = np.array([r[1] for r in rows]); C = np.array([r[2] for r in rows]); PAR = np.array([r[3] for r in rows])
X = np.array([r[4] for r in rows]); Y = np.array([r[5] for r in rows]); WANT = np.array([r[6] for r in rows]); TAG = np.array([r[7] for r in rows])
TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32
g.X, g.Y, g.TX, g.TY = X, Y, TX, TY
CZ = g.coefs(T, Z, PAR)


def block(Cf, start_bits, slope_mant, slope_grid, cut, ref_e=None):
    A, B, Cc, _ = Cf
    first = R(R(R(A * (TX + 0.5)) + R(B * (TY + 0.5))) + Cc)
    e = np.frexp(first)[1] if ref_e is None else ref_e
    unit = lambda b: np.ldexp(1.0, e - b)
    start = np.trunc(first / unit(start_bits)) * unit(start_bits)
    sl = lambda s: np.trunc(z3.make_rnd(slope_mant, "trunc")(s) / unit(slope_grid)) * unit(slope_grid)
    v = start + sl(A) * (X - TX - 0.5) + sl(B) * (Y - TY - 0.5)
    return (z3.make_rnd(cut, "trunc")(v) if cut else v), e


def color(P):
    perm = P["perm"]
    if P["premul"]:
        vals = R(C * Z)
    else:
        vals = C
    CN = g.coefs(T, vals, PAR) if perm == "depth" else coefs_plain(vals)
    den, e = block(CZ, 20, 20, 26, P["den_cut"])
    if P["num"] == "block":
        num, _ = block(CN, P["num_start"], 20, P["num_grid"], P["num_cut"])
    elif P["num"] == "block, depth's exponent":
        num, _ = block(CN, P["num_start"], 20, P["num_grid"], P["num_cut"], e + 8)
    else:
        A, B, Cc, _ = CN
        num = R(R(R(A * X) + R(B * Y)) + Cc)
    if P["premul"]:
        with np.errstate(all="ignore"):
            if P["div"] == "div":
                v = z3.make_rnd(P["div_bits"], "trunc")(num / den)
            else:
                v = z3.make_rnd(P["div_bits"], "trunc")(num * z3.make_rnd(P["div_bits"], "trunc")(1.0 / den))
    else:
        v = num
    return np.clip(np.floor(v + P["bias"]), 0, 255).astype(int)


def coefs_plain(vals):
    return g.coefs(T, vals, np.zeros(len(T), dtype=int) + 2)        # unused variant


OPT = {"perm": ["depth"], "premul": [True], "num": ["float", "block", "block, depth's exponent"], "num_start": [20, 24, 28],
       "num_grid": [26, 30, 34], "num_cut": [0, 19, 24], "den_cut": [19, 0], "div": ["div", "recip"], "div_bits": [24, 19, 20],
       "bias": [0.497, 0.5]}
keys = list(OPT)
res = []
for combo in itertools.product(*[OPT[k] for k in keys]):
    P = dict(zip(keys, combo))
    if P["num"] == "float" and (P["num_start"], P["num_grid"], P["num_cut"]) != (20, 26, 0):
        continue
    ok = color(P) == WANT
    res.append((int(ok.sum()), P, {int(t): int(ok[TAG == t].sum()) for t in np.unique(TAG)}))
res.sort(key=lambda r: -r[0])
print(len(rows), "samples; per set:", {int(t): int((TAG == t).sum()) for t in np.unique(TAG)}, "(0, 2 = zblock pages, 9 = shade)")
print("254s on the console in zblock samples:", int(((WANT == 254) & (TAG != 9)).sum()))
for s, P, by in res[:12]:
    print(s, {k: v for k, v in P.items() if k not in ("perm", "premul")}, by)
