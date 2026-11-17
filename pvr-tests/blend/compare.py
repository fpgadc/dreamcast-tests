"""Compare the blend test's console dumps with refsw2-offline's, square by square.

    python compare.py [folder with page<p>.hw.raw and page<p>.off.raw, default /tmp/pvr_tests/blend] [-v]

page p = set * 4 + view (run2.sh blend off hw=<ip>). Prints, per blend setting, whether hardware and the
offline renderer agree, and for those that do not, what each of them produced for one square. -v lists every
differing square. Also writes page<p>.hw.png for each dump.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

FACTORS = ["ZERO", "ONE", "OTHER", "1-OTHER", "SRC_A", "1-SRC_A", "DST_A", "1-DST_A"]
VIEWS = ["fb color", "buf2 color", "fb alpha", "buf2 alpha"]
COLS, SETS = 20, 4
M = 0xffffffff


def color(s, q, layer):
    """Same as color() in main.c: (a, r, g, b)."""
    edge = [0, 1, 127, 128, 254, 255]
    x = ((s * 12 + q * 3 + layer) * 2654435761 + 12345) & M
    out = []
    for _ in range(4):
        x = (x * 1664525 + 1013904223) & M
        b = (x >> 16) & 0xff
        out.append(edge[b % 6] if b < 96 else b)
    return tuple(out)


def setting(c):
    return (f"src {'buf2' if c & 64 else 'poly'} -> dst {'buf2' if c & 128 else 'fb'}".ljust(21)
            + f"{FACTORS[c & 7]:>7s} , {FACTORS[(c >> 3) & 7]:<7s}")


def rgb(d, s, v, kind):
    """The color of page set * 4 + view as (480, 640, 3)."""
    a = np.fromfile(d / f"page{s * 4 + v}.{kind}.raw", dtype="<u4").reshape(480, 640)
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(np.int16)


def main():
    args = [a for a in sys.argv[1:] if a != "-v"]
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/blend")
    verbose = "-v" in sys.argv
    diffs = {c: [] for c in range(256)}          # setting -> [(set, view, q, hw, emu)]
    uneven = 0
    seen = 0
    for s in range(SETS):
        for v in range(4):
            raw, off = d / f"page{s * 4 + v}.hw.raw", d / f"page{s * 4 + v}.off.raw"
            if not raw.exists() or not off.exists():
                print(f"set {s} view {v}: missing {'console dump' if not raw.exists() else 'offline renderer dump'}")
                continue
            seen += 1
            hw, emu = rgb(d, s, v, "hw"), rgb(d, s, v, "off")
            Image.fromarray(hw.astype(np.uint8)).save(d / f"page{s * 4 + v}.hw.png")
            for c in range(256):
                for q in range(4):
                    y, x = (c // COLS) * 32 + (q >> 1) * 16, (c % COLS) * 32 + (q & 1) * 16
                    h, e = hw[y:y + 16, x:x + 16].reshape(-1, 3), emu[y:y + 16, x:x + 16].reshape(-1, 3)
                    uneven += int((h != h[0]).any()) + int((e != e[0]).any())
                    hv, ev = tuple(int(t) for t in h[136]), tuple(int(t) for t in e[136])     # the square's center
                    if hv != ev:
                        diffs[c].append((s, v, q, hv, ev))
    if not seen:
        return
    print(f"{seen} set/view pairs compared; squares that are not one flat color: {uneven}")
    bad = [c for c in range(256) if diffs[c]]
    print(f"{256 - len(bad)} of 256 blend settings match exactly in every set and view; {len(bad)} differ\n")
    for c in bad:
        views = sorted({v for _, v, _, _, _ in diffs[c]})
        worst = max(max(abs(a - b) for a, b in zip(h, e)) for _, _, _, h, e in diffs[c])
        print(f"{c:3d}  {setting(c)}  {len(diffs[c]):3d} of {seen * 4} squares differ, max {worst:3d}, in: "
              + ", ".join(VIEWS[v] for v in views))
        rows = diffs[c] if verbose else diffs[c][:1]
        for s, v, q, h, e in rows:
            print(f"       set {s} square {q} {VIEWS[v]:10s}: hardware {h}  offline {e}   "
                  f"[argb: poly {color(s, q, 2)}  fb {color(s, q, 0)}  buf2 {color(s, q, 1)}]")


if __name__ == "__main__":
    main()
