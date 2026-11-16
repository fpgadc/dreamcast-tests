"""TSP 1/w with the plane's coefficients held as short floats (absolute screen coordinates)."""
import contextlib
import io
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
with contextlib.redirect_stdout(io.StringIO()):
    import twfit as t

X, Y, TX, TY, WANT, PAGE = t.X, t.Y, t.TX, t.TY, t.WANT, t.PAGE
PL = {p: t.planes(p[0], p[1], 24) for p in ((1, 5), (0, 0))}


def iw(P):
    A, B, C = PL[P["perm"]]
    Ta, Tc, Tw = z3.make_rnd(P["slope"], "trunc"), z3.make_rnd(P["const"], "trunc"), z3.make_rnd(P["wide"], "trunc")
    rnd = (lambda v, b: z3.make_rnd(b, "nearest")(v)) if P["round"] == "nearest" else (lambda v, b: z3.make_rnd(b, "trunc")(v))
    a, b, c = rnd(A, P["slope"]), rnd(B, P["slope"]), rnd(C, P["const"])
    off = P["at"]
    x, y = X - 0.5 + off, Y - 0.5 + off
    if P["order"] == 0: return Tw(Tw(Tw(a * x) + Tw(b * y)) + c)
    if P["order"] == 1: return Tw(Tw(a * x) + Tw(Tw(b * y) + c))
    return Tw(Tw(Tw(a * x) + c) + Tw(b * y))


def score(P):
    with np.errstate(all="ignore"):
        out = t.fog_out(iw(P))
    ok = out == WANT
    return int(ok.sum()), [int(ok[PAGE == p].sum()) for p in range(8)], int((np.abs(out - WANT) <= 1).sum())


res = []
for perm in PL:
    for const in (14, 15, 16, 17, 18):
        for slope in (14, 15, 16, 17, 18, 20, 24):
            for wide in (24, 28, 32, 20):
                for order in (0, 1, 2):
                    for rd in ("trunc", "nearest"):
                        P = {"perm": perm, "const": const, "slope": slope, "wide": wide, "order": order, "round": rd, "at": 0.5}
                        res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples per page", [int((PAGE == p).sum()) for p in range(8)])
for s, P in res[:14]:
    print(s, P)
