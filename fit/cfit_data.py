"""Samples of the Gouraud page (pvr_tests/shade page 0) in the row format of cfit3: depth 1 everywhere."""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3

a = np.fromfile(Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/shade/page0.hw.raw"), dtype="<u4").reshape(480, 640)
HW = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
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
          (32.0, 96.0, 32.0, 256.0, (0xffffffff, 0xffffffff, 0xff000000, 0xff000000))]
rows = []
for x, y, w, h, cols in quads:
    corners = [(x, y), (x + w, y), (x, y + h), (x + w, y + h)]
    for iy in range(0, int(h), 2 if h <= 32 else 3):
        for ix in range(0, int(w), 1):
            px, py = x + ix + 0.5, y + iy + 0.5
            for idx, par in (((0, 1, 2), 0), ((1, 2, 3), 1)):
                t = tuple(corners[i] for i in idx)
                if z3.inside(t, px, py):
                    cv = [float((cols[i] >> 16) & 255) for i in idx]
                    rows.append(([v for p in t for v in p], [1.0] * 3, cv, par, px, py, HW[int(py), int(px), 0], 9))
                    break
