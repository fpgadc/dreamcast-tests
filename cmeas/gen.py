"""Cases for cmeas: Gouraud colors on triangles with exactly known planes.

    python gen.py [--seed N]        ->  cases.h, cases.json

Every polygon is one triangle of the quad over a 32x32 cell, depth 1.0, packed colors at the corners. The
three color channels are three independent planes, A = (c2 - c1) / 32 and so on, so every pixel gives three
8 bit readings of plane values that pvr_tests/uvmeas explains as numbers: what is left to learn is how the
17 bit attribute becomes an 8 bit color.

page 0  flat colors                     page 1  the same, other winding
page 2  changing along x only           page 3  along y only
page 4  both                            page 5  both, other winding
page 6  both, small differences (1 .. 8 levels across the cell)        page 7  the same, other winding

--mix wext: the colors of pages 0, 1 (flat) and 6, 7 (small differences) at depths that are not ordinary, the
same at the three corners (what pvr_tests/uvmeas measured for texture coordinates with its mixes whuge, wtiny,
wneg and wspec): pages 0-1 very large (2^98 .. 2^127), 2-3 very small (2^-126 .. 2^-100; cells 250-299
denormals and zero), 4-5 below zero (-2^-6 .. -2^8), 6-7 infinity, NaNs, the largest and smallest numbers of
both signs. Odd pages have the other winding; every fourth cell has small color differences. A depth is given
as the bits of the single precision number ("zbits").

--mix wsum: the sums of the setup at the bottom of the range. Colors of 0 .. 47 (a quarter of the cells: any)
at a depth of 1 .. 1.75 times 2^-126 .. 2^-123, so that the three terms of a slope are zeros or numbers of
2^-126 .. 2^-122, and the corners in all six orders (three rotations of each winding), so that the zero term
is the first, the second or the third. Which zero counts as 2^-127 in which addition shows in the sign of
the slope.
"""
import argparse
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
PAGES = 8


def page(rng, p):
    tris = []
    for cell in range(300):
        cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
        ccw = p in (1, 5, 7)
        c0 = rng.integers(0, 256, 3)
        if p in (0, 1):
            cols = np.stack([c0, c0, c0])
        elif p in (6, 7):
            cols = np.clip(np.stack([c0, c0 + rng.integers(-8, 9, 3), c0 + rng.integers(-8, 9, 3)]), 0, 255)
        else:
            cols = np.stack([c0, rng.integers(0, 256, 3) if p != 3 else c0, rng.integers(0, 256, 3) if p != 2 else c0])
        # cols: rows = corners TL, TR, BL; columns = r, g, b
        if ccw:
            x, y, cc = [cx, cx, cx + 32], [cy, cy + 32, cy], cols[[0, 2, 1]]
        else:
            x, y, cc = [cx, cx + 32, cx], [cy, cy, cy + 32], cols
        tris.append({"x": x, "y": y, "col": cc.astype(int).tolist(), "group": f"{['flat', 'flat', 'x', 'y', 'xy', 'xy', 'small', 'small'][p]} {'ccw' if ccw else 'cw'}"})
    return tris


WSPEC = [0x7f800000, 0xff800000, 0x7fc00000, 0xffc00000, 0x7fa00000, 0x7f800001, 0x7fffffff, 0x7f7fffff, 0x7f000000,
         0x7f400000, 0xff7fffff, 0xff000000, 0x7e800000, 0x00000000, 0x80000000, 0x00000001, 0x80000001, 0x00400000,
         0x007fffff, 0x807fffff, 0x00800000, 0x80800000, 0x00800001, 0x00c00000, 0x01000000, 0x81000000]


