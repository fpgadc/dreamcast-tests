"""Fit the TSP's 1/w from the fog readout (pvr_tests/tspw)."""
import itertools
import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3

D = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/tspw")
f32 = lambda v: float(np.float32(v))
PERMS = list(itertools.permutations(range(3)))
rows = []       # tri(6) z(3) parity px py want page


def green(page):
    a = np.fromfile(D / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
    return ((a >> 8) & 255).astype(int)


base = [1.03125, 1.5, 100.25, 1.03125]
unit = 1.0 / 1048576.0
for page in range(8):
    im = green(page)
    for c in range(0, 240, 3 if page >= 4 else 2):
        x, y = (c % 20) * 32.0, (c // 20) * 32.0
        if page < 6:
            z0 = base[page if page < 4 else 0] * (1.0 + (c - 120) * unit)
            z, zr = f32(z0), f32(z0 * (1.0 + 1.0 / 4096.0))
        if page == 3:
            qx, qy, w, zc = x + 12, y + 12, 8.0, (z, z, z, z)
        elif page == 4:
            qx, qy, w, zc = x, y, 32.0, (z, zr, z, zr)
        elif page == 5:
            qx, qy, w, zc = x, y, 32.0, (z, z, zr, zr)
        elif page == 6:
            qx, qy, w, zc = x, y, 32.0, (f32(1), f32(4), f32(1), f32(4))
        elif page == 7:
            qx, qy, w, zc = x, y, 32.0, (f32(32), f32(0.52), f32(32), f32(0.52))
        else:
            qx, qy, w, zc = x, y, 32.0, (z, z, z, z)
        corners = [(qx, qy), (qx + w, qy), (qx, qy + w), (qx + w, qy + w)]
        n = int(w)
        pts = [(i, j) for i in range(0, n, 5 if n > 8 else 3) for j in range(1, n, 7 if n > 8 else 3)]
        for idx, par in (((0, 1, 2), 0), ((1, 2, 3), 1)):
            t = tuple(corners[i] for i in idx)
            for i, j in pts:
                px, py = qx + i + 0.5, qy + j + 0.5
                if z3.inside(t, px, py):
                    rows.append(([v for p in t for v in p], [zc[k] for k in idx], par, px, py, im[int(py), int(px)], page))
T = np.array([r[0] for r in rows]); Z = np.array([r[1] for r in rows]); PAR = np.array([r[2] for r in rows])
X = np.array([r[3] for r in rows]); Y = np.array([r[4] for r in rows]); WANT = np.array([r[5] for r in rows]); PAGE = np.array([r[6] for r in rows])
TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32


def planes(perm_e, perm_o, bits):
    Rb = z3.make_rnd(bits, "trunc")
    idx = np.where(PAR[:, None] == 0, np.array(PERMS[perm_e])[None, :], np.array(PERMS[perm_o])[None, :])
    r = np.arange(len(T))
    x1, x2, x3 = (T[r, 2 * idx[:, i]] for i in range(3))
    y1, y2, y3 = (T[r, 2 * idx[:, i] + 1] for i in range(3))
    v1, v2, v3 = (Z[r, idx[:, i]] for i in range(3))
    e1 = Rb(Rb(x2 * y3) - Rb(x3 * y2)); e2 = Rb(Rb(x3 * y1) - Rb(x1 * y3)); e3 = Rb(Rb(x1 * y2) - Rb(x2 * y1))
    a = Rb(Rb(Rb(v1 * Rb(y2 - y3)) + Rb(v2 * Rb(y3 - y1))) + Rb(v3 * Rb(y1 - y2)))
    b = Rb(Rb(Rb(v1 * Rb(x3 - x2)) + Rb(v2 * Rb(x1 - x3))) + Rb(v3 * Rb(x2 - x1)))
    c = Rb(Rb(Rb(v1 * e1) + Rb(v2 * e2)) + Rb(v3 * e3))
    inv = Rb(1.0 / Rb(Rb(e1 + e2) + e3))
    return Rb(a * inv), Rb(b * inv), Rb(c * inv)


def fog_out(iw):
    w = np.clip(iw, 1.0, 255.999985).astype(np.float32)
    bits = w.view(np.uint32)
    bf = (bits >> 11) & 255
    alpha = (255 * bf) >> 8
    return ((255 * alpha) >> 8).astype(int)


PL = {}


def iw(P):
    key = (P["perm"], P["setup"])
    if key not in PL:
        PL[key] = planes(P["perm"][0], P["perm"][1], P["setup"])
    A, B, C = PL[key]
    if P["kind"] == "float":
        Rb = z3.make_rnd(P["bits"], "trunc")
        if P["order"] == 0: return Rb(Rb(Rb(A * X) + Rb(B * Y)) + C)
        if P["order"] == 1: return Rb(Rb(A * X) + Rb(Rb(B * Y) + C))
        return Rb(Rb(Rb(A * X) + C) + Rb(B * Y))
    # block: per tile start and steps on the start's exponent
    R24 = z3.make_rnd(24, "trunc")
    first = R24(R24(R24(A * (TX + 0.5)) + R24(B * (TY + 0.5))) + C)
    e = np.frexp(first)[1]
    u = lambda b: np.ldexp(1.0, e - b)
    q = lambda v, b: np.trunc(v / u(b)) * u(b)
    start = q(first, P["bits"])
    sx, sy = q(A, P["bits"] + P["order"] * 3), q(B, P["bits"] + P["order"] * 3)
    return start + sx * (X - TX - 0.5) + sy * (Y - TY - 0.5)


def score(P):
    with np.errstate(all="ignore"):
        out = fog_out(iw(P))
    ok = out == WANT
    near = np.abs(out - WANT) <= 1
    return int(ok.sum()), [int(ok[PAGE == p].sum()) for p in range(8)], int(near.sum())


print(len(rows), "samples per page", [int((PAGE == p).sum()) for p in range(8)])
res = []
for perm in ((1, 5), (0, 0), (0, 2), (1, 3), (4, 5), (4, 3)):
    for setup in (24, 20):
        for kind, bitsl, orders in (("float", range(12, 25), (0, 1, 2)), ("block", range(12, 25), (0, 1, 2, 3))):
            for bits in bitsl:
                for order in orders:
                    P = {"perm": perm, "setup": setup, "kind": kind, "bits": bits, "order": order}
                    res.append((score(P), P))
res.sort(key=lambda r: -r[0][0])
for s, P in res[:14]:
    print(s, P)
