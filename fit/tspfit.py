"""Fit the TSP's attribute interpolation as its own block: planes for color * depth and for depth evaluated
per pixel with their own mantissa widths, then divided.

Data: the red channel of the steep, flat red, Gouraud shaded quads of zblock (pages 0, 2) and ztile (pages 0, 2),
where the console shows 254 / 253 in particular columns of particular cells; and the Gouraud page of shade.
"""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zgroup as g
import cfit_data

f32 = lambda v: float(np.float32(v))
T_ = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests")
rows = []       # tri(6) z(3) c(3) parity px py want tag


def chan(test, page):
    a = np.fromfile(T_ / test / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
    return ((a >> 16) & 255).astype(int), ((a >> 8) & 255).astype(int)


def add_quad(x0, y0, w, zl, zr, r, gch, tag, xs):
    for t, par in z3.strip_tris(x0, y0, w, 32):
        for dx in xs:
            for dy in (1, 9, 17, 25, 30):
                px, py = x0 + dx + 0.5, y0 + dy + 0.5
                if z3.inside(t, px, py) and gch[int(py), int(px)] < 128:
                    rows.append(([v for p in t for v in p], [zr if vx > x0 else zl for vx, vy in t], [255.0] * 3, par, px, py,
                                 r[int(py), int(px)], tag))


for tag, (test, page) in enumerate((("zblock", 0), ("zblock", 2), ("ztile", 0), ("ztile", 2))):
    r, gch = chan(test, page)
    if test == "zblock":
        zl, zr = (f32(32.0), f32(0.52)) if page == 0 else (f32(1.0), f32(32.77))
        for c in range(240):
            add_quad((c % 20) * 32.0, (c // 20) * 32.0, 32.0, zl, zr, r, gch, tag, (0, 5, 11, 16, 21, 26, 28, 29, 30, 31))
    elif page == 0:
        for c in range(240):
            add_quad((c % 20) * 32.0 + 16, (c // 20) * 32.0, 16.0, f32(40.0), f32(0.52), r, gch, tag, (0, 4, 8, 11, 12, 13, 14, 15))
    else:
        for c in range(120):
            add_quad((c % 10) * 64.0, (c // 10) * 32.0, 64.0, f32(32.0), f32(0.52), r, gch, tag, (0, 16, 31, 32, 48, 56, 60, 61, 62, 63))
n_steep = len(rows)
rows += [r for i, r in enumerate(cfit_data.rows) if i % 3 == 0]

T = np.array([r[0] for r in rows]); Z = np.array([r[1] for r in rows]); C = np.array([r[2] for r in rows]); PAR = np.array([r[3] for r in rows])
X = np.array([r[4] for r in rows]); Y = np.array([r[5] for r in rows]); WANT = np.array([r[6] for r in rows]); TAG = np.array([r[7] for r in rows])
PERMS = list(itertools.permutations(range(3)))


def planes(vals, perm_e, perm_o, bits):
    """Determinant-form plane coefficients with `bits` of mantissa, truncating."""
    Rb = z3.make_rnd(bits, "trunc")
    idx = np.where(PAR[:, None] == 0, np.array(PERMS[perm_e])[None, :], np.array(PERMS[perm_o])[None, :])
    r = np.arange(len(T))
    x1, x2, x3 = (T[r, 2 * idx[:, i]] for i in range(3))
    y1, y2, y3 = (T[r, 2 * idx[:, i] + 1] for i in range(3))
    v1, v2, v3 = (vals[r, idx[:, i]] for i in range(3))
    e1 = Rb(Rb(x2 * y3) - Rb(x3 * y2)); e2 = Rb(Rb(x3 * y1) - Rb(x1 * y3)); e3 = Rb(Rb(x1 * y2) - Rb(x2 * y1))
    a = Rb(Rb(Rb(v1 * Rb(y2 - y3)) + Rb(v2 * Rb(y3 - y1))) + Rb(v3 * Rb(y1 - y2)))
    b = Rb(Rb(Rb(v1 * Rb(x3 - x2)) + Rb(v2 * Rb(x1 - x3))) + Rb(v3 * Rb(x2 - x1)))
    c = Rb(Rb(Rb(v1 * e1) + Rb(v2 * e2)) + Rb(v3 * e3))
    inv = Rb(1.0 / Rb(Rb(e1 + e2) + e3))
    return Rb(a * inv), Rb(b * inv), Rb(c * inv)


def evalp(pl, bits, order):
    Rb = z3.make_rnd(bits, "trunc")
    A, B, Cc = pl
    if order == 0: return Rb(Rb(Rb(A * X) + Rb(B * Y)) + Cc)
    if order == 1: return Rb(Rb(A * X) + Rb(Rb(B * Y) + Cc))
    return Rb(Rb(Rb(A * X) + Cc) + Rb(B * Y))


def color(P):
    Rs = z3.make_rnd(P["setup"], "trunc")
    pe, po = P["perm"]
    zp = planes(Z, pe, po, P["setup"])
    cp = planes(Rs(C * Z), pe, po, P["setup"])
    num = evalp(cp, P["eval"], P["order"])
    den = evalp(zp, P["eval"], P["order"])
    Rd = z3.make_rnd(P["div"], "trunc")
    with np.errstate(all="ignore"):
        v = Rd(num / den) if P["how"] == "div" else Rd(num * Rd(1.0 / den))
    return np.clip(np.floor(v + P["bias"]), 0, 255).astype(int)


def score(P):
    ok = color(P) == WANT
    return int(ok.sum()), [int(ok[TAG == t].sum()) for t in np.unique(TAG)]


print(len(rows), "samples:", [int((TAG == t).sum()) for t in np.unique(TAG)], "(zblock 0, zblock 2, ztile 0, ztile 2, shade)")
print("console values below 255 in the steep samples:", int((WANT[:n_steep] < 255).sum()))
base = {"setup": 24, "eval": 24, "order": 0, "div": 24, "how": "div", "bias": 0.497, "perm": (1, 5)}
print("everything at 24 bits:", score(base))
res = []
for setup in (24, 20, 16):
    for ev in (24, 22, 20, 19, 18, 17, 16, 15, 14, 13, 12):
        for dv in (24, 20, 16, 12, 10, 8):
            for how in ("div", "recip"):
                for bias in (0.497, 0.0):
                    for order in (0, 1, 2):
                        P = {"setup": setup, "eval": ev, "order": order, "div": dv, "how": how, "bias": bias, "perm": (1, 5)}
                        res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
for s, P in res[:12]:
    print(s, P)
