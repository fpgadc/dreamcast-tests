"""Cases for uvmeas: plane values of the TSP read through a wrapping texture coordinate.

    python gen.py [--seed N] [--mix first|flatscan|steep|wflat|general|recip|recipscan|reciprow|reciprown]        ->  cases.h, cases.json

The texture is 1024 texels wide and texel i holds the number i; it repeats. With 1/w = 1 the texel a pixel
shows is floor(U * 1024) mod 1024: ten bits of the U the TSP computed there, and which ten depends on how
large U is. U of magnitude 2^m shows the mantissa bits m+1 .. m+10 below the leading one, so any window of
the mantissa can be read (the fog table readout of tspw / wmeas shows one fixed window of 1/w only).

Every polygon is one triangle of the quad over a 32x32 cell, depth 1.0 at all corners, so the plane of U is
known exactly: A = (U2 - U1) / 32, B = (U3 - U1) / 32. "ccw" cells use the corner order TL, BL, TR instead of
TL, TR, BL (the determinant of the setup changes sign).

page 0  U flat, 24 significant bits, magnitude 2^0 .. 2^13          page 1  the same, other winding
page 2  U changes along x only (16 bit start, slope 2^0 .. 2^-20 of it per cell)
page 3  along y only          page 4  both          page 5  both, other winding
page 6  both, slopes with 24 significant bits        page 7  flat, magnitude 2^10 .. 2^22

--mix flatscan: flat U of magnitude 2^6 .. 2^9 (the 16th mantissa bit is visible in the reading); a cell keeps
its 16 bit value and the pages walk U through it in steps of 6 units of the 24th bit, from 21 below. Odd
cells have the other winding.
--mix steep: slopes of 1/8 to 8 times the corner value per cell, so that the plane value on screen is far
from the constant of the plane (its value at the screen origin) in size and often in sign.
--mix wflat: as "first" but with a depth other than 1 (the same at the three corners, 16 or 24 significant
bits): the premultiply of U by 1/w at the corners and the reciprocal of a 1/w that is not a power of two.
--mix general: any small triangle inside the cell, depth 1, three unrelated U values: the setup of an
attribute plane on general triangles (the planes of the other mixes have coefficients with few bits).
--mix recip: flat depth with a 16 bit mantissa D and U = 128 or 256. Then U / w and 1 / w are both exactly
representable planes, the first is the second times a power of two, and the U delivered is D times the
reciprocal the TSP has for D: the reciprocal function, read for 2400 values of D per run (cells 150-299 of
a page have consecutive values of D).
--mix wexp: the U of "first", with a flat depth D * 2^e whose exponent runs through all of single precision:
cell c has e = c - 126 (2^-126 .. 2^127), cells 254 .. 299 are denormals, the largest denormal, the largest
finite number, infinity, NaN and zero. D = 1 on pages 0-3, 16 significant bits on 4-5, 24 on 6-7. Where does
the TSP's division by 1/w stop working?
"""
import argparse
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
PAGES = 8
f32 = lambda a: np.asarray(a, dtype=np.float32).astype(np.float64)


def mant(rng, bits):
    """A random number in [1, 2) with `bits` significant bits."""
    return float(rng.integers(1 << (bits - 1), 1 << bits)) / (1 << (bits - 1))


def page(rng, p):
    tris = []
    for cell in range(300):
        cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
        ccw = p in (1, 5)
        m = int(rng.integers(0, 14)) if p != 7 else int(rng.integers(10, 23))
        if p in (0, 1, 7):
            u0 = mant(rng, 24) * 2.0 ** m
            du = dv = 0.0
            kind = "flat"
        else:
            u0 = mant(rng, 16) * 2.0 ** m
            bits = 24 if p == 6 else int(rng.integers(1, 9))
            du = float(rng.choice([-1, 1])) * mant(rng, bits) * 2.0 ** (m - int(rng.integers(0, 21))) if p in (2, 4, 5, 6) else 0.0
            dv = float(rng.choice([-1, 1])) * mant(rng, bits) * 2.0 ** (m - int(rng.integers(0, 21))) if p in (3, 4, 5, 6) else 0.0
            kind = {2: "x", 3: "y", 4: "xy", 5: "xy", 6: "xy24"}[p]
        u = f32([u0, u0 + du, u0 + dv])                    # at TL, TR, BL
        if ccw:
            x, y, uu = [cx, cx, cx + 32], [cy, cy + 32, cy], [u[0], u[2], u[1]]
        else:
            x, y, uu = [cx, cx + 32, cx], [cy, cy, cy + 32], [u[0], u[1], u[2]]
        tris.append({"x": x, "y": y, "u": [float(t) for t in uu], "group": f"{kind} {'ccw' if ccw else 'cw'}", "m": m})
    return tris


