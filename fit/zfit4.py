"""As zfit3, with per-vertex depths: adds the sloped planes of precision pages 3 and 4."""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3

f32 = np.float32
obs = []          # (triA xy[3], zA[3], parityA, triB, zB[3], parityB, mode, drawn, px, py, tag)
for o in z3.obs:
    (ta, pa), za, (tb, pb), zb, mode, drawn, px, py, tag = o
    obs.append((ta, (za,) * 3, pa, tb, (zb,) * 3, pb, mode, drawn, px, py, tag))


def corner_z(tri, x, y, zl, zr, vertical):
    return tuple((zr if ((vy > y) if vertical else (vx > x)) else zl) for vx, vy in tri)


def add_slope(page, cell_r, cell_c, z0, k, vertical, gp, tag):
    x, y = cell_c * 32.0, cell_r * 32.0
    step = 1.0 / float(1 << k)
    zl, zr, zb = float(f32(z0)), float(f32(float(f32(z0)) * (1.0 + step))), float(f32(float(f32(z0)) * (1.0 + step * 0.5)))
    tris = z3.strip_tris(x, y, 32, 32)
    for dx in range(2, 32, 4):
        for dy in range(1, 32, 6):
            px, py = x + dx + 0.5, y + dy + 0.5
            t = next((t for t in tris if z3.inside(t[0], px, py)), None)
            if t is None:
                continue
            obs.append((t[0], corner_z(t[0], x, y, zl, zr, vertical), t[1], t[0], (zb,) * 3, t[1], 0, bool(gp[int(py), int(px)]), px, py, tag))


g3, g4 = z3.green(3), z3.green(4)
Z0 = [1.0, 1.5, 1000.0, 0.001, 1.9999999]
for r in range(10):
    for c in range(20):
        add_slope(3, r, c, Z0[r % 5], c + 1, r >= 5, g3, 3)
for r in range(15):
    for c in range(20):
        add_slope(4, r, c, 1.0, r + 6, False, g4, 4)

N = len(obs)
TA = np.array([[c for v in o[0] for c in v] for o in obs]); ZA = np.array([o[1] for o in obs]); PA = np.array([o[2] for o in obs])
TB = np.array([[c for v in o[3] for c in v] for o in obs]); ZB = np.array([o[4] for o in obs]); PB = np.array([o[5] for o in obs])
MODE = np.array([o[6] for o in obs]); DRAWN = np.array([o[7] for o in obs])
PX = np.array([o[8] for o in obs]); PY = np.array([o[9] for o in obs]); TAG = np.array([o[10] for o in obs])
PERMS = list(itertools.permutations(range(3)))


