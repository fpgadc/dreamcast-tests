"""Color through the TSP: 16 bit plane coefficients, then how the division by 1/w is done."""
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


def recip_table(den, index_bits, out_bits, interp):
    """1/den from a table on the top mantissa bits, optionally with linear interpolation between entries."""
    m, e = np.frexp(den)                      # 0.5 <= m < 1
    n = 1 << index_bits
    pos = (m - 0.5) * 2 * n
    i = np.floor(pos)
    def entry(k):
        return np.floor((1.0 / (0.5 + k / (2.0 * n))) * (1 << out_bits)) / (1 << out_bits)     # 1/m in (1, 2]
    if interp:
        f = pos - i
        r = entry(i) + (entry(i + 1) - entry(i)) * f
        r = np.floor(r * (1 << out_bits)) / (1 << out_bits)
    else:
        r = entry(i)
    return np.ldexp(r, -e)


def score(P):
    with np.errstate(all="ignore"):
        num = ev(CP, 16, P["slope"], P["wide"])
        den = ev(ZP, 16, P["slope"], P["wide"])
        if P["how"] == "div":
            v = z3.make_rnd(P["bits"], "trunc")(num / den)
        elif P["how"] == "recip":
            v = z3.make_rnd(24, "trunc")(num * z3.make_rnd(P["bits"], "trunc")(1.0 / den))
        elif P["how"] == "recip, rounded up":
            m, e = np.frexp(1.0 / den)
            v = num * np.ldexp(np.ceil(m * (1 << P["bits"])) / (1 << P["bits"]), e)
        else:
            v = num * recip_table(den, P["index"], P["bits"], P["how"] == "table, interpolated")
    out = np.clip(np.floor(v + P["bias"]), 0, 255).astype(int)
    ok = out == WANT
    low = (WANT < 255) & (TAG < 4)
    return int(ok.sum()), [int(ok[TAG == k].sum()) for k in np.unique(TAG)], f"254s {int((ok & low).sum())}/{int(low.sum())}, false {int(((out < 255) & (WANT == 255) & (TAG < 4)).sum())}"


res = []
for slope in (16, 17, 18):
    for wide in (18, 19, 20, 21, 22):
        for bias in (0.497, 0.5):
            for how, bitsl, idxl in (("div", (24, 20, 16, 14, 12), (0,)), ("recip", (24, 20, 18, 16, 14, 12, 10), (0,)),
                                     ("recip, rounded up", (20, 16, 14, 12, 10), (0,)),
                                     ("table", (16, 12, 10, 8), (6, 7, 8, 9, 10)), ("table, interpolated", (16, 12, 10), (4, 5, 6, 7, 8))):
                for bits in bitsl:
                    for idx in idxl:
                        P = {"slope": slope, "wide": wide, "bias": bias, "how": how, "bits": bits, "index": idx}
                        res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
print(len(X), "samples", [int((TAG == k).sum()) for k in np.unique(TAG)], "(zblock 0, zblock 2, ztile 0, ztile 2, shade)")
for s, P in res[:14]:
    print(s, P)