def tri_of(cell, u, ccw, group, m):
    cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
    if ccw:
        x, y, uu = [cx, cx, cx + 32], [cy, cy + 32, cy], [u[0], u[2], u[1]]
    else:
        x, y, uu = [cx, cx + 32, cx], [cy, cy, cy + 32], [u[0], u[1], u[2]]
    return {"x": x, "y": y, "u": [float(t) for t in uu], "group": f"{group} {'ccw' if ccw else 'cw'}", "m": m}


def flatscan_pages(rng):
    fixed = [(int(rng.integers(1 << 15, 1 << 16)), int(rng.integers(6, 10)), int(rng.integers(0, 6))) for _ in range(300)]
    pages = []
    for p in range(PAGES):
        tris = []
        for cell, (g, m, r) in enumerate(fixed):
            k = -21 + 6 * p + r
            u = float(np.ldexp(g * 256 + k, m + 1 - 24))          # g: the 16 bit mantissa; k units of the 24th bit above it
            assert u == float(np.float32(u))
            t = tri_of(cell, [u, u, u], cell % 2 == 1, "flat", m)
            t["k"] = k
            tris.append(t)
        pages.append(tris)
    return pages


def steep_pages(rng):
    pages = []
    for p in range(PAGES):
        tris = []
        for cell in range(300):
            m = int(rng.integers(4, 10))
            bits = 24 if p >= 6 else int(rng.integers(1, 9))
            while True:
                u0 = mant(rng, 16) * 2.0 ** m
                du = float(rng.choice([-1, 1])) * mant(rng, bits) * 2.0 ** (m + int(rng.integers(-3, 4))) if p % 3 != 1 else 0.0
                dv = float(rng.choice([-1, 1])) * mant(rng, bits) * 2.0 ** (m + int(rng.integers(-3, 4))) if p % 3 != 0 else 0.0
                u = f32([u0, u0 + du, u0 + dv])
                if np.abs(u).max() < 16000:
                    break
            tris.append(tri_of(cell, u, p in (4, 5, 7), "steep", m))
        pages.append(tris)
    return pages


def wflat_pages(rng):
    pages = []
    for p in range(PAGES):
        tris = page(rng, p if p < 7 else 4)
        for t in tris:
            bits = 16 if rng.uniform() < 0.5 else 24
            z = float(np.float32(mant(rng, bits) * 2.0 ** int(rng.integers(-6, 8))))
            t["z"] = [z, z, z]
            t["group"] = "w" + t["group"]
        pages.append(tris)
    return pages


WEXP_SPECIAL = ([("2^%d" % k, 2.0 ** k) for k in range(-127, -150, -1)] + [("largest denormal", float(np.uint32(0x007fffff).view(np.float32))),
                ("largest finite", float(np.uint32(0x7f7fffff).view(np.float32))), ("infinity", float("inf")), ("NaN", float("nan")), ("0", 0.0), ("-0", -0.0)])


def wexp_pages(rng):
    pages = []
    for p in range(PAGES):
        tris = page(rng, (0, 1, 2, 4, 0, 4, 6, 7)[p])
        for cell, t in enumerate(tris):
            D = 1.0 if p < 4 else mant(rng, 16 if p < 6 else 24)
            if cell < 254:
                name, z = "2^%d" % (cell - 126), float(np.float32(D * 2.0 ** (cell - 126)))
            else:
                name, z = WEXP_SPECIAL[(cell - 254) % len(WEXP_SPECIAL)]
            t["z"] = [z, z, z]
            t["group"] = "wexp " + t["group"]
            t["zname"] = name
        pages.append(tris)
    return pages


def recip_pages(rng, scan=False, first=None):
    """scan: 2400 consecutive values of D, from a random start (--seed picks it)."""
    pages = []
    start = int(rng.integers(32768, 65536 - 2400)) if first is None else first
    for p in range(PAGES):
        base = int(rng.integers(32768, 65536 - 150))
        tris = []
        for cell in range(300):
            D = int(rng.integers(32768, 65536)) if cell < 150 else base + cell - 150
            if scan:
                D = min(start + p * 300 + cell, 65535)
            z = float(np.ldexp(D, int(rng.integers(-4, 7)) - 16))
            j = int(rng.integers(7, 9))
            u = float(2.0 ** j)
            t = tri_of(cell, [u, u, u], False, "recip", j)
            t["z"] = [z, z, z]
            t["D"] = D
            tris.append(t)
        pages.append(tris)
    return pages


