"""What the cells of accbuf show.

    python3 pvr-tests/accbuf/analyze.py [kind, default hw] [folder, default /tmp/pvr_tests/accbuf] [pages, e.g. 0,2,5]

Every labelled cell (a tile) as a list of its colours with the pixels' count and bounding box (x0-x1, y0-y1 within
the tile). For the race pages (6 to 8) the cells of bands are checked against the bands: which pixels differ."""
import sys
from pathlib import Path

import numpy as np

NAMES = {0x000040: "background", 0x202020: "grey20", 0x404040: "grey40", 0x606060: "grey60", 0x808080: "grey80",
         0xff0000: "red", 0x00ff00: "green", 0x0000ff: "blue", 0xffff00: "yellow", 0xff00ff: "magenta", 0x00ffff: "cyan",
         0xff8000: "orange", 0x8000ff: "purple", 0x00ff80: "mint", 0xffffff: "white"}
for j in range(8):
    NAMES[0x002000 + 0x001c00 * j] = f"band{j}"


def name(c):
    return NAMES.get(c, f"{c:06x}")


def describe(t):
    out = []
    cols, counts = np.unique(t, return_counts=True)
    for c, n in sorted(zip(cols.tolist(), counts.tolist()), key=lambda cn: -cn[1])[:8]:
        ys, xs = np.nonzero(t == c)
        out.append(f"{name(c)} {n} [{xs.min()}-{xs.max()},{ys.min()}-{ys.max()}]")
    if len(cols) > 8:
        out.append(f"... {len(cols)} colours")
    return "; ".join(out)


def bands_damage(t):
    exp = np.repeat(np.array([0x002000 + 0x001c00 * j for j in range(8)]), 4)[:, None] * np.ones((1, 32), dtype=np.int64)
    bad = t != exp
    if not bad.any():
        return "bands whole"
    ys, xs = np.nonzero(bad)
    cols = sorted(set(name(c) for c in np.unique(t[bad]).tolist()))
    rows = sorted(set(ys.tolist()))
    return f"DAMAGED: {int(bad.sum())} pixels, lines {rows[0]}-{rows[-1]} ({len(rows)} lines), x {xs.min()}-{xs.max()}, as {', '.join(cols[:4])}"


def main():
    kind = sys.argv[1] if len(sys.argv) > 1 else "hw"
    d = Path(sys.argv[2] if len(sys.argv) > 2 else "/tmp/pvr_tests/accbuf")
    pages = [int(x) for x in sys.argv[3].split(",")] if len(sys.argv) > 3 else None
    labels = {}
    for line in open(d / "labels.txt", errors="replace"):
        p = line.split(None, 5)
        if p and p[0] == "LABEL":
            labels.setdefault(int(p[1]), []).append((int(p[2]), p[5].strip()))
    for page in sorted(labels):
        if pages is not None and page not in pages:
            continue
        im = np.fromfile(d / f"page{page}.{kind}.raw", dtype="<u4").reshape(480, 640).astype(np.int64) & 0xffffff
        print(f"page {page}")
        for cell, text in labels[page]:
            x, y = (cell % 20) * 32, (cell // 20) * 32
            t = im[y:y + 32, x:x + 32]
            if page >= 6 and text.startswith("bands"):
                print(f"  {cell:3d} {text[:58]:58s} {bands_damage(t)}")
            else:
                print(f"  {cell:3d} {text[:58]:58s} {describe(t)}")


if __name__ == "__main__":
    main()
