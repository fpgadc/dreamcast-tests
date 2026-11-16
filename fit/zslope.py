"""Slopes with their own mantissa limit as well as the tile grid: fit on the precision and zblock dumps together."""
import io
import contextlib
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zfit4 as z
import zgroup as g
with contextlib.redirect_stdout(io.StringIO()):
    import zblockfit as zb

R = g.R


def mant(v, bits, mode="trunc"):
    return z3.make_rnd(bits, "trunc")(v)


def depth(C, X, Y, P):
    A, B, Cc, _ = C
    TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32
    first = R(R(R(A * (TX + 0.5)) + R(B * (TY + 0.5))) + Cc)
    e = np.frexp(first)[1]
    unit = lambda bits: np.ldexp(1.0, e - bits)
    start = np.trunc(first / unit(P["start_bits"])) * unit(P["start_bits"])
    def slope(s):
        s = mant(s, P["slope_mant"])
        return np.trunc(s / unit(P["slope_grid"])) * unit(P["slope_grid"])
    v = start + slope(A) * (X - TX - 0.5) + slope(B) * (Y - TY - 0.5)
    return z3.make_rnd(19, "trunc")(v)


def score(P):
    da, db = depth(g.CA, g.X, g.Y, P), depth(g.CB, g.X, g.Y, P)
    pred = np.where(z.MODE == 0, db > da, np.where(z.MODE == 1, db < da, db == da))
    s1 = int((pred == z.DRAWN).sum())
    da, db = depth(zb.CA, zb.X, zb.Y, P), depth(zb.CB, zb.X, zb.Y, P)
    pred = np.where(zb.MODE == 0, db > da, db == da)
    s2 = int((pred == zb.DRAWN).sum())
    return s1 + s2, s1, s2


print("precision observations", len(g.X), "zblock observations", len(zb.X))
res = []
for sm in (14, 15, 16, 17, 18, 19, 20, 21, 22, 24):
    for sg in (24, 25, 26, 27, 28):
        for sb in (19, 20, 21):
            P = {"start_bits": sb, "slope_mant": sm, "slope_grid": sg}
            with np.errstate(all="ignore"):
                res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
for s, P in res[:12]:
    print(s, P)
