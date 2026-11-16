"""TSP block interpolation, also varying where the tile start and the pixel sample sit (the TSP has its own
half-pixel offset bit) and how the start value is formed."""
import contextlib
import io
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
with contextlib.redirect_stdout(io.StringIO()):
    import tspfit as t

X, Y, WANT, TAG = t.X, t.Y, t.WANT, t.TAG
TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32
PXI, PYI = np.floor(X) - TX, np.floor(Y) - TY          # integer pixel index in the tile
R24 = z3.make_rnd(24, "trunc")
PL = {}
for perm in ((1, 5), (0, 0), (0, 2)):
    PL[perm] = (t.planes(t.Z, perm[0], perm[1], 24), t.planes(R24(t.C * t.Z), perm[0], perm[1], 24))


def block(pl, P, e_from=None):
    A, B, Cc = pl
    so = P["start_at"]
    first = R24(R24(R24(A * (TX + so)) + R24(B * (TY + so))) + Cc)
    e = np.frexp(first)[1] if e_from is None else e_from
    unit = lambda b: np.ldexp(1.0, e - b)
    rnd = np.trunc if P["round"] == "trunc" else np.rint
    q = lambda v, b: rnd(v / unit(b)) * unit(b)
    start = q(first, P["start"])
    sx, sy = q(A, P["start"] + P["guard"]), q(B, P["start"] + P["guard"])
    step = P["sample_at"] - so
    return start + sx * (PXI + step) + sy * (PYI + step), e


def color(P):
    zp, cp = PL[P["perm"]]
    den, e = block(zp, P)
    num, _ = block(cp, P)
    with np.errstate(all="ignore"):
        v = num / den
    return np.clip(np.floor(v + P["bias"]), 0, 255).astype(int)


def score(P):
    ok = color(P) == WANT
    return int(ok.sum()), [int(ok[TAG == k].sum()) for k in np.unique(TAG)]


res = []
for perm in PL:
    for start in range(12, 21):
        for guard in (0, 2, 3, 4, 5, 6, 8):
            for so in (0.0, 0.5):
                for sa in (0.0, 0.5):
                    for rd in ("trunc", "nearest"):
                        for bias in (0.497, 0.5, 0.0):
                            P = {"perm": perm, "start": start, "guard": guard, "start_at": so, "sample_at": sa, "round": rd, "bias": bias}
                            res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples", [int((TAG == k).sum()) for k in np.unique(TAG)], "; console values below 255:", int((WANT[TAG < 4] < 255).sum()))
for s, P in res[:14]:
    print(s, P)
