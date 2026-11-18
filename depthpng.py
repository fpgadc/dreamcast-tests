"""A picture of a depth buffer read out by zexfil (dumprender.sh z: depth.*.raw; pvr-tests/zexfil: page*.raw).

    python3 depthpng.py <depth.raw> [out.png] [--like other.raw] [--diff other.raw]

The depths are 32 bit words, 640 per line, the floats' bits (1/w: larger is nearer). Grey on a log scale, nearer
brighter, from the 0.5th to the 99.5th percentile of the positive depths (--like: of another file's, so that two
pictures have the same scale); black where there is nothing (0), dark blue for a depth below +0 (0xffffffff) or
not a number. --diff: the depths that differ from another file's in red, each with a red box around it so that a
single pixel shows. Out: the input's name with .png for .raw."""
import sys
import numpy as np
from PIL import Image


def load(path):
    a = np.fromfile(path, dtype="<u4")
    return a.reshape(len(a) // 640, 640)


def main():
    args, opts, k = [], {}, 1
    while k < len(sys.argv):
        if sys.argv[k] in ("--like", "--diff"):
            opts[sys.argv[k]] = sys.argv[k + 1]
            k += 2
        else:
            args.append(sys.argv[k])
            k += 1
    if not args:
        print(__doc__)
        return 1
    a = load(args[0])
    out = args[1] if len(args) > 1 else (args[0][:-4] if args[0].endswith(".raw") else args[0]) + ".png"
    f = a.view("<f4")
    with np.errstate(invalid="ignore"):
        ok = (a != 0xffffffff) & np.isfinite(f) & (f > 0)
    ref = load(opts["--like"]) if "--like" in opts else a
    rf = ref.view("<f4")
    with np.errstate(invalid="ignore"):
        rok = (ref != 0xffffffff) & np.isfinite(rf) & (rf > 0)
    if rok.any():
        lo, hi = np.percentile(np.log2(rf[rok].astype(np.float64)), [0.5, 99.5])
    else:
        lo, hi = 0.0, 1.0
    hi = max(hi, lo + 1e-9)
    g = np.zeros(a.shape)
    g[ok] = 40 + 215 * np.clip((np.log2(f[ok].astype(np.float64)) - lo) / (hi - lo), 0, 1)
    rgb = np.stack([g, g, g], -1).astype(np.uint8)
    rgb[~ok & (a != 0)] = (0, 0, 96)
    if "--diff" in opts:
        b = load(opts["--diff"])
        for y, x in np.argwhere(a != b):
            y0, y1, x0, x1 = max(y - 4, 0), min(y + 4, a.shape[0] - 1), max(x - 4, 0), min(x + 4, 639)
            rgb[y0, x0:x1 + 1] = rgb[y1, x0:x1 + 1] = (255, 0, 0)
            rgb[y0:y1 + 1, x0] = rgb[y0:y1 + 1, x1] = (255, 0, 0)
            rgb[y, x] = (255, 0, 0)
    Image.fromarray(rgb).save(out)
    print(f"{out}: log2 depth {lo:.2f} .. {hi:.2f} (2^{lo:.1f} .. 2^{hi:.1f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
