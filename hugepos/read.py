"""hugepos: per page and triangle, how much of what exact geometry covers is drawn.

    python read.py [folder, default /tmp/pvr_tests/hugepos] [hw | emu | off]      (off: refsw2-offline, run2.sh)
"""
import sys
from fractions import Fraction as Fr
from pathlib import Path

import numpy as np
from PIL import Image

import json

MS = json.loads((Path(__file__).parent / "cases.json").read_text())["ms"]


def load(d, page, kind):
    if kind in ("hw", "off"):
        a = np.fromfile(d / f"page{page}.{kind}.raw", dtype="<u4").reshape(480, 640)
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
    return np.asarray(Image.open(d / f"page{page}.emu.png").convert("RGB")).astype(int)


def tris(m):
    M = float(np.float32(2.0) ** m)
    return [[(-4 * M, -3 * M), (4 * M, 3 * M), (-3 * M, 4 * M)], [(-64.0, -64.0), (704.0, 0.0), (-3 * M, 4 * M)], [(-4 * M, 100.0), (4 * M, 380.0), (320.0, -1000.0)]]


def exact(t):
    """coverage in exact arithmetic (pixel centers, ties left out: they do not matter for the totals)"""
    gy, gx = np.mgrid[0:480, 0:640]
    inside = np.ones((480, 640), bool)
    P = [(Fr(float(np.float32(x))), Fr(float(np.float32(y)))) for x, y in t]
    area = (P[1][0] - P[0][0]) * (P[2][1] - P[0][1]) - (P[2][0] - P[0][0]) * (P[1][1] - P[0][1])
    s = 1 if area > 0 else -1
    for i in range(3):
        (xa, ya), (xb, yb) = P[i], P[(i + 1) % 3]
        # (xb - xa) (py - ya) - (yb - ya) (px - xa), scaled down so that float64 keeps the sign
        ex, ey = xb - xa, yb - ya
        k = max(abs(ex), abs(ey))
        fx, fy = float(ex / k), float(ey / k)
        c = float((-ex * ya + ey * xa) / k)
        inside &= s * (fx * (gy + 0.5) - fy * (gx + 0.5) + c) > 0
    return inside


def main():
    args = [a for a in sys.argv[1:] if a not in ("hw", "emu", "off")]
    kind = "emu" if "emu" in sys.argv else "off" if "off" in sys.argv else "hw"
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/hugepos")
    which = json.loads((Path(__file__).parent / "cases.json").read_text()).get("tri", "all")
    names = ["three huge corners", "one huge corner", "two corners huge in x"]
    sel = [0, 1, 2] if which == "all" else ["a", "b", "c"].index(which)
    print(f"{'console' if kind == 'hw' else 'emulator'}, triangle {which}: pixels drawn that are inside in exact arithmetic / pixels inside (pixels drawn that are not)")
    print("   M        " + "        ".join(names[k] for k in (sel if which == "all" else [sel])))
    for p, m in enumerate(MS):
        im = load(d, p, kind) > 128
        row = []
        for ch in (sel if which == "all" else [sel]):
            e = exact(tris(m)[ch])
            got = im[..., ch] if which == "all" else im.any(axis=-1)
            row.append(f"{int((got & e).sum()):6d} / {int(e.sum()):6d} ({int((got & ~e).sum()):6d})")
        print(f"  2^{m:<4d} " + "    ".join(row))


if __name__ == "__main__":
    main()
