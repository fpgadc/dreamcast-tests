"""Second look at the shared-exponent idea: a tile start value and per-pixel slopes on one exponent's grid."""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zfit4 as z
import zgroup as g

R, X, Y, TX, TY = g.R, g.X, g.Y, g.TX, g.TY
quant, expo = g.quant, g.expo


def depth(C, P):
    A, B, Cc, zmax = C
    ox = 0.5 if P["start"] == "first pixel center" else 0.0
    start_f = R(R(R(A * (TX + ox)) + R(B * (TY + ox))) + Cc)           # float value at the tile's start
    ref = {"start": start_f, "C": Cc, "zmax": zmax}[P["ref"]]
    e = expo(ref)
    start = quant(start_f, e, P["start_bits"])
    ax, by = quant(A, e, P["slope_bits"]), quant(B, e, P["slope_bits"])
    nx, ny = X - TX - ox, Y - TY - ox             # 0.5-based or integer pixel steps
    if P["start"] != "first pixel center":
        # the half pixel as an extra half step
        nx, ny = X - TX, Y - TY
    if P["acc"] == "direct":
        v = start + ax * nx + by * ny
    else:                                          # rows first, each partial sum kept on the accumulator's grid
        v = quant(start + by * ny, e, P["acc_bits"])
        v = quant(v + ax * nx, e, P["acc_bits"])
    if P["cut"] == "group":
        return quant(v, e, P["bits"])
    return z3.make_rnd(P["bits"], "trunc")(v)      # cut relative to the value's own exponent


def score(P):
    da, db = depth(g.CA, P), depth(g.CB, P)
    pred = np.where(z.MODE == 0, db > da, np.where(z.MODE == 1, db < da, db == da))
    ok = pred == z.DRAWN
    return int(ok.sum()), {int(t): int(ok[z.TAG == t].sum()) for t in np.unique(z.TAG)}


OPT = {"ref": ["start", "C", "zmax"], "start": ["first pixel center", "tile corner"], "start_bits": [19, 20, 21, 22, 24, 28],
       "slope_bits": [19, 21, 23, 24, 26, 28, 32], "acc": ["direct", "seq"], "acc_bits": [24, 28], "cut": ["group", "own"], "bits": [19]}
keys = list(OPT)
res = []
for combo in itertools.product(*[OPT[k] for k in keys]):
    P = dict(zip(keys, combo))
    if P["acc"] == "direct" and P["acc_bits"] != 24:
        continue
    with np.errstate(all="ignore"):
        s, by = score(P)
    res.append((s, P, by))
res.sort(key=lambda r: -r[0])
print("best of", len(res))
for s, P, by in res[:14]:
    print(s, P, {k: v for k, v in by.items() if k in (0, 1, 2, 3)})
