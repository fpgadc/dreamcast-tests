"""TSP attribute interpolation as block floating point per tile, with its own (shorter) widths."""
import contextlib
import io
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
with contextlib.redirect_stdout(io.StringIO()):
    import tspfit as t

X, Y, WANT, TAG = t.X, t.Y, t.WANT, t.TAG
TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32
R24 = z3.make_rnd(24, "trunc")
ZP = t.planes(t.Z, 1, 5, 24)
CP = t.planes(R24(t.C * t.Z), 1, 5, 24)


def block(pl, P, e_from=None):
    A, B, Cc = pl
    first = R24(R24(R24(A * (TX + 0.5)) + R24(B * (TY + 0.5))) + Cc)
    e = np.frexp(first)[1] if e_from is None else e_from
    unit = lambda b: np.ldexp(1.0, e - b)
    q = lambda v, b: np.trunc(v / unit(b)) * unit(b)
    start = q(first, P["start"])
    sx, sy = q(A, P["start"] + P["guard"]), q(B, P["start"] + P["guard"])
    return start + sx * (X - TX - 0.5) + sy * (Y - TY - 0.5), e


def color(P):
    den, e = block(ZP, P)
    num, _ = block(CP, P, (e + 8) if P["exp"] == "depth's + 8" else None)
    if P["cut"]:
        den = z3.make_rnd(P["cut"], "trunc")(den); num = z3.make_rnd(P["cut"], "trunc")(num)
    with np.errstate(all="ignore"):
        Rd = z3.make_rnd(P["div"], "trunc")
        v = Rd(num / den) if P["how"] == "div" else Rd(num * Rd(1.0 / den))
    return np.clip(np.floor(v + P["bias"]), 0, 255).astype(int)


def score(P):
    ok = color(P) == WANT
    return int(ok.sum()), [int(ok[TAG == k].sum()) for k in np.unique(TAG)]


res = []
for start in range(10, 25):
    for guard in (0, 2, 4, 5, 6, 8, 10):
        for cut in (0, 12, 14, 16, 19):
            for div in (24, 16, 12):
                for how in ("div", "recip"):
                    for ex in ("own", "depth's + 8"):
                        for bias in (0.497, 0.0):
                            P = {"start": start, "guard": guard, "cut": cut, "div": div, "how": how, "exp": ex, "bias": bias}
                            res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples", [int((TAG == k).sum()) for k in np.unique(TAG)], "; console values below 255:", int((WANT[TAG < 4] < 255).sum()))
for s, P in res[:14]:
    print(s, P)
