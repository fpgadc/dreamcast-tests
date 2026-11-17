"""Cases for mipmeas: the mipmap level and the trilinear fraction, read as numbers.

    python3 gen.py [--seed N] [--mix first]        ->  cases.h, cases.json

Every cell is a 32x32 quad with a 64x64 mipmapped texture whose levels are flat colours (main.c), mapped so
that su texels fall on a pixel along x and sv along y. With trilinear pass B (FilterMode 3) alone on the
screen a pixel shows the NEXT level's colour times the blend coefficient, so the colour gives the level
the TSP chose and the fraction it blends with; pass A (FilterMode 2) shows this level times the rest.

page 0  bias 4/4, pass B, su = sv = 1 + c/300            page 1  the same, pass A
page 2  bias 4/4, pass B, su = sv = 2^(c/50): six levels  page 3  bias 5/4, pass B, 1 + c/300
page 4  bias 7/4, pass B, 2^(c/50) / 2                    page 5  bias 4/4, 1/w = 0.75, 1 + c/300
page 6  bias 4/4, pass B, not square: sv = su / 2 (cells 0-149), su = sv / 2 (150-299)
page 7  bias 13/4, pass B, 24 bit ratios between 1/2 and 8

--mix w: how 1/w enters (all pass B, bias 4/4 unless said):
page 0  ratio 1.5, flat 1/w = 0.5 + c/600               page 1  ratio 1 + c/300, flat 1/w with 24 bits, 2^-3 .. 2^3
page 2  ratio 1 + c/300, flat 1/w a power of two         page 3  ratio 1.25, flat 1/w with 16 bit mantissas
page 4  1/w falls from 1 to 0.5 left to right, ratio (at 1/w = 1) 1 + c/300         page 5  the same top to bottom
page 6  ratio 1.5, flat 1/w = (256 + c) / 256 .. : every 8 bit mantissa step and between
page 7  bias, ratio and flat 1/w all random

--mix dcalc: DCalcCtrl 1 (the numerators taken at the first vertex, not at the screen's origin) and the square root:
page 0  1/w falls along x, DCalcCtrl 1      page 1  along y, DCalcCtrl 1      page 2  flat, DCalcCtrl 1
page 3  the texture turned by 0 .. 90 degrees (ratio 1.5): both derivatives of a direction are there
page 4  the same, DCalcCtrl 1               page 5  u and v both along x, v's share 0 .. 1
page 6  one triangle with 1/w falling along x, each of its corners sent first in turn, DCalcCtrl 1
page 7  random bias, 1/w and DCalcCtrl

--mix det: 1/w = 1, right triangles with legs of a and b pixels at the cell's top left corner instead of
quads, so that the setup's determinant a * b is not a power of two; the ratio steps through 1.5 .. 1.5 * 1.0176
in 18 steps for each size, which brackets the level value finely.
page 0  legs (32, b), b = 17 .. 32      page 1  (a, 32)      page 2  (b, b)      page 3  (24, b)
page 4  (32, b) with bias 5/4           page 5  (b, 20)      page 6  (17 .. 32, 17 .. 32) random
page 7  (32, b), ratio around 1.25

--mix wext: the level at depths that are not ordinary (flat 1/w; pass B and bias 4/4 unless said). With 1/w = z
the level comes from products like z * z, which leave the range of the TSP's numbers long before z does.
page 0  ratio 3, z = 2^(c - 126) for cells 0-253, then denormals, the largest numbers, infinity, NaN, zero and
        numbers below zero                       page 1  the same, pass A
page 2  ratio 1.5, z = 1.5 * 2^(c - 126)         page 3  ratio 6, z = 2^(c - 126)
page 4  ratio 3, z = -2^(c - 126)                page 5  ratio 3, bias 8/4, z = 2^(c - 126)
page 6  ratio 1 .. 8, z of any exponent with a 16 bit mantissa
page 7  ratio 3, z with 24 bit mantissas at 2^-70 .. 2^-58 (cells 0-149) and 2^58 .. 2^70
"""
import argparse
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
PAGES = 8


def case(su, sv, bias, mode, z, group):
    """The quad's corners TL, TR, BL, BR: (u, v) with the texture 64 texels wide. z: 1/w, one for all corners or four."""
    su, sv = float(su), float(sv)
    z = [float(z)] * 4 if np.isscalar(z) else [float(v) for v in z]
    return {"su": su, "sv": sv, "bias": bias, "mode": mode, "z": z, "group": group,
            "uv": [[0.0, 0.0], [su / 2, 0.0], [0.0, sv / 2], [su / 2, sv / 2]]}


