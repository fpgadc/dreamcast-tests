"""Compare two runs of a test built on common/pvrtest2.h, word for word, region by region.

    python3 compare2.py <test> [a, default hw] [b, default off]

a, b: hw (a console's dumps), off (refsw2-offline's dumps), emu (the emulator's screenshots; colour only),
kos (the console's dumps of the test's earlier version on KallistiOS's driver, kept in fit/kos_hw: a ported
test has to give the same picture; colour only, the earlier harness did not set the other bits),
ref (the console's dumps of this test, kept in fit/hw by archive_hw.sh: what regress.sh scores refsw2-offline
against without a console).
The regions are the labels of the test (labels.txt, written by run2.sh off). With hw and off the whole
framebuffer word is compared, including the bits that are not colour."""
import sys
from pathlib import Path

import numpy as np


def load(d, page, kind):
    if kind in ("kos", "ref"):
        import gzip
        k = Path(__file__).parent / ("fit/kos_hw" if kind == "kos" else "fit/hw") / d.name
        f = k / f"page{page}.hw.raw.gz"
        if f.exists():
            return np.frombuffer(gzip.open(f).read(), dtype="<u4").reshape(480, 640).astype(np.int64)
        return np.frombuffer(gzip.open(k / f"page{page}.hw16.raw.gz").read(), dtype="<u2").reshape(480, 640).astype(np.int64)
    if kind == "emu":
        from PIL import Image
        im = np.asarray(Image.open(d / f"page{page}.emu.png").convert("RGB")).astype(np.int64)
        return im[..., 0] << 16 | im[..., 1] << 8 | im[..., 2]
    f = d / f"page{page}.{kind}.raw"
    if f.exists():
        return np.fromfile(f, dtype="<u4").reshape(480, 640).astype(np.int64)
    return np.fromfile(d / f"page{page}.{kind}16.raw", dtype="<u2").reshape(480, 640).astype(np.int64)


def main():
    args = [x for x in sys.argv[1:] if x not in ("-q", "-a")]   # -q: the last line only; -a: every region that differs, not the 12 worst
    test = args[0]
    a, b = (args[1:] + ["hw", "off"])[:2] if len(args) < 3 else args[1:3]
    d = Path("/tmp/pvr_tests") / test
    labels = [l.split(" ", 5) for l in (d / "labels.txt").read_text().splitlines() if l.startswith("LABEL ")]
    quiet = "-q" in sys.argv
    mask = 0xffffff if "emu" in (a, b) or "kos" in (a, b) else 0xffffffff
    pages, regions, same, pixels, pixels_same = {}, 0, 0, 0, 0
    worst = []
    for _, page, cell, cw, ch, text in labels:
        page, cell, cw, ch = int(page), int(cell), int(cw), int(ch)
        if page not in pages:
            pages[page] = (load(d, page, a) & mask, load(d, page, b) & mask)
        x0, y0 = (cell % 20) * 32, (cell // 20) * 32
        ra, rb = (p[y0:y0 + ch * 32, x0:x0 + cw * 32] for p in pages[page])
        n = int((ra != rb).sum())
        regions += 1
        same += n == 0
        pixels += ra.size
        pixels_same += ra.size - n
        if n:
            i = np.argwhere(ra != rb)[0]
            worst.append((n, f"  page {page} {text}: {n} of {ra.size} pixels differ; first at ({x0 + i[1]}, {y0 + i[0]}): "
                             f"{a} {int(ra[tuple(i)]):08x}, {b} {int(rb[tuple(i)]):08x}"))
    for _, line in sorted(worst, reverse=True)[:0 if quiet else (None if "-a" in sys.argv else 12)]:
        print(line)
    print(f"{test}: {a} = {b} in {same} of {regions} regions, on {pixels_same} of {pixels} pixels")


if __name__ == "__main__":
    main()
