"""Fit the arithmetic of Gouraud color interpolation against the console (pvr_tests/shade page 0)."""
import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3

D = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/shade")
a = np.fromfile(D / "page0.hw.raw", dtype="<u4").reshape(480, 640)
HW = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)       # r g b

small = [
    (0xff000000, 0xffffffff, 0xff000000, 0xffffffff), (0xffffffff, 0xff000000, 0xffffffff, 0xff000000),
    (0xff000000, 0xff000000, 0xffffffff, 0xffffffff), (0xffffffff, 0xffffffff, 0xff000000, 0xff000000),
    (0xffffffff, 0xff000000, 0xff000000, 0xff000000), (0xff000000, 0xffffffff, 0xff000000, 0xff000000),
    (0xff000000, 0xff000000, 0xffffffff, 0xff000000), (0xff000000, 0xff000000, 0xff000000, 0xffffffff),
    (0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff), (0xff000000, 0xff101010, 0xff000000, 0xff101010),
    (0xff000000, 0xff010101, 0xff000000, 0xff010101), (0xfffefefe, 0xffffffff, 0xfffefefe, 0xffffffff),
    (0xff646464, 0xff656565, 0xff646464, 0xff656565), (0xff808080, 0xff808080, 0xff808080, 0xff808080),
    (0xff000000, 0xff0000ff, 0xff00ff00, 0xffff0000), (0xff7f7f7f, 0xff808080, 0xff808080, 0xff7f7f7f),
]
quads = [(c * 32.0, 0.0, 32.0, 32.0, small[c]) for c in range(16)]
quads += [(0.0, 32.0, 640.0, 32.0, (0xff000000, 0xffffffff, 0xff000000, 0xffffffff)),
          (0.0, 64.0, 640.0, 32.0, (0xffffffff, 0xff000000, 0xffffffff, 0xff000000)),
          (0.0, 96.0, 32.0, 256.0, (0xff000000, 0xff000000, 0xffffffff, 0xffffffff)),
          (32.0, 96.0, 32.0, 256.0, (0xffffffff, 0xffffffff, 0xff000000, 0xff000000)),
          (64.0, 96.0, 32.0, 256.0, (0xff0000ff, 0xff0000ff, 0xffff0000, 0xffff0000))]

rows = []       # x1 y1 x2 y2 x3 y3, c1 c2 c3, parity, px, py, want
for x, y, w, h, cols in quads:
    corners = [(x, y), (x + w, y), (x, y + h), (x + w, y + h)]
    tris = [((0, 1, 2), 0), ((1, 2, 3), 1)]
    stepx = 1 if w <= 32 else 7
    stepy = 1 if h <= 32 else 5
    for iy in range(0, int(h), stepy):
        for ix in range(0, int(w), stepx):
            px, py = x + ix + 0.5, y + iy + 0.5
            for idx, par in tris:
                t = tuple(corners[i] for i in idx)
                if z3.inside(t, px, py):
                    for ch, sh in ((0, 16), (1, 8), (2, 0)):
                        cv = [float((cols[i] >> sh) & 255) for i in idx]
                        rows.append([c for v in t for c in v] + cv + [par, px, py, HW[int(py), int(px), ch]])
                    break
A = np.array(rows)
T = A[:, 0:6]; CV = A[:, 6:9]; PAR = A[:, 9].astype(int); PX = A[:, 10]; PY = A[:, 11]; WANT = A[:, 12].astype(int)
PERMS = list(itertools.permutations(range(3)))


def color(P):
    R = z3.make_rnd(P["bits"], "trunc")
    pe, po = PERMS[P["perm_even"]], PERMS[P["perm_odd"]]
    idx = np.where(PAR[:, None] == 0, np.array(pe)[None, :], np.array(po)[None, :])
    r = np.arange(len(T))
    x1, x2, x3 = (T[r, 2 * idx[:, i]] for i in range(3))
    y1, y2, y3 = (T[r, 2 * idx[:, i] + 1] for i in range(3))
    c1, c2, c3 = (CV[r, idx[:, i]] for i in range(3))
    off = 0.5 if P["at"] == "corner" else 0.0
    x, y = PX - off, PY - off
    e1 = R(R(x2 * y3) - R(x3 * y2)); e2 = R(R(x3 * y1) - R(x1 * y3)); e3 = R(R(x1 * y2) - R(x2 * y1))
    det = R(R(e1 + e2) + e3)
    inv = R(1.0 / det)
    if P["form"] == "det":
        a = R(R(R(c1 * R(y2 - y3)) + R(c2 * R(y3 - y1))) + R(c3 * R(y1 - y2)))
        b = R(R(R(c1 * R(x3 - x2)) + R(c2 * R(x1 - x3))) + R(c3 * R(x2 - x1)))
        c = R(R(R(c1 * e1) + R(c2 * e2)) + R(c3 * e3))
        v = R(R(R(R(a * inv) * x) + R(R(b * inv) * y)) + R(c * inv))
    else:       # differences from vertex 1
        d2, d3 = R(c2 - c1), R(c3 - c1)
        a = R(R(d2 * R(y3 - y1)) - R(d3 * R(y2 - y1)))
        b = R(R(d3 * R(x2 - x1)) - R(d2 * R(x3 - x1)))
        dd = R(R(R(x2 - x1) * R(y3 - y1)) - R(R(x3 - x1) * R(y2 - y1)))
        i2 = R(1.0 / dd)
        v = R(c1 + R(R(R(a * i2) * R(x - x1)) + R(R(b * i2) * R(y - y1))))
    if P["final"] == "round": out = np.floor(v + 0.5)
    elif P["final"] == "trunc": out = np.floor(v)
    else: out = np.floor(v + P["final"])
    return np.clip(out, 0, 255).astype(int)


OPT = {"bits": [24], "perm_even": [0], "perm_odd": [0], "at": ["center"], "form": ["det", "diff"], "final": [0.5 - 2.0 ** -k for k in (2, 3, 4, 5, 6, 8, 10, 12, 16, 20)]}
print(len(A), "pixel/channel samples")
res = []
keys = list(OPT)
for combo in itertools.product(*[OPT[k] for k in keys]):
    P = dict(zip(keys, combo))
    with np.errstate(all="ignore"):
        s = int((color(P) == WANT).sum())
    res.append((s, P))
res.sort(key=lambda r: -r[0])
for s, P in res[:14]:
    print(s, P)
