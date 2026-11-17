"""Which arithmetic does the hardware's blend unit use?  python fit.py [folder of dumps, default /tmp/pvr_tests/blend]

Predicts the framebuffer / buffer 2 color of every square from the known input colors with several candidate
formulas and counts exact matches against the hardware dumps (and against refsw2-offline's).
Buffer selection is modeled the way refsw2 does it: source = SrcSel ? buffer 2 : polygon,
destination = DstSel ? buffer 2 : framebuffer, written to the destination.
"""
import sys
from pathlib import Path

import numpy as np

from compare import COLS, FACTORS, SETS, color, setting

# (x, inverted) -> multiplier out of 256, where the coefficient is x / 255 or, inverted, 1 - x / 255
up = lambda x: x + (x == 255)
COEFS = {
    "c + (c >> 7), c = x or 255 - x      (refsw2)": lambda x, inv: (255 - x if inv else x) + ((255 - x if inv else x) >> 7),
    "c, c = x or 255 - x": lambda x, inv: 255 - x if inv else x,
    "c + 1, c = x or 255 - x": lambda x, inv: (255 - x if inv else x) + 1,
    "x' or 256 - x', x' = x but 255 -> 256": lambda x, inv: 256 - up(x) if inv else up(x),
}


def blend(coef, src, dst, sf, df, joint):
    """src, dst: (a, r, g, b). Returns the blended (a, r, g, b).
    joint: the two products are added before the shift, rather than each shifted and then added."""
    out = []
    for ch in range(4):
        def c(mode, other):
            x = [0, 255, other[ch], other[ch], src[0], src[0], dst[0], dst[0]][mode]
            return coef(x, mode >= 2 and mode % 2 == 1)
        cs, cd = c(sf, dst), c(df, src)
        v = (src[ch] * cs + dst[ch] * cd) >> 8 if joint else (src[ch] * cs >> 8) + (dst[ch] * cd >> 8)
        out.append(min(v, 255))
    return tuple(out)


def load(d, s, v, kind):
    """kind: hw (console) or off (refsw2-offline); the page is set * 4 + view."""
    a = np.fromfile(d / f"page{s * 4 + v}.{kind}.raw", dtype="<u4").reshape(480, 640)
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1)


def main():
    d = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/pvr_tests/blend")
    for kind in ("hw", "off"):
        img = {(s, v): load(d, s, v, kind) for s in range(SETS) for v in (0, 1)}
        print(f"--- {'hardware' if kind == 'hw' else 'offline renderer'}: exact matches of the written buffer's color, "
              f"by formula (of {192 * 4 * SETS} squares; buffer 2 -> buffer 2 settings left out)")
        models = [(f"{n:46s} {'sum, then >> 8' if j else 'each >> 8, then sum'}", m, j) for n, m in COEFS.items() for j in (False, True)]
        misses = {}
        for name, coef, joint in models:
            ok = 0
            per_factor = np.zeros((8, 2), dtype=int)       # [factor, as src / as dst] -> misses
            for c in range(256):
                if (c & 192) == 192:
                    continue
                sf, df, ssel, dsel = c & 7, (c >> 3) & 7, (c >> 6) & 1, (c >> 7) & 1
                for s in range(SETS):
                    for q in range(4):
                        y, x = (c // COLS) * 32 + (q >> 1) * 16 + 8, (c % COLS) * 32 + (q & 1) * 16 + 8
                        got = tuple(int(t) for t in img[(s, dsel)][y, x])
                        src = color(s, q, 1) if ssel else color(s, q, 2)
                        dst = color(s, q, 1) if dsel else color(s, q, 0)
                        want = blend(coef, src, dst, sf, df, joint)[1:]
                        if got == want:
                            ok += 1
                        else:
                            per_factor[sf, 0] += 1
                            per_factor[df, 1] += 1
            misses[name] = per_factor
            print(f"  {ok:5d}  {name}")
        best = min(misses, key=lambda n: misses[n].sum())
        if misses[best].sum():
            print(f"  misses of the best formula ({best}) by factor, as source / as destination:")
            for f in range(8):
                print(f"      {FACTORS[f]:8s} {misses[best][f, 0]:5d} {misses[best][f, 1]:5d}")
    # buffer 2 -> buffer 2: does anything get written?
    print("--- source = buffer 2 and destination = buffer 2 (settings 192..255), hardware:")
    same = changed = 0
    examples = []
    for c in range(192, 256):
        for s in range(SETS):
            b2 = load(d, s, 1, "hw")
            for q in range(4):
                y, x = (c // COLS) * 32 + (q >> 1) * 16 + 8, (c % COLS) * 32 + (q & 1) * 16 + 8
                got = tuple(int(t) for t in b2[y, x])
                if got == color(s, q, 1)[1:]:
                    same += 1
                else:
                    changed += 1
                    if len(examples) < 4:
                        examples.append(f"      {setting(c)}: buffer 2 was {color(s, q, 1)[1:]}, became {got}")
    print(f"  buffer 2 left exactly as it was in {same} squares, changed in {changed}")
    print("\n".join(examples))


if __name__ == "__main__":
    main()
