"""When a polygon reads its source from buffer 2 (SrcSel), are its blend factors ignored?

    python srcsel.py [folder of dumps from run2.sh, default /tmp/pvr_tests/blend]

Uses the 64 settings with SrcSel set and DstSel clear (source = buffer 2, destination = framebuffer) and the
64 with both set, and counts, per hypothesis, how many squares of the hardware dumps it predicts exactly.
The arithmetic is the one fit.py found for hardware. A square only tells hypotheses apart when they predict
different colors for it, so the number of squares where a hypothesis differs from the first one is shown too.
"""
import sys
from pathlib import Path

from compare import COLS, FACTORS, SETS, color
from fit import COEFS, blend, load

HW = COEFS["x' or 256 - x', x' = x but 255 -> 256"]
ONE, ZERO = 1, 0


def run(d, kind, both):
    img = {(s, v): load(d, s, v, kind) for s in range(SETS) for v in (0, 1)}
    hyp = {
        "factors applied, source = buffer 2 (color and alpha)":
            lambda p, f, b, sf, df, dst: blend(HW, b, dst, sf, df, False),
        "factors applied, source = buffer 2's color with the polygon's alpha":
            lambda p, f, b, sf, df, dst: blend(HW, (p[0],) + b[1:], dst, sf, df, False),
        "factors applied, source = the polygon (SrcSel ignored)":
            lambda p, f, b, sf, df, dst: blend(HW, p, dst, sf, df, False),
        "factors ignored: buffer 2 copied (ONE, ZERO)":
            lambda p, f, b, sf, df, dst: blend(HW, b, dst, ONE, ZERO, False),
        "factors ignored: buffer 2 added (ONE, ONE)":
            lambda p, f, b, sf, df, dst: blend(HW, b, dst, ONE, ONE, False),
        "nothing written: destination unchanged":
            lambda p, f, b, sf, df, dst: dst,
    }
    names = list(hyp)
    ok = {n: 0 for n in names}
    differs = {n: 0 for n in names}
    per_factor = {n: [0] * 64 for n in names}
    total = 0
    for c in range(192 if both else 64, 256 if both else 128):
        sf, df = c & 7, (c >> 3) & 7
        for s in range(SETS):
            for q in range(4):
                y, x = (c // COLS) * 32 + (q >> 1) * 16 + 8, (c % COLS) * 32 + (q & 1) * 16 + 8
                got = tuple(int(t) for t in img[(s, 1 if both else 0)][y, x])
                p, f, b = color(s, q, 2), color(s, q, 0), color(s, q, 1)
                dst = b if both else f
                want = {n: hyp[n](p, f, b, sf, df, dst)[1:] for n in names}
                total += 1
                for n in names:
                    ok[n] += want[n] == got
                    differs[n] += want[n] != want[names[0]]
                    per_factor[n][c & 63] += want[n] == got
    where = "buffer 2 -> buffer 2" if both else "buffer 2 -> framebuffer"
    print(f"--- {kind}: source = {where}, {total} squares")
    for n in names:
        note = "" if n == names[0] else f"   (predicts something else than the first in {differs[n]} squares)"
        print(f"  {ok[n]:5d} match   {n}{note}")
    return per_factor[names[0]], total // 64


def main():
    d = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/pvr_tests/blend")
    for kind in ("hw", "off"):
        if not (d / f"page0.{kind}.raw").exists():
            continue
        run(d, kind, False)
        per, n = run(d, kind, True)
        if kind == "hw":
            print("  buffer 2 -> buffer 2, squares (of %d) that match 'factors applied', by setting:" % n)
            print("        dst factor:  " + "  ".join(f"{FACTORS[f]:>7s}" for f in range(8)))
            for sf in range(8):
                print(f"    src {FACTORS[sf]:>8s}:  " + "  ".join(f"{per[sf + 8 * df]:7d}" for df in range(8)))
    print("\n(the offline lines use the hardware's arithmetic, so they are only a rough guide)")


if __name__ == "__main__":
    main()