def wext_pages(rng):
    pages = []
    for p in range(PAGES):
        tris = page(rng, p % 2)                         # flat colors, the page's winding
        small = page(rng, 6 + p % 2)
        order = rng.permutation(300)
        for cell in range(300):
            t = small[cell] if cell % 4 == 3 else tris[cell]
            if p < 2:
                e = int(rng.integers(98, 128))
                b, name = int(np.float32((1.0 if cell % 2 else 1 + rng.integers(0, 1 << 15) / 32768.0) * 2.0 ** e).view(np.uint32)), "2^%d" % e
            elif p < 4:
                if cell >= 250:
                    b = int(rng.integers(1, 1 << 23)) if cell % 10 else 0
                    name = "denormal %06x" % b if b else "0"
                else:
                    e = int(rng.integers(-126, -99))
                    b, name = int(np.float32((1.0 if cell % 2 else 1 + rng.integers(0, 1 << 15) / 32768.0) * 2.0 ** e).view(np.uint32)), "2^%d" % e
            elif p < 6:
                D, e = int(rng.integers(32768, 65536)), int(rng.integers(-6, 9))
                b, name = int(np.float32(-np.ldexp(D, e - 15)).view(np.uint32)), "-%04x 2^%d" % (D, e)
            else:
                b = WSPEC[int(order[cell]) % len(WSPEC)]
                name = "%08x" % b
            t["zbits"] = [b, b, b]
            t["zname"] = name
            t["group"] = "wext " + t["group"]
            tris[cell] = t
        pages.append(tris)
    return pages


def wsum_pages(rng):
    pages = []
    for p in range(PAGES):
        tris = []
        for cell in range(300):
            cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
            order = int(rng.integers(0, 6))
            corners = [(cx, cy), (cx + 32, cy), (cx, cy + 32)]                  # TL, TR, BL: one winding
            if order >= 3:
                corners = [corners[0], corners[2], corners[1]]                  # the other
            corners = corners[order % 3:] + corners[:order % 3]
            hi = 256 if cell % 4 == 3 else 48
            cols = rng.integers(0, hi, (3, 3))
            if rng.uniform() < 0.3:
                cols[1] = cols[0]
            if rng.uniform() < 0.3:
                cols[2] = cols[0]
            m, e = 1 + int(rng.integers(0, 4)) / 4.0, int(rng.integers(-126, -122))
            b = int(np.float32(m * 2.0 ** e).view(np.uint32))
            tris.append({"x": [c[0] for c in corners], "y": [c[1] for c in corners], "col": cols.astype(int).tolist(),
                         "group": "wsum order %d" % order, "zbits": [b, b, b], "zname": "%g * 2^%d" % (m, e)})
        pages.append(tris)
    return pages


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--mix", default="first", choices=["first", "wext", "wsum"])
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)
    pages = wext_pages(rng) if args.mix == "wext" else wsum_pages(rng) if args.mix == "wsum" else [page(rng, p) for p in range(PAGES)]
    write(HERE, {"seed": args.seed, "mix": args.mix, "pages": pages})
    print(f"seed {args.seed}: {PAGES} pages, {sum(len(p) for p in pages)} triangles")


def write(folder, data):
    """cases.h and cases.json from the description of a set (also used to run an archived set again).

    Every float goes into cases.h as the bit pattern of the single precision number: the test program never
    converts or calculates one, so a PC and the console send the same bits."""
    pages = data["pages"]
    fl = lambda v: "0x%08xu" % int(np.float32(v).view(np.uint32))
    lines = ["/* Generated by gen.py - do not edit. Floats are bit patterns. */", "#include <stdint.h>", f"#define NUM_TRIS {sum(len(p) for p in pages)}",
             "typedef struct { unsigned char page; uint32_t xy[3][2]; uint32_t argb[3]; uint32_t z[3]; } tri_t;   /* z: the depth's bits */", "static const tri_t tris[NUM_TRIS] = {"]
    for p, tris in enumerate(pages):
        for t in tris:
            xy = ", ".join("{ %s, %s }" % (fl(t["x"][k]), fl(t["y"][k])) for k in range(3))
            argb = ", ".join("0x%08x" % (0xff000000 | (t["col"][k][0] << 16) | (t["col"][k][1] << 8) | t["col"][k][2]) for k in range(3))
            z = ", ".join("0x%08xu" % b for b in t.get("zbits", [0x3f800000] * 3))
            lines.append("    { %d, { %s }, { %s }, { %s } }," % (p, xy, argb, z))
    lines.append("};")
    (folder / "cases.h").write_bytes((chr(10).join(lines) + chr(10)).encode())
    (folder / "cases.json").write_text(json.dumps(data))


if __name__ == "__main__":
    main()
