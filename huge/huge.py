"""Read the huge-value test's results.  python huge.py [folder, default /tmp/pvr_tests/huge]
Shows the console (hw), refsw2-offline (off, page<n>.off.raw) and, if present, an emulator's screenshots (emu)."""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

MS = [8, 10, 12, 14, 16, 18, 20, 24, 30, 40, 60, 100, 120]
ROWS = ["z, always", "z, gequal vs empty", "B = 2z greater than A = z (pass)", "B = z + 64 units greater than z (pass)",
        "B = z/2 greater than A = z (fail)", "-z, always", "-z, gequal vs empty", "B = -z greater than A = 1.0 (fail)",
        "B = 1.0 greater than A = -z (pass)"]
SROWS = ["always", "gequal vs empty", "B = value greater than A = 1.0", "B = 1.0 greater than A = value", "value equal to itself"]
SPECIAL = ["0", "-0", "den.min", "den.max", "norm.min", "max", "+inf", "-inf", "NaN", "-NaN", "sNaN"]


def load(d, page, kind):
    if kind in ("hw", "off"):
        f = d / f"page{page}.{kind}.raw"
        if not f.exists():
            return None
        a = np.fromfile(f, dtype="<u4").reshape(480, 640)
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
    f = d / f"page{page}.emu.png"
    return np.asarray(Image.open(f).convert("RGB")).astype(int) if f.exists() else None


def letter(im, r, c):
    """What the middle of the cell shows; lower case if the cell is not uniform over its inner 24x24."""
    cell = im[r * 32 + 4:r * 32 + 28, c * 32 + 4:c * 32 + 28].reshape(-1, 3)
    def name(p):
        hi = tuple(int(v > 128) for v in p)
        return {(1, 1, 1): "W", (1, 0, 0): "R", (0, 1, 0): "G", (0, 0, 0): "."}.get(hi, "?")
    names = {name(p) for p in cell[::7]}
    n = name(cell[len(cell) // 2 + 12])
    return n if len(names) == 1 else n.lower() if n != "." else ":"


def main():
    d = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/pvr_tests/huge")
    ims = {k: load(d, 0, k) for k in ("hw", "off", "emu")}
    print("page 0, depth. W white, R red (A), G green (B won), . nothing drawn; lower case or ':' = not uniform")
    print("   columns: z = 2^e, e = -126, -113, ... 108, 121, 127")
    for r, name in enumerate(ROWS):
        line = f"   {name:40s}"
        for k in ("hw", "off", "emu"):
            if ims[k] is not None:
                line += f"  {k} " + "".join(letter(ims[k], r, c) for c in range(20))
        print(line)
    print("   special values: " + " ".join(SPECIAL))
    for i, name in enumerate(SROWS):
        line = f"   {name:40s}"
        for k in ("hw", "off", "emu"):
            if ims[k] is not None:
                line += f"  {k} " + " ".join(f"{letter(ims[k], 10 + i, c):>{len(SPECIAL[c])}s}" for c in range(11))
        print(line)
    yy, xx = np.mgrid[0:480, 0:640]
    ideal = 4 * (yy + 0.5) > 3 * (xx + 0.5)
    print("pages 1-15, position: a triangle that should cover the screen below the diagonal 3x = 4y (153600 pixels)")
    for page in range(1, 16):
        name = f"M = 2^{MS[page - 1]}" if page < 14 else "M = inf" if page == 14 else "NaN x"
        line = f"   {name:10s}"
        for k in ("hw", "off", "emu"):
            im = load(d, page, k)
            if im is None:
                line += f"  {k}: no frame"
                continue
            lit = im.max(axis=-1) > 128
            wrong = lit != ideal
            # how far from the diagonal the wrong pixels are, in pixels
            dist = np.abs(4 * (yy + 0.5) - 3 * (xx + 0.5)) / 5
            line += f"  {k}: {int(lit.sum()):6d} lit, {int(wrong.sum()):6d} differ from exact" + (f" (up to {dist[wrong].max():.1f} px from the diagonal)" if wrong.any() else "")
        print(line)


if __name__ == "__main__":
    main()