def pages_first(rng):
    pages = []
    for p in range(PAGES):
        cells = []
        for c in range(300):
            lin, geo = 1 + c / 300, 2.0 ** (c / 50)
            if p == 0: q = case(lin, lin, 4, 3, 1.0, "linear")
            elif p == 1: q = case(lin, lin, 4, 2, 1.0, "linear pass A")
            elif p == 2: q = case(geo, geo, 4, 3, 1.0, "levels")
            elif p == 3: q = case(lin, lin, 5, 3, 1.0, "linear bias 5")
            elif p == 4: q = case(geo / 2, geo / 2, 7, 3, 1.0, "levels bias 7")
            elif p == 5: q = case(lin, lin, 4, 3, 0.75, "linear w")
            elif p == 6:
                b = 1 + (c % 150) / 150
                q = case(b, b / 2, 4, 3, 1.0, "wide") if c < 150 else case(b / 2, b, 4, 3, 1.0, "tall")
            else:
                s = float(rng.integers(1 << 23, 1 << 24)) / (1 << 23) * 2.0 ** int(rng.integers(-1, 3))
                q = case(s, s, 13, 3, 1.0, "random bias 13")
            cells.append(q)
        pages.append(cells)
    return pages


def pages_w(rng):
    m24 = lambda: float(rng.integers(1 << 23, 1 << 24)) / (1 << 23)
    pages = []
    for p in range(PAGES):
        cells = []
        for c in range(300):
            lin = 1 + c / 300
            if p == 0: q = case(1.5, 1.5, 4, 3, 0.5 + c / 600, "w sweep")
            elif p == 1: q = case(lin, lin, 4, 3, m24() * 2.0 ** int(rng.integers(-3, 4)), "w 24 bit")
            elif p == 2: q = case(lin, lin, 4, 3, 2.0 ** (c % 13 - 6), "w power of two")
            elif p == 3: q = case(1.25, 1.25, 4, 3, float(rng.integers(1 << 15, 1 << 16)) / (1 << 15) * 2.0 ** int(rng.integers(-2, 3)), "w 16 bit")
            elif p == 4: q = case(lin, lin, 4, 3, [1.0, 0.5, 1.0, 0.5], "w falls along x")
            elif p == 5: q = case(lin, lin, 4, 3, [1.0, 1.0, 0.5, 0.5], "w falls along y")
            elif p == 6: q = case(1.5, 1.5, 4, 3, (512 + c) / 512, "w 9 bit steps")
            else: q = case(m24() * 2.0 ** int(rng.integers(0, 3)), m24(), int(rng.integers(1, 16)), 3, m24() * 2.0 ** int(rng.integers(-2, 3)), "random")
            cells.append(q)
        pages.append(cells)
    return pages


