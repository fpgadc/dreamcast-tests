"""Read the zblock test.  python zblock.py [folder, default /tmp/pvr_tests/zblock]

For each page, the j (B's offset in steps) at which B passes, for the console (hw) and for refsw2-offline (off), in both
triangles of the cell (pixel (31, 8) is in the upper right triangle, (31, 24)... is the same one; the lower
left triangle only reaches the last column at its bottom, so pixel (30, 31) is used for it).
"""
import sys
from pathlib import Path

import numpy as np

NAMES = ["falling 32 -> 0.52, B in 2^-18 steps, GREATER", "falling, 2^-18 steps, EQUAL", "rising 1 -> 32.77, B in 2^-14 steps, GREATER",
         "rising, 2^-14 steps, EQUAL", "falling, B in 2^-13 steps, GREATER", "falling, 2^-13 steps, EQUAL"]


def load(d, page, kind):
    if True:
        f = d / f"page{page}.{kind}.raw"
        if not f.exists():
            return None
        a = np.fromfile(f, dtype="<u4").reshape(480, 640)
        return ((a >> 8) & 255) > 128


def runs(v):
    """Compress a boolean row into 'j0..j1' ranges where it is true."""
    out, start = [], None
    for i, b in enumerate(list(v) + [False]):
        if b and start is None:
            start = i
        if not b and start is not None:
            out.append(f"{start - 120}..{i - 1 - 120}" if i - 1 > start else f"{start - 120}")
            start = None
    return ", ".join(out) if out else "never"


def main():
    d = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/pvr_tests/zblock")
    for page in range(6):
        print(f"page {page}: {NAMES[page]}")
        for kind in ("hw", "off"):
            g = load(d, page, kind)
            if g is None:
                continue
            for name, (px, py) in (("upper triangle, pixel (31, 8)", (31, 8)), ("lower triangle, pixel (30, 31)", (30, 31))):
                row = [bool(g[(c // 20) * 32 + py, (c % 20) * 32 + px]) for c in range(240)]
                print(f"   {kind:3s} {name}: B passes for j = {runs(row)}")


if __name__ == "__main__":
    main()
