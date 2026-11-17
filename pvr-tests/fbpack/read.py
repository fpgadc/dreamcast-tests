"""Compare the fbpack dumps of two runs, pixel for pixel, with the page's pixel format taken into account.

    python3 read.py [a, default hw] [b, default off] [folder, default /tmp/pvr_tests/fbpack]

hw: a console, off: refsw2-offline (see ../../run2.sh). The formats are read from main.c."""
import re
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
MODES = dict(fb_packmode_0555=0, fb_packmode_565=1, fb_packmode_4444=2, fb_packmode_1555=3, fb_packmode_888=4,
             fb_packmode_0888=5, fb_packmode_8888=6)
BYTES = [2, 2, 2, 2, 3, 4, 4]
# (name, shift, bits) of the fields of a pixel
FIELDS = [[("K", 15, 1), ("R", 10, 5), ("G", 5, 5), ("B", 0, 5)], [("R", 11, 5), ("G", 5, 6), ("B", 0, 5)],
          [("A", 12, 4), ("R", 8, 4), ("G", 4, 4), ("B", 0, 4)], [("A", 15, 1), ("R", 10, 5), ("G", 5, 5), ("B", 0, 5)],
          [("R", 16, 8), ("G", 8, 8), ("B", 0, 8)], [("K", 24, 8), ("R", 16, 8), ("G", 8, 8), ("B", 0, 8)],
          [("A", 24, 8), ("R", 16, 8), ("G", 8, 8), ("B", 0, 8)]]
ROWS = ["opaque", "opaque, alpha ignored", "translucent", "translucent, alpha ignored", "translucent, blended",
        "punch through", "translucent over opaque", "background"]


def formats():
    src = (HERE / "main.c").read_text()
    body = src[src.index("formats[NUM_PAGES] = {"):]
    body = body[:body.index("};")]
    out = []
    for m in re.finditer(r"\{\s*(fb_packmode_\w+),\s*(\w+),\s*(\w+)(?:,\s*(\w+))?\s*\}", body):
        out.append((MODES[m.group(1)], int(m.group(2), 16), int(m.group(3), 16), int(m.group(4) or "0")))
    return out


def pixels(path, mode):
    n = BYTES[mode]
    raw = np.fromfile(path, dtype="<u1")[:640 * 480 * n].reshape(480, 640, n).astype(np.int64)
    return sum(raw[..., k] << (8 * k) for k in range(n))


def main():
    args = sys.argv[1:]
    a, b = (args + ["hw", "off"])[:2] if len(args) < 2 else args[:2]
    d = Path(args[2]) if len(args) > 2 else Path("/tmp/pvr_tests/fbpack")
    total = same = 0
    for p, (mode, kval, threshold, dither) in enumerate(formats()):
        pa, pb = pixels(d / f"page{p}.{a}.raw", mode), pixels(d / f"page{p}.{b}.raw", mode)
        bad = []
        for r, name in enumerate(ROWS):
            ys = slice(r * 32, 480 if r == 7 else r * 32 + 32)
            x, y = pa[ys], pb[ys]
            total += x.size
            same += int((x == y).sum())
            if (x != y).any():
                parts = [f"{f} {int((((x >> s) ^ (y >> s)) & ((1 << n) - 1) != 0).sum())}" for f, s, n in FIELDS[mode]]
                i = np.argwhere(x != y)[0]
                bad.append(f"    {name}: {int((x != y).sum())} of {x.size} pixels differ ({', '.join(parts)}); "
                           f"first at x {i[1]}: {a} {int(x[tuple(i)]):x}, {b} {int(y[tuple(i)]):x}")
        print(f"page {p:2d}  packmode {mode} kval {kval:02x} threshold {threshold:02x} dither {dither}: " + ("same" if not bad else "DIFFERENT"))
        for line in bad:
            print(line)
    print(f"fbpack: {a} = {b} on {same} of {total} pixels")


if __name__ == "__main__":
    main()
