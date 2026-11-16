"""TSP 1/w: 16 bit constant, then the widths and rounding direction of the products and sums."""
import contextlib
import io
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
with contextlib.redirect_stdout(io.StringIO()):
    import twfit as t

X, Y, WANT, PAGE = t.X, t.Y, t.WANT, t.PAGE
A, B, C = t.planes(1, 5, 24)


def rnd(v, bits, mode):
    if bits >= 53:
        return v
    m, e = np.frexp(v)
    f = {"trunc": np.trunc, "floor": np.floor, "nearest": np.rint, "ceil": np.ceil}[mode]
    return np.ldexp(f(m * (1 << bits)) / (1 << bits), e)


def iw(P):
    a, b = rnd(A, P["slope"], P["smode"]), rnd(B, P["slope"], P["smode"])
    c = rnd(C, P["const"], "trunc")
    ax, by = rnd(a * X, P["prod"], P["pmode"]), rnd(b * Y, P["prod"], P["pmode"])
    if P["order"] == 0: v = rnd(rnd(ax + by, P["sum"], P["umode"]) + c, P["sum"], P["umode"])
    elif P["order"] == 1: v = rnd(ax + rnd(by + c, P["sum"], P["umode"]), P["sum"], P["umode"])
    else: v = rnd(rnd(ax + c, P["sum"], P["umode"]) + by, P["sum"], P["umode"])
    return v


def score(P):
    with np.errstate(all="ignore"):
        out = t.fog_out(iw(P))
    ok = out == WANT
    return int(ok.sum()), [int(ok[PAGE == p].sum()) for p in (4, 5, 6, 7)], int((np.abs(out - WANT) <= 1).sum())


res = []
for slope in (16, 18, 20, 24):
    for smode in ("trunc", "floor"):
        for prod in (16, 17, 18, 19, 20, 21, 22, 24, 53):
            for pmode in ("trunc", "floor"):
                for s in (16, 17, 18, 19, 20, 22, 24, 53):
                    for umode in ("trunc", "floor"):
                        for order in (0, 1, 2):
                            P = {"const": 16, "slope": slope, "smode": smode, "prod": prod, "pmode": pmode, "sum": s, "umode": umode, "order": order}
                            res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples; pages 4, 5, 6, 7 have", [int((PAGE == p).sum()) for p in (4, 5, 6, 7)])
for s, P in res[:14]:
    print(s, P)
