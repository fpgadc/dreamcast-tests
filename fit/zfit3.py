"""Coordinate-descent search over triangle-setup arithmetic, against hardware depth outcomes (pvr_tests/precision)."""
import itertools
import struct
import sys
from pathlib import Path

import numpy as np

D = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/precision")
MANT = [0x000000, 0x7fffff, 0x400000, 0x000001, 0x2aaaaa, 0x555555, 0x00ffff, 0x7f0000]
BASES = [(127 << 23) | m for m in MANT] + [(117 << 23) | m for m in (0, 0x7fffff, 0x555555)] + [(137 << 23) | m for m in (0, 0x7fffff, 0x555555)]


def f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits & 0xffffffff))[0]


def green(page):
    a = np.fromfile(D / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
    return ((a >> 8) & 255) > 128


def strip_tris(x, y, w, h, order=(0, 1, 2, 3)):
    c = [(x, y), (x + w, y), (x, y + h), (x + w, y + h)]
    p = [c[i] for i in order]
    return [((p[0], p[1], p[2]), 0), ((p[1], p[2], p[3]), 1)]       # (triangle in strip order, parity)


def inside(tri, px, py):
    (x1, y1), (x2, y2), (x3, y3) = tri
    e = lambda ax, ay, bx, by: (bx - ax) * (py - ay) - (by - ay) * (px - ax)
    s = [e(x1, y1, x2, y2), e(x2, y2, x3, y3), e(x3, y3, x1, y1)]
    return all(v > 0 for v in s) or all(v < 0 for v in s)


def observations():
    obs = []
    g = {p: green(p) for p in (0, 1, 2)}
    def add(A, za, B, zb, mode, gp, pts, tag):
        for px, py in pts:
            ta = next((t for t in A if inside(t[0], px, py)), None)
            tb = next((t for t in B if inside(t[0], px, py)), None)
            if ta and tb:
                obs.append((ta, za, tb, zb, mode, bool(gp[int(py), int(px)]), px, py, tag))
    for page in (0, 1, 2):
        for r in range(14):
            for c in range(20):
                x, y = c * 32.0, r * 32.0
                if page == 0:
                    b = BASES[r] + (1 << c) if c < 10 else BASES[r] - (1 << (c - 10))
                    mode = 0 if c < 10 else 1
                else:
                    b = BASES[r] + (c - 10)
                    mode = 2
                A = strip_tris(x, y, 32, 32)
                B = strip_tris(x, y, 32, 32) if page == 2 else strip_tris(x + 4, y + 4, 24, 24)
                add(A, f32(BASES[r]), B, f32(b), mode, g[page], [(x + 10.5, y + 20.5), (x + 20.5, y + 10.5)], page)
    for page in range(6, 22):
        gp = green(page)
        less = page >= 14
        d = 1 << (page - 14 if less else page - 6)
        zb = f32((127 << 23) - d if less else (127 << 23) + d)
        for r in range(15):
            for c in range(20):
                s = 8.0 if c & 1 else 32.0
                x, y = c * 32.0 + (32 - s) / 2, r * 32.0 + (32 - s) / 2
                pts = [(int(x + fx * s) + 0.5, int(y + fy * s) + 0.5) for fx, fy in ((0.5, 0.2), (0.8, 0.5), (0.5, 0.8), (0.2, 0.5))]
                add(strip_tris(x, y, s, s), 1.0, strip_tris(x, y, s, s, order=(1, 3, 0, 2)), zb, 1 if less else 0, gp, pts, 6)
    return obs


obs = observations()
N = len(obs)
def tri_arr(k):
    return np.array([[c for v in o[k][0] for c in v] for o in obs], dtype=np.float64), np.array([o[k][1] for o in obs])
TA, PA = tri_arr(0)
TB, PB = tri_arr(2)
ZA = np.array([o[1] for o in obs]); ZB = np.array([o[3] for o in obs])
MODE = np.array([o[4] for o in obs]); DRAWN = np.array([o[5] for o in obs])
PX = np.array([o[6] for o in obs]); PY = np.array([o[7] for o in obs]); TAG = np.array([o[8] for o in obs])
PERMS = list(itertools.permutations(range(3)))


def make_rnd(bits, mode):
    if bits >= 53:
        return lambda v: v
    f = np.trunc if mode == "trunc" else np.rint
    sc = float(1 << bits)
    def rnd(v):
        m, e = np.frexp(v)
        return np.ldexp(f(m * sc) / sc, e)
    return rnd


def depth(P, T, Z, parity):
    Rm = make_rnd(P["bits"] if not P["mul_exact"] else 53, P["mode"])
    Ra = make_rnd(P["bits"], P["mode"])
    Rs = make_rnd(P["store"], "trunc")
    pe, po = PERMS[P["perm_even"]], PERMS[P["perm_odd"]]
    idx = np.where(parity[:, None] == 0, np.array(pe)[None, :], np.array(po)[None, :])     # N x 3
    rows = np.arange(len(T))
    X = np.stack([T[rows, 2 * idx[:, i]] for i in range(3)], axis=1)
    Y = np.stack([T[rows, 2 * idx[:, i] + 1] for i in range(3)], axis=1)
    if P["origin"] == "tile":
        ox, oy = np.floor(PX / 32) * 32, np.floor(PY / 32) * 32
    else:
        ox = oy = 0.0
    x1, x2, x3 = X[:, 0] - ox, X[:, 1] - ox, X[:, 2] - ox
    y1, y2, y3 = Y[:, 0] - oy, Y[:, 1] - oy, Y[:, 2] - oy
    off = 0.5 if P["at"] == "corner" else 0.0
    x, y = PX - ox - off, PY - oy - off
    z1 = z2 = z3 = Z
    def sum3(t1, t2, t3):
        if P["sum"] == 0: return Ra(Ra(t1 + t2) + t3)
        if P["sum"] == 1: return Ra(t1 + Ra(t2 + t3))
        return Ra(Ra(t1 + t3) + t2)
    e1 = Ra(Rm(x2 * y3) - Rm(x3 * y2))
    e2 = Ra(Rm(x3 * y1) - Rm(x1 * y3))
    e3 = Ra(Rm(x1 * y2) - Rm(x2 * y1))
    a = sum3(Rm(z1 * Ra(y2 - y3)), Rm(z2 * Ra(y3 - y1)), Rm(z3 * Ra(y1 - y2)))
    b = sum3(Rm(z1 * Ra(x3 - x2)), Rm(z2 * Ra(x1 - x3)), Rm(z3 * Ra(x2 - x1)))
    c = sum3(Rm(z1 * e1), Rm(z2 * e2), Rm(z3 * e3))
    if P["det"] == "edges":
        det = sum3(e1, e2, e3)
    else:
        det = Ra(Rm(Ra(x2 - x1) * Ra(y3 - y1)) - Rm(Ra(x3 - x1) * Ra(y2 - y1)))
    with np.errstate(all="ignore"):
        if P["div"] == "recip":
            inv = Ra(1.0 / det)
            q = lambda n: Rm(n * inv)
        else:
            q = lambda n: Ra(n / det)
        def ev(a_, b_, c_):
            ax, by = Rm(a_ * x), Rm(b_ * y)
            if P["eval"] == 0: return Ra(Ra(ax + by) + c_)
            if P["eval"] == 1: return Ra(ax + Ra(by + c_))
            return Ra(Ra(ax + c_) + by)
        if P["form"] == "coef":
            z = ev(q(a), q(b), q(c))
        else:
            z = q(ev(a, b, c))
    return Rs(z)


def score(P, detail=False):
    da, db = depth(P, TA, ZA, PA), depth(P, TB, ZB, PB)
    pred = np.where(MODE == 0, db > da, np.where(MODE == 1, db < da, db == da))
    ok = pred == DRAWN
    if detail:
        return {int(t): (int(ok[TAG == t].sum()), int((TAG == t).sum())) for t in np.unique(TAG)}
    return int(ok.sum())


OPTIONS = {
    "bits": [24, 23, 22, 21, 20, 19, 18, 26, 28, 32],
    "mul_exact": [False, True],
    "mode": ["trunc", "nearest"],
    "store": [53, 24, 23, 22, 21, 20, 19, 18],
    "origin": ["screen", "tile"],
    "at": ["center", "corner"],
    "perm_even": list(range(6)),
    "perm_odd": list(range(6)),
    "sum": [0, 1, 2],
    "det": ["edges", "diff"],
    "div": ["div", "recip"],
    "eval": [0, 1, 2],
    "form": ["coef", "num"],
}


def descend(P):
    best = score(P)
    improved = True
    while improved:
        improved = False
        for k, opts in OPTIONS.items():
            for o in opts:
                if o == P[k]:
                    continue
                Q = dict(P); Q[k] = o
                s = score(Q)
                if s > best:
                    best, P, improved = s, Q, True
        # the two vertex orders together
        for pe in range(6):
            for po in range(6):
                Q = dict(P); Q["perm_even"] = pe; Q["perm_odd"] = po
                s = score(Q)
                if s > best:
                    best, P, improved = s, Q, True
    return best, P


if __name__ == "__main__":
    print(N, "observations,", int(DRAWN.sum()), "drawn")
    rng = np.random.default_rng(1)
    results = []
    starts = [{"bits": 24, "mul_exact": False, "mode": "trunc", "store": 53, "origin": "screen", "at": "center", "perm_even": 5,
               "perm_odd": 5, "sum": 0, "det": "edges", "div": "div", "eval": 0, "form": "coef"}]
    for _ in range(int(sys.argv[1]) if len(sys.argv) > 1 else 6):
        starts.append({k: v[rng.integers(len(v))] for k, v in OPTIONS.items()})
    for P in starts:
        s, Q = descend(P)
        results.append((s, Q))
        print(s, Q, score(Q, True))
        sys.stdout.flush()