def pages_det(rng):
    pages = []
    for p in range(PAGES):
        cells = []
        for c in range(300):
            b = 17 + c % 16
            s = (1.25 if p == 7 else 1.5) * (1 + (c // 16) / 1024)
            legs = [(32, b), (b, 32), (b, b), (24, b), (32, b), (b, 20), (int(rng.integers(17, 33)), int(rng.integers(17, 33))), (32, b)][p]
            q = case(s, s, 5 if p == 4 else 4, 3, 1.0, "det")
            q["legs"] = list(legs)
            q["uv"] = [[0.0, 0.0], [s * legs[0] / 64, 0.0], [0.0, s * legs[1] / 64], [0.0, 0.0]]
            cells.append(q)
        pages.append(cells)
    return pages


DET2 = [  # (corners in pixels from the cell's corner, what is special)
    ([(0, 0), (32, 0), (0, 32)], "32x32"), ([(0, 0), (16, 0), (0, 16)], "16x16"), ([(0, 0), (8, 0), (0, 8)], "8x8"),
    ([(0, 0), (32, 0), (0, 16)], "32x16"), ([(0, 0), (16, 0), (0, 32)], "16x32"), ([(0, 0), (0, 32), (32, 0)], "32x32 other winding"),
    ([(0, 0), (32, 8), (0, 32)], "slanted, det 1024"), ([(0, 0), (32, 0), (8, 32)], "slanted 2, det 1024"), ([(0, 0), (30, 0), (0, 30)], "30x30"),
    ([(0, 0), (32, 0), (0, 31)], "32x31"), ([(0, 0), (32, 0), (0, 32)], "32x32 u + 0.25"), ([(0, 0), (24, 0), (0, 24)], "24x24"),
    ([(0, 0), (32, 0), (0, 32)], "32x32 u and v swapped"), ([(0, 0), (32, 0), (0, 32)], "32x32 v constant"), ([(0, 0), (20, 0), (0, 20)], "20x20"),
    ([(0, 0), (32, 4), (4, 32)], "det 1008"), ([(0, 0), (32, 16), (0, 32)], "slanted 3, det 1024"), ([(0, 0), (16, 0), (0, 8)], "16x8"),
    ([(0, 0), (32, 0), (0, 32)], "32x32 1/w 0.5"), ([(0, 0), (12, 0), (0, 12)], "12x12"),
]


def pages_det2(rng):
    """Triangles of 20 kinds (one per column); the ratio rises by 1/1024 per row from a start that differs by page."""
    pages = []
    for p in range(PAGES):
        cells = []
        for c in range(300):
            tri, name = DET2[c % 20]
            s = (1.5, 1.25, 1.75, 1.0625, 3.0, 1.5, 1.9, 1.5)[p] * (1 + (c // 20) / 1024)
            bias, z = (5 if p == 5 else 4), (0.5 if name == "32x32 1/w 0.5" else 1.0)
            q = case(s, s, bias, 2 if p == 7 else 3, z, name)
            q["tri"] = [list(v) for v in tri]
            uv = [[s * x / 64, s * y / 64] for x, y in tri]
            if name == "32x32 u + 0.25": uv = [[u + 0.25, v] for u, v in uv]
            if name == "32x32 u and v swapped": uv = [[v, u] for u, v in uv]
            if name == "32x32 v constant": uv = [[u, 0.0] for u, v in uv]
            q["uv"] = uv + [[0.0, 0.0]]
            cells.append(q)
        pages.append(cells)
    return pages


def pages_dcalc(rng):
    """DCalcCtrl 1, and the square root: see the top of the file."""
    import math
    corners = ((0, 0), (32, 0), (0, 32), (32, 32))
    pages = []
    for p in range(PAGES):
        cells = []
        for c in range(300):
            lin = 1 + c / 300
            if p == 0: q = case(lin, lin, 4, 3, [1.0, 0.5, 1.0, 0.5], "w falls along x, DCalc 1"); q["dcalc"] = 1
            elif p == 1: q = case(lin, lin, 4, 3, [1.0, 1.0, 0.5, 0.5], "w falls along y, DCalc 1"); q["dcalc"] = 1
            elif p == 2: q = case(lin, lin, 4, 3, 1.0, "flat, DCalc 1"); q["dcalc"] = 1
            elif p in (3, 4):
                t, s = math.radians(90 * c / 300), 1.5
                q = case(s, s, 4, 3, 1.0, "rotated" + (", DCalc 1" if p == 4 else "")); q["dcalc"] = int(p == 4)
                q["uv"] = [[s * (x * math.cos(t) + y * math.sin(t)) / 64, s * (-x * math.sin(t) + y * math.cos(t)) / 64] for x, y in corners]
                q["turn"] = 90 * c / 300
            elif p == 5:
                t, s = c / 300, 1.25
                q = case(s, s, 4, 3, 1.0, "both along x")
                q["uv"] = [[s * x / 64, s * t * x / 64] for x, y in corners]
                q["share"] = t
            elif p == 6:
                order = [[(0, 0), (32, 0), (0, 32)], [(32, 0), (0, 32), (0, 0)], [(0, 32), (0, 0), (32, 0)]][c % 3]
                q = case(lin, lin, 4, 3, 1.0, "first vertex %d, DCalc 1" % (c % 3)); q["dcalc"] = 1
                q["tri"] = [list(v) for v in order]
                q["z"] = [1.0 - x / 64 for x, y in order] + [1.0]
                q["uv"] = [[lin * x / 64, lin * y / 64] for x, y in order] + [[0.0, 0.0]]
            else:
                z = [float(rng.uniform(0.4, 1.0)) for _ in range(2)]
                q = case(lin, lin, int(rng.integers(2, 9)), 3, [z[0], z[1], z[0], z[1]] if c % 2 else [z[0], z[0], z[1], z[1]], "random"); q["dcalc"] = int(rng.integers(0, 2))
            q.setdefault("dcalc", 0)
            cells.append(q)
        pages.append(cells)
    return pages


def pages_wext(rng):
    m24 = lambda: float(rng.integers(1 << 23, 1 << 24)) / (1 << 23)
    f32 = lambda v: float(np.float32(v))
    bits = lambda b: float(np.uint32(b).view(np.float32))
    special = [bits(0x00400000), bits(0x00000001), bits(0x007fffff), bits(0x7f7fffff), float("inf"), float("nan"), 0.0, -0.0, float("-inf"),
               -1.0, f32(-2.0 ** -100), f32(-2.0 ** 100), bits(0x7f400000), bits(0x00800000), bits(0x80800000), bits(0xff7fffff)]
    pages = []
    for p in range(PAGES):
        cells = []
        for c in range(300):
            pw = f32(2.0 ** (c - 126)) if c < 254 else special[(c - 254) % len(special)]
            if p == 0: q = case(3, 3, 4, 3, pw, "wext ratio 3")
            elif p == 1: q = case(3, 3, 4, 2, pw, "wext ratio 3 pass A")
            elif p == 2: q = case(1.5, 1.5, 4, 3, f32(1.5 * pw) if c < 254 else pw, "wext ratio 1.5")
            elif p == 3: q = case(6, 6, 4, 3, pw, "wext ratio 6")
            elif p == 4: q = case(3, 3, 4, 3, -pw if c < 254 else pw, "wext ratio 3, below zero")
            elif p == 5: q = case(3, 3, 8, 3, pw, "wext ratio 3 bias 8")
            elif p == 6: q = case(2.0 ** rng.uniform(0, 3), 2.0 ** rng.uniform(0, 3), 4, 3,
                                  f32(float(rng.integers(1 << 15, 1 << 16)) / (1 << 15) * 2.0 ** int(rng.integers(-126, 128))), "wext random")
            else: q = case(3, 3, 4, 3, f32(m24() * 2.0 ** (int(rng.integers(-70, -57)) if c < 150 else int(rng.integers(58, 71)))), "wext edges")
            cells.append(q)
        pages.append(cells)
    return pages


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--mix", default="first", choices=["first", "w", "det", "det2", "dcalc", "wext"])
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)
    pages = {"w": pages_w, "det": pages_det, "det2": pages_det2, "dcalc": pages_dcalc, "wext": pages_wext}.get(args.mix, pages_first)(rng)
    write(HERE, {"seed": args.seed, "mix": args.mix, "pages": pages})
    print(f"seed {args.seed} mix {args.mix}: {PAGES} pages, {sum(len(p) for p in pages)} quads")


def write(folder, data):
    """cases.h and cases.json from the description of a set (also used to run an archived set again).

    Every float goes into cases.h as the bit pattern of the single precision number: the test program never
    converts or calculates one, so a PC and the console send the same bits."""
    pages = data["pages"]
    fl = lambda v: "0x%08xu" % int(np.float32(v).view(np.uint32))
    lines = ["/* Generated by gen.py - do not edit. Floats are bit patterns. */", "#include <stdint.h>", f"#define NUM_QUADS {sum(len(p) for p in pages)}",
             "typedef struct { unsigned char page, bias, mode, dcalc, tri[3][2]; unsigned short cell; uint32_t z[4]; uint32_t uv[4][2]; } quad_t;   /* corners TL, TR, BL, BR; tri (not all zero): a triangle with these corners, in pixels from the cell's, instead */",
             "static const quad_t quads[NUM_QUADS] = {"]
    for p, cells in enumerate(pages):
        for c, q in enumerate(cells):
            uv = ", ".join("{ %s, %s }" % (fl(u), fl(v)) for u, v in q["uv"])
            z = q["z"] if isinstance(q["z"], list) else [q["z"]] * 4
            tri = q.get("tri") or ([[0, 0], [q["legs"][0], 0], [0, q["legs"][1]]] if q.get("legs") else [[0, 0]] * 3)
            lines.append("    { %d, %d, %d, %d, { %s }, %d, { %s }, { %s } }," % (p, q["bias"], q["mode"], q.get("dcalc", 0), ", ".join("{ %d, %d }" % (v[0], v[1]) for v in tri), c, ", ".join(fl(v) for v in z), uv))
    lines.append("};")
    (folder / "cases.h").write_bytes((chr(10).join(lines) + chr(10)).encode())
    (folder / "cases.json").write_text(json.dumps(data))


if __name__ == "__main__":
    main()