def depth(P, T, Z, parity):
    Rm = z3.make_rnd(P["bits"], "trunc")
    Ra = Rm
    Rs = z3.make_rnd(P["store"], "trunc")
    pe, po = PERMS[P["perm_even"]], PERMS[P["perm_odd"]]
    idx = np.where(parity[:, None] == 0, np.array(pe)[None, :], np.array(po)[None, :])
    rows = np.arange(len(T))
    x1, x2, x3 = (T[rows, 2 * idx[:, i]] for i in range(3))
    y1, y2, y3 = (T[rows, 2 * idx[:, i] + 1] for i in range(3))
    z1, z2, z3_ = (Z[rows, idx[:, i]] for i in range(3))
    off = {"center": 0.0, "corner": 0.5}[P["at"]]
    x, y = PX - off, PY - off
    def sum3(t1, t2, t3):
        if P["sum"] == 0: return Ra(Ra(t1 + t2) + t3)
        if P["sum"] == 1: return Ra(t1 + Ra(t2 + t3))
        return Ra(Ra(t1 + t3) + t2)
    e1 = Ra(Rm(x2 * y3) - Rm(x3 * y2))
    e2 = Ra(Rm(x3 * y1) - Rm(x1 * y3))
    e3 = Ra(Rm(x1 * y2) - Rm(x2 * y1))
    if P["ab"] == "zdy":       # z_i times coordinate differences
        a = sum3(Rm(z1 * Ra(y2 - y3)), Rm(z2 * Ra(y3 - y1)), Rm(z3_ * Ra(y1 - y2)))
        b = sum3(Rm(z1 * Ra(x3 - x2)), Rm(z2 * Ra(x1 - x3)), Rm(z3_ * Ra(x2 - x1)))
    else:                      # coordinates times z differences
        a = sum3(Rm(y1 * Ra(z2 - z3_)), Rm(y2 * Ra(z3_ - z1)), Rm(y3 * Ra(z1 - z2)))
        b = sum3(Rm(x1 * Ra(z3_ - z2)), Rm(x2 * Ra(z1 - z3_)), Rm(x3 * Ra(z2 - z1)))
    c = sum3(Rm(z1 * e1), Rm(z2 * e2), Rm(z3_ * e3))
    det = sum3(e1, e2, e3)
    with np.errstate(all="ignore"):
        inv = Ra(1.0 / det)
        q = lambda n: Rm(n * inv)
        def ev(a_, b_, c_):
            ax, by = Rm(a_ * x), Rm(b_ * y)
            if P["eval"] == 0: return Ra(Ra(ax + by) + c_)
            if P["eval"] == 1: return Ra(ax + Ra(by + c_))
            if P["eval"] == 2: return Ra(Ra(ax + c_) + by)
            if P["eval"] == 5:          # integer pixel coordinates, the half pixel folded into the constant
                xi, yi = np.floor(x), np.floor(y)
                cc = Ra(Ra(c_ + Rm(a_ * 0.5)) + Rm(b_ * 0.5))
                return Ra(Ra(Rm(a_ * xi) + Rm(b_ * yi)) + cc)
            if P["eval"] == 6:          # tile-relative pixel, the tile origin folded into the constant
                tx, ty = np.floor(x / 32) * 32, np.floor(y / 32) * 32
                cc = Ra(Ra(Rm(a_ * tx) + Rm(b_ * ty)) + c_)
                return Ra(Ra(Rm(a_ * (x - tx)) + Rm(b_ * (y - ty))) + cc)
            if P["eval"] == 7:          # as 6, integer pixel index and half pixel in the constant too
                tx, ty = np.floor(x / 32) * 32, np.floor(y / 32) * 32
                cc = Ra(Ra(Rm(a_ * (tx + 0.5)) + Rm(b_ * (ty + 0.5))) + c_)
                return Ra(Ra(Rm(a_ * np.floor(x - tx)) + Rm(b_ * np.floor(y - ty))) + cc)
            if P["eval"] == 3:          # start of the tile row, then steps along x
                x0 = np.floor(x / 32) * 32 + (x - np.floor(x))
                n = np.round(x - x0).astype(int)
                v = Ra(Ra(Rm(a_ * x0) + by) + c_)
                for i in range(32):
                    v = np.where(i < n, Ra(v + a_), v)
                return v
            if P["eval"] == 4:          # start of the tile, steps along y then x
                x0 = np.floor(x / 32) * 32 + (x - np.floor(x)); y0 = np.floor(y / 32) * 32 + (y - np.floor(y))
                nx, ny = np.round(x - x0).astype(int), np.round(y - y0).astype(int)
                v = Ra(Ra(Rm(a_ * x0) + Rm(b_ * y0)) + c_)
                for i in range(32):
                    v = np.where(i < ny, Ra(v + b_), v)
                for i in range(32):
                    v = np.where(i < nx, Ra(v + a_), v)
                return v
        if P["form"] == "coef":
            zz = ev(q(a), q(b), q(c))
        else:
            zz = q(ev(a, b, c))
    return Rs(zz)


def score(P, detail=False):
    da, db = depth(P, TA, ZA, PA), depth(P, TB, ZB, PB)
    pred = np.where(MODE == 0, db > da, np.where(MODE == 1, db < da, db == da))
    ok = pred == DRAWN
    if detail:
        return {int(t): (int(ok[TAG == t].sum()), int((TAG == t).sum())) for t in np.unique(TAG)}
    return int(ok.sum())


OPTIONS = {"bits": [24], "store": [19], "at": ["center"], "perm_even": [1, 4], "perm_odd": [3, 5], "sum": [0],
           "ab": ["zdy", "ydz"], "eval": [0, 3, 4, 5, 6, 7], "form": ["coef", "num"]}

if __name__ == "__main__":
    P = {"bits": 24, "store": 19, "at": "center", "perm_even": 1, "perm_odd": 5, "sum": 0, "ab": "zdy", "eval": 0, "form": "coef"}
    print(N, "observations; current model:", score(P), score(P, True))
    keys = list(OPTIONS)
    res = []
    for combo in itertools.product(*[OPTIONS[k] for k in keys]):
        Q = dict(zip(keys, combo))
        res.append((score(Q), Q))
    res.sort(key=lambda r: -r[0])
    for s, Q in res[:12]:
        print(s, Q, score(Q, True))
