"""Fit the block-exponent depth interpolation on the zblock dumps (steep rising and falling planes)."""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zgroup as g

D = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/zblock")
f32 = lambda v: float(np.float32(v))
R = g.R


def green(page):
    a = np.fromfile(D / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
    return ((a >> 8) & 255) > 128


rows = []       # triA(6) zA(3) parA triB(6) zB(3) parB mode drawn px py page
for page in range(6):
    gp = green(page)
    falling = page < 2 or page >= 4
    zl, zr = (f32(32.0), f32(0.52)) if falling else (f32(1.0), f32(32.77))
    center = zl + (zr - zl) * (31.5 / 32.0)
    step = 1.0 / 262144.0 if page < 2 else 1.0 / 16384.0 if page < 4 else 1.0 / 8192.0
    for c in range(240):
        x, y = (c % 20) * 32.0, (c // 20) * 32.0
        zb = f32(center + (c - 120) * step)
        A = z3.strip_tris(x, y, 32, 32)
        B = z3.strip_tris(x + 28, y, 4, 32)
        for dx in (28, 29, 30, 31):
            for dy in (2, 8, 14):
                px, py = x + dx + 0.5, y + dy + 0.5
                ta = next((t for t in A if z3.inside(t[0], px, py)), None)
                tb = next((t for t in B if z3.inside(t[0], px, py)), None)
                if ta is None or tb is None:
                    continue
                za = [zr if vx > x else zl for vx, vy in ta[0]]
                rows.append(([c_ for v in ta[0] for c_ in v], za, ta[1], [c_ for v in tb[0] for c_ in v], [zb] * 3, tb[1],
                             2 if page & 1 else 0, bool(gp[int(py), int(px)]), px, py, page))
TA = np.array([r[0] for r in rows]); ZA = np.array([r[1] for r in rows]); PA = np.array([r[2] for r in rows])
TB = np.array([r[3] for r in rows]); ZB = np.array([r[4] for r in rows]); PB = np.array([r[5] for r in rows])
MODE = np.array([r[6] for r in rows]); DRAWN = np.array([r[7] for r in rows]); X = np.array([r[8] for r in rows]); Y = np.array([r[9] for r in rows])
PAGE = np.array([r[10] for r in rows])
TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32
CA, CB = g.coefs(TA, ZA, PA), g.coefs(TB, ZB, PB)


def q(v, e, bits, mode):
    unit = np.ldexp(1.0, e - bits)
    return (np.trunc(v / unit) if mode == "trunc" else np.floor(v / unit) if mode == "floor" else np.ceil(v / unit)) * unit


def depth(C, P):
    A, B, Cc, _ = C
    first = R(R(R(A * (TX + 0.5)) + R(B * (TY + 0.5))) + Cc)
    if P["ref"] == "start":
        e = np.frexp(first)[1]
    elif P["ref"] == "C":
        e = np.frexp(Cc)[1]
    else:                               # the largest of the tile's four corner values
        cs = [np.abs(R(R(R(A * (TX + i)) + R(B * (TY + j))) + Cc)) for i in (0.5, 31.5) for j in (0.5, 31.5)]
        e = np.frexp(np.maximum.reduce(cs))[1]
    start = q(first, e, P["start_bits"], "trunc")
    ax, by = q(A, e, P["slope_bits"], P["slope_mode"]), q(B, e, P["slope_bits"], P["slope_mode"])
    v = start + ax * (X - TX - 0.5) + by * (Y - TY - 0.5)
    return z3.make_rnd(P["cut"], "trunc")(v)


def score(P):
    da, db = depth(CA, P), depth(CB, P)
    pred = np.where(MODE == 0, db > da, db == da)
    ok = pred == DRAWN
    return int(ok.sum()), {int(p): int(ok[PAGE == p].sum()) for p in range(6)}


OPT = {"ref": ["start", "C", "max corner"], "start_bits": [19, 20, 21, 22, 24], "slope_bits": [22, 23, 24, 25, 26, 27, 28, 30],
       "slope_mode": ["trunc", "floor", "ceil"], "cut": [19, 20, 21, 23]}
keys = list(OPT)
res = []
for combo in itertools.product(*[OPT[k] for k in keys]):
    P = dict(zip(keys, combo))
    with np.errstate(all="ignore"):
        s, by = score(P)
    res.append((s, P, by))
res.sort(key=lambda r: -r[0])
print(len(rows), "observations; per page", {int(p): int((PAGE == p).sum()) for p in range(6)})
cur = {"ref": "start", "start_bits": 20, "slope_bits": 26, "slope_mode": "trunc", "cut": 19}
print("current model:", score(cur))
for s, P, by in res[:12]:
    print(s, P, by)
