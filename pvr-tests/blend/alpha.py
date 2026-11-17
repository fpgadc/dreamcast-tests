"""Does the blend unit treat alpha like a color channel?  python alpha.py [folder of dumps from run2.sh, default /tmp/pvr_tests/blend]

Views 2 and 3 of the blend test show white * (alpha of the framebuffer / of buffer 2). This predicts them
with the hardware arithmetic found by fit.py, applied to the alpha channel as well, including the extra
blend that makes alpha visible, and counts exact matches against the hardware dumps.
"""
import sys
from pathlib import Path

from compare import COLS, SETS, color
from fit import COEFS, blend, load

HW = COEFS["x' or 256 - x', x' = x but 255 -> 256"]


def main():
    d = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/pvr_tests/blend")
    shown = lambda a: (255 * (a + (a == 255))) >> 8          # white * alpha, the way hardware multiplies
    for view, name, sel in ((2, "framebuffer alpha", 0), (3, "buffer 2 alpha", 1)):
        img = {s: load(d, s, view, "hw") for s in range(SETS)}
        ok = total = untouched = 0
        for c in range(256):
            sf, df, ssel, dsel = c & 7, (c >> 3) & 7, (c >> 6) & 1, (c >> 7) & 1
            if ssel and dsel:
                continue
            for s in range(SETS):
                for q in range(4):
                    y, x = (c // COLS) * 32 + (q >> 1) * 16 + 8, (c % COLS) * 32 + (q & 1) * 16 + 8
                    got = int(img[s][y, x][0])
                    p, f, b = color(s, q, 2), color(s, q, 0), color(s, q, 1)
                    src = b if ssel else p
                    dst = b if dsel else f
                    out = blend(HW, src, dst, sf, df, False)[0]
                    alpha = out if dsel == sel else (b if sel else f)[0]      # the buffer not written keeps its alpha
                    total += 1
                    ok += shown(alpha) == got
                    untouched += shown((b if sel else f)[0]) == got
        print(f"{name}: {ok} of {total} squares match 'alpha blended like a color channel' "
              f"({untouched} would match 'alpha left as it was')")


if __name__ == "__main__":
    main()
