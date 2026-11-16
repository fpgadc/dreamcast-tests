"""Read the precision test's results.  python precision.py [folder, default /tmp/pvr_tests/precision]

Prints, for hardware (page<n>.hw.raw) and for refsw2-offline (page<n>.off.raw), what each page shows. See main.c for what is drawn.
"""
import struct
import sys
from pathlib import Path

import numpy as np

MANT = [0x000000, 0x7fffff, 0x400000, 0x000001, 0x2aaaaa, 0x555555, 0x00ffff, 0x7f0000]
BASES = [(127 << 23) | m for m in MANT] + [(117 << 23) | m for m in (0, 0x7fffff, 0x555555)] + [(137 << 23) | m for m in (0, 0x7fffff, 0x555555)]
Z0 = ["1.0", "1.5", "1000", "0.001", "1.9999999"]


def load(d, page, kind):
    if True:
        f = d / f"page{page}.{kind}.raw"
        if not f.exists():
            return None
        a = np.fromfile(f, dtype="<u4").reshape(480, 640)
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)


def green(im, r, c, dx=16, dy=16):
    p = im[r * 32 + dy, c * 32 + dx]
    return p[1] > 128


def f32(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def main():
    d = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/pvr_tests/precision")
    for kind, name in (("hw", "HARDWARE"), ("off", "OFFLINE RENDERER (refsw2)")):
        im = load(d, 0, kind)
        if im is None:
            continue
        print(f"===== {name}")
        print("page 0: smallest step (in units of the last place of a 32 bit float) that compares greater / less")
        for r, b in enumerate(BASES):
            up = next((1 << c for c in range(10) if green(im, r, c)), None)
            dn = next((1 << c for c in range(10) if green(im, r, 10 + c)), None)
            pat = "".join("G" if green(im, r, c) else "." for c in range(20))
            print(f"   {b:08x} ({f32(b):<12.9g})  greater from +{up}   less from -{dn}    [{pat[:10]} {pat[10:]}]")
        for page, what in ((1, "B on the inner 24x24"), (2, "B with A's vertices")):
            im = load(d, page, kind)
            print(f"page {page}: offsets (units of the last place) at which EQUAL passes, {what}")
            for r, b in enumerate(BASES):
                eq = [c - 10 for c in range(20) if green(im, r, c)]
                print(f"   {b:08x}  {eq if eq else 'never'}")
        im = load(d, 3, kind)
        print("page 3: sloped A against flat B at A's middle value. Number of the 32 pixels along the slope where B wins")
        print("        (16 = exact), then the first pixel where it loses. Columns k = 1..20 (A rises by z0 * 2^-k over the cell)")
        for r in range(10):
            counts, firsts = [], []
            for c in range(20):
                line = [bool(im[r * 32 + 16, c * 32 + i][1] > 128) for i in range(32)] if r < 5 else \
                       [bool(im[r * 32 + i, c * 32 + 16][1] > 128) for i in range(32)]
                counts.append(sum(line))
                firsts.append(line.index(False) if False in line else 32)
            print(f"   {'x' if r < 5 else 'y'} z0 {Z0[r % 5]:>9s}  wins {' '.join(f'{v:2d}' for v in counts)}")
            if counts != firsts:
                print(f"   {'':14s} first loss {' '.join(f'{v:2d}' for v in firsts)}   (not one contiguous run)")
        im = load(d, 4, kind)
        print("page 4: the same for z0 = 1.0 at each screen column (pixels where B wins; 16 = exact)")
        for r in range(15):
            counts = [sum(bool(im[r * 32 + 16, c * 32 + i][1] > 128) for i in range(32)) for c in range(20)]
            print(f"   k {r + 6:2d}  {' '.join(f'{v:2d}' for v in counts)}")
        if load(d, 6, kind) is not None:
            print("pages 6-21: flat z = 1.0 through two triangulations of the same square. Per cell, the smallest step (in units")
            print("        of the last place) at which every pixel is resolved: above (GREATER) / below (LESS). 'x' = not even at 128.")
            print("        Even columns are 32x32 squares, odd columns 8x8. Rows are screen rows of cells.")
            pages = {pg: load(d, pg, kind) for pg in range(6, 22)}
            for r in range(15):
                out = []
                for c in range(20):
                    lo, hi = (0, 32) if c % 2 == 0 else (12, 20)
                    def need(first):
                        for q in range(8):
                            cell = pages[first + q][r * 32 + lo:r * 32 + hi, c * 32 + lo:c * 32 + hi, 1] > 128
                            if cell.all():
                                return str(1 << q)
                        return "x"
                    out.append(f"{need(6)}/{need(14)}")
                print("   " + " ".join(f"{v:>7s}" for v in out))
        im = load(d, 5, kind)
        print("page 5: edge position. For an edge moved 2^-n pixels off a pixel center: the largest n (1..32) for which the")
        print("        pixel is still treated as on the far side (past the center: not lit; before the center: lit)")
        for r in range(9):
            if r == 4:
                continue
            kind_, out = r % 5, []
            for c in range(5):
                col = 19 if c == 4 else c * 5
                lit = [bool(im[r * 32 + i, col * 32 + 16].max() > 128) for i in range(32)] if kind_ < 2 else \
                      [bool(im[r * 32 + 16, col * 32 + i].max() > 128) for i in range(32)]
                want = bool(kind_ % 2)          # before the center -> lit
                n = 0
                while n < 32 and lit[n] == want:
                    n += 1
                x = col * 32 + 16.5 if kind_ < 2 else r * 32 + 16.5
                # the smallest offset a 32 bit float can hold at this coordinate
                fl = int(np.floor(np.log2(x))) - 23
                out.append(f"col {col:2d}: n={n:2d} (float limit {-fl})" + ("" if lit[min(n, 31)] != want or n == 32 else ""))
            print(f"   {'left' if kind_ < 2 else 'top '} edge, {'before' if kind_ % 2 else 'past  '} center{', other winding' if r > 4 else '               '}  " + "   ".join(out))
            tie = [bool(im[r * 32 + 31, (19 if c == 4 else c * 5) * 32 + 16].max() > 128) if kind_ < 2 else
                   bool(im[r * 32 + 16, (19 if c == 4 else c * 5) * 32 + 31].max() > 128) for c in range(5)]
            print(f"   {'':52s}  at the smallest offset (rounds to the center itself): lit = {tie}")


if __name__ == "__main__":
    main()
