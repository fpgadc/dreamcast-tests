"""Color through the TSP with 16 bit plane constants: does it produce the console's 254s?"""
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
R24 = z3.make_rnd(24, "trunc")
ZP = t.planes(t.Z, 1, 5, 24)
CP = t.planes(R24(t.C * t.Z), 1, 5, 24)


def ev(pl, cbits, sbits, wide):
    A, B, Cc = pl
    Tc, Ts, Tw = z3.make_rnd(cbits, "trunc"), z3.make_rnd(sbits, "trunc"), z3.make_rnd(wide, "trunc")
    return Tw(Tw(Tw(Ts(A) * X) + Tw(Ts(B) * Y)) + Tc(Cc))


def score(P):
    with np.errstate(all="ignore"):
        num = ev(CP, P["num_const"], P["slope"], P["wide"])
        den = ev(ZP, P["den_const"], P["slope"], P["wide"])
        v = z3.make_rnd(P["div"], "trunc")(num / den)
    out = np.clip(np.floor(v + P["bias"]), 0, 255).astype(int)
    ok = out == WANT
    low = (WANT < 255) & (TAG < 4)
    return int(ok.sum()), [int(ok[TAG == k].sum()) for k in np.unique(TAG)], f"254s reproduced {int((ok & low).sum())} of {int(low.sum())}, wrongly lowered {int(((out < 255) & (WANT == 255) & (TAG < 4)).sum())}"


res = []
for nc in (14, 15, 16, 17, 18, 20, 24):
    for dc in (14, 15, 16, 17, 18, 20, 24):
        for slope in (16, 20, 24):
            for wide in (24, 20):
                for div in (24, 16):
                    for bias in (0.497, 0.5, 0.0):
                        P = {"num_const": nc, "den_const": dc, "slope": slope, "wide": wide, "div": div, "bias": bias}
                        res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples", [int((TAG == k).sum()) for k in np.unique(TAG)], "(zblock 0, zblock 2, ztile 0, ztile 2, shade)")
for s, P in res[:12]:
    print(s, P)
