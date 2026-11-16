"""More forms for the TSP's 1/w: short products or short slopes, on absolute or tile-relative coordinates."""
import contextlib
import io
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
with contextlib.redirect_stdout(io.StringIO()):
    import twfit as t

X, Y, TX, TY, WANT, PAGE = t.X, t.Y, t.TX, t.TY, t.WANT, t.PAGE
R24 = z3.make_rnd(24, "trunc")
A, B, C = t.planes(1, 5, 24)


def iw(P):
    Tm = z3.make_rnd(P["m"], "trunc")
    Tw = z3.make_rnd(P["wide"], "trunc")
    k = P["kind"]
    if k == "short products, absolute":
        return Tw(Tw(Tm(A * X) + Tm(B * Y)) + C)
    if k == "short slopes, absolute":
        return Tw(Tw(Tw(Tm(A) * X) + Tw(Tm(B) * Y)) + C)
    if k == "short products and constant, absolute":
        return Tw(Tw(Tm(A * X) + Tm(B * Y)) + Tm(C))
    first = R24(R24(R24(A * (TX + 0.5)) + R24(B * (TY + 0.5))) + C)
    if k == "short slopes, from the tile start":
        return Tw(first + Tm(A) * (X - TX - 0.5) + Tm(B) * (Y - TY - 0.5))
    if k == "short start and slopes, from the tile start":
        return Tw(Tm(first) + Tm(A) * (X - TX - 0.5) + Tm(B) * (Y - TY - 0.5))
    if k == "short products, from the tile start":
        return Tw(Tw(first + Tm(A * (X - TX - 0.5))) + Tm(B * (Y - TY - 0.5)))
    if k == "short start (absolute, short products), then steps":
        f2 = Tw(Tw(Tm(A * (TX + 0.5)) + Tm(B * (TY + 0.5))) + C)
        return Tw(f2 + Tm(A) * (X - TX - 0.5) + Tm(B) * (Y - TY - 0.5))
    raise ValueError(k)


def score(P):
    with np.errstate(all="ignore"):
        out = t.fog_out(iw(P))
    ok = out == WANT
    return int(ok.sum()), [int(ok[PAGE == p].sum()) for p in range(8)], int((np.abs(out - WANT) <= 1).sum())


kinds = ["short products, absolute", "short slopes, absolute", "short products and constant, absolute", "short slopes, from the tile start",
         "short start and slopes, from the tile start", "short products, from the tile start", "short start (absolute, short products), then steps"]
res = []
for k in kinds:
    for m in range(10, 25):
        for wide in (24, 22, 20, 19):
            P = {"kind": k, "m": m, "wide": wide}
            res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples per page", [int((PAGE == p).sum()) for p in range(8)])
for s, P in res[:16]:
    print(s, P)
