"""Does the console's depth use a shared exponent (block floating point) rather than a per-value 19 bit cut?

Uses the observations of zfit4 (flat polygons, pages 0-2 and 6-21, and sloped planes, pages 3-4). The plane
coefficients A, B, C come from the fitted setup arithmetic; what varies here is how a pixel's depth is formed
from them and how it is quantized.
"""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zfit4 as z

R = z3.make_rnd(24, "trunc")
PERMS = z.PERMS


def coefs(T, Z, parity):
    pe, po = PERMS[1], PERMS[5]
    idx = np.where(parity[:, None] == 0, np.array(pe)[None, :], np.array(po)[None, :])
    r = np.arange(len(T))
    x1, x2, x3 = (T[r, 2 * idx[:, i]] for i in range(3))
    y1, y2, y3 = (T[r, 2 * idx[:, i] + 1] for i in range(3))
    z1, z2, z3_ = (Z[r, idx[:, i]] for i in range(3))
    e1 = R(R(x2 * y3) - R(x3 * y2)); e2 = R(R(x3 * y1) - R(x1 * y3)); e3 = R(R(x1 * y2) - R(x2 * y1))
    a = R(R(R(z1 * R(y2 - y3)) + R(z2 * R(y3 - y1))) + R(z3_ * R(y1 - y2)))
    b = R(R(R(z1 * R(x3 - x2)) + R(z2 * R(x1 - x3))) + R(z3_ * R(x2 - x1)))
    c = R(R(R(z1 * e1) + R(z2 * e2)) + R(z3_ * e3))
    det = R(R(e1 + e2) + e3)
    inv = R(1.0 / det)
    return R(a * inv), R(b * inv), R(c * inv), np.max(np.abs(Z), axis=1)


CA = coefs(z.TA, z.ZA, z.PA)
CB = coefs(z.TB, z.ZB, z.PB)
X, Y = z.PX, z.PY
TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32


def expo(v):
    return np.frexp(v)[1]              # v = m * 2^e, 0.5 <= m < 1


def quant(v, e, bits):
    """Truncate v to a grid whose step is 2^(e - bits): `bits` bits below the exponent e."""
    step = np.ldexp(1.0, e - bits)
    return np.trunc(v / step) * step


def depth(C, P, other=None):
    A, B, Cc, zmax = C
    # the value the exponent is taken from
    if P["ref"] == "self":
        ref = None
    elif P["ref"] == "C":
        ref = Cc
    elif P["ref"] == "zmax":
        ref = zmax
    elif P["ref"] == "tile origin":
        ref = R(R(R(A * TX) + R(B * TY)) + Cc)
    elif P["ref"] == "tile far corner":
        ref = np.maximum(np.abs(R(R(R(A * TX) + R(B * TY)) + Cc)), np.abs(R(R(R(A * (TX + 32)) + R(B * (TY + 32))) + Cc)))
    elif P["ref"] == "pair zmax":
        ref = np.maximum(zmax, other[3])
    bits = P["bits"]
    if P["eval"] == "float":
        v = R(R(R(A * X) + R(B * Y)) + Cc)
    elif P["eval"] == "terms":           # each term put on the grid before the sum
        e = expo(ref)
        v = quant(A * X, e, bits) + quant(B * Y, e, bits) + quant(Cc, e, bits)
        return v
    elif P["eval"] == "steps":           # start of the tile on the grid, then whole grid steps per pixel
        e = expo(ref)
        start = quant(R(R(R(A * (TX + 0.5)) + R(B * (TY + 0.5))) + Cc), e, bits)
        ax, by = quant(A, e, bits + P["extra"]), quant(B, e, bits + P["extra"])
        v = start + ax * (X - TX - 0.5) + by * (Y - TY - 0.5)
        return quant(v, e, bits)
    elif P["eval"] == "steps exact":     # as above, slopes not put on the grid
        e = expo(ref)
        start = quant(R(R(R(A * (TX + 0.5)) + R(B * (TY + 0.5))) + Cc), e, bits)
        v = start + A * (X - TX - 0.5) + B * (Y - TY - 0.5)
        return quant(v, e, bits)
    if ref is None:
        return z3.make_rnd(bits, "trunc")(v)
    return quant(v, expo(ref), bits)


def score(P):
    da, db = depth(CA, P, CB), depth(CB, P, CA)
    pred = np.where(z.MODE == 0, db > da, np.where(z.MODE == 1, db < da, db == da))
    ok = pred == z.DRAWN
    return int(ok.sum()), {int(t): int(ok[z.TAG == t].sum()) for t in np.unique(z.TAG)}


print(len(X), "observations; per page totals:", {int(t): int((z.TAG == t).sum()) for t in np.unique(z.TAG)})
res = []
for ref in ("self", "C", "zmax", "tile origin", "tile far corner", "pair zmax"):
    for ev in ("float", "terms", "steps", "steps exact"):
        if ref == "self" and ev != "float":
            continue
        for bits in (18, 19, 20):
            for extra in ((0, 4, 8) if ev == "steps" else (0,)):
                P = {"ref": ref, "eval": ev, "bits": bits, "extra": extra}
                with np.errstate(all="ignore"):
                    s, by = score(P)
                res.append((s, P, by))
res.sort(key=lambda r: -r[0])
for s, P, by in res[:16]:
    print(s, P, by)