def reciprow_pages(rng, neg=False):
    """300 consecutive divisors (one per cell, the same on every page), each page with another 16 bit U of
    64 .. 256: eight products of the same reciprocal, which pin its value down."""
    start = int(rng.integers(32768, 65536 - 300))
    es = rng.integers(-4, 7, 300)
    pages = []
    for p in range(PAGES):
        tris = []
        for cell in range(300):
            D = start + cell
            z = float(np.ldexp(D, int(es[cell]) - 16))
            j = int(rng.integers(6, 8))
            u = float(mant(rng, 16) * 2.0 ** j) * (-1.0 if neg else 1.0)
            t = tri_of(cell, [u, u, u], False, "reciprow", j)
            t["z"] = [z, z, z]
            t["D"] = D
            tris.append(t)
        pages.append(tris)
    return pages


def general_pages(rng):
    pages = []
    for p in range(PAGES):
        tris = []
        for cell in range(300):
            cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
            ck = rng.choice(["int", "16", "float"], p=[0.25, 0.25, 0.5])
            while True:
                pts = rng.uniform(0.0, 32.0, (3, 2))
                pts = np.round(pts) if ck == "int" else np.round(pts * 16) / 16 if ck == "16" else pts
                x, y = f32(cx + pts[:, 0]), f32(cy + pts[:, 1])
                if abs((x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])) > 250:
                    break
            m = int(rng.integers(6, 10))
            u0 = mant(rng, 24) * 2.0 ** m
            dk = rng.choice(["flat", "gentle", "medium", "steep"], p=[0.2, 0.3, 0.3, 0.2])
            rel = {"flat": 0.0, "gentle": 2.0 ** -int(rng.integers(8, 15)), "medium": 2.0 ** -int(rng.integers(3, 8)), "steep": 0.5}[dk]
            u = f32(u0 * (1.0 + rng.uniform(-1, 1, 3) * rel))
            tris.append({"x": x.tolist(), "y": y.tolist(), "u": u.tolist(), "group": f"general {ck} {dk}", "m": m})
        pages.append(tris)
    return pages


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--start", type=int, default=None, help="recipscan: the first divisor")
    ap.add_argument("--mix", default="first", choices=["first", "flatscan", "steep", "wflat", "general", "recip", "recipscan", "reciprow", "reciprown", "wexp"])
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)
    pages = {"flatscan": flatscan_pages, "steep": steep_pages, "wflat": wflat_pages, "general": general_pages, "recip": recip_pages, "recipscan": lambda r: recip_pages(r, True, args.start), "reciprow": reciprow_pages, "reciprown": lambda r: reciprow_pages(r, True), "wexp": wexp_pages}.get(args.mix, lambda r: [page(r, p) for p in range(PAGES)])(rng)
    for tris in pages:
        for t in tris:
            t.setdefault("z", [1.0, 1.0, 1.0])
    write(HERE, {"seed": args.seed, "mix": args.mix, "pages": pages})
    print(f"seed {args.seed}: {PAGES} pages, {sum(len(p) for p in pages)} triangles")


def write(folder, data):
    """cases.h and cases.json from the description of a set (also used to run an archived set again).

    Every float goes into cases.h as the bit pattern of the single precision number: the test program never
    converts or calculates one, so a PC and the console send the same bits."""
    pages = data["pages"]
    fl = lambda v: "0x%08xu" % int(np.float32(v).view(np.uint32))
    lines = ["/* Generated by gen.py - do not edit. Floats are bit patterns. */", "#include <stdint.h>",
             f"#define NUM_TRIS {sum(len(p) for p in pages)}",
             "typedef struct { unsigned char page; uint32_t v[3][4]; } tri_t;   /* x, y, u, z */", "static const tri_t tris[NUM_TRIS] = {"]
    for p, tris in enumerate(pages):
        for t in tris:
            z = t.get("z", [1.0, 1.0, 1.0])
            v = ", ".join("{ " + ", ".join(fl(q) for q in (t["x"][k], t["y"][k], t["u"][k], z[k])) + " }" for k in range(3))
            lines.append("    { %d, { %s } }," % (p, v))
    lines.append("};")
    (folder / "cases.h").write_bytes((chr(10).join(lines) + chr(10)).encode())
    (folder / "cases.json").write_text(json.dumps(data))


if __name__ == "__main__":
    main()
