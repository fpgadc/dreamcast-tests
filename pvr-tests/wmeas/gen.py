"""Cases for wmeas: the 1/w the TSP computes, read at every pixel of general triangles.

    python gen.py [--seed N] [--mix general|flatedge]        ->  cases.h, cases.json

Every polygon is black with table fog; every fog table pair is (0, 255) and the fog color is white, so the
gray level of a pixel shows bits 11..18 of the mantissa of the 1/w the TSP computed there (see ../tspw).
All the pixels well inside a triangle are data: about 300,000 readings per run.

Pages 0-5: one small triangle in each 32x32 cell (integer, 1/16 and float corners; flat to steep depth).
Pages 6-7: triangles of any size drawn over each other, largest first (corners up to 1500 pixels off screen).
Depths stay inside 1.1 .. 250 so that the fog table index does not clamp.

--mix flatedge: flat polygons only, each at a depth 0 .. 2.5 units of the 16th mantissa bit above a step of
the reading (in steps of the 24th bit), to see by how much the 1/w of the TSP falls short of the depth. The
geometry of a cell is the same on every page; only the depth changes. Cells 0-99: the upper triangle of the
quad over the whole cell.
--mix slopes: quad triangles with exactly known slopes and a step of the reading through the middle.
--mix flatscan: the same, but a cell keeps its step and its depth walks through it in 8 steps of 6 units of
the 24th bit: from 20 below the step (even cells) or from 236 above it (odd cells).
"""
import argparse
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
PAGES = 8
f32 = lambda a: np.asarray(a, dtype=np.float32).astype(np.float64)


def depths(rng, kind):
    while True:
        z0 = float(np.exp(rng.uniform(np.log(1.5), np.log(200.0))))
        if kind == "flat":
            z = np.full(3, z0)
        elif kind == "gentle":
            z = z0 * (1.0 + rng.uniform(-1, 1, 3) * 2.0 ** -rng.integers(8, 15))
        elif kind == "medium":
            z = z0 * (1.0 + rng.uniform(-1, 1, 3) * 2.0 ** -rng.integers(3, 8))
        elif kind == "wide":
            z = z0 * 2.0 ** rng.uniform(-6, 0, 3)
        else:
            z = z0 * rng.uniform(0.25, 1.0, 3)
        if z.min() > 1.1 and z.max() < 250:
            return f32(z)


def snap(rng, pts, kind):
    if kind == "int":
        return np.round(pts)
    if kind == "16":
        return np.round(pts * 16) / 16
    return pts


def cells_page(rng):
    tris = []
    for cell in range(300):
        cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
        ck = rng.choice(["int", "16", "float"], p=[0.25, 0.25, 0.5])
        dk = rng.choice(["flat", "gentle", "medium", "steep", "wide"], p=[0.15, 0.2, 0.2, 0.3, 0.15])
        while True:
            pts = snap(rng, rng.uniform(0.0, 32.0, (3, 2)), ck)
            x, y = f32(cx + pts[:, 0]), f32(cy + pts[:, 1])
            if abs((x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])) > 250:
                break
        tris.append({"x": x.tolist(), "y": y.tolist(), "z": depths(rng, dk).tolist(), "group": f"cell: {ck} corners, {dk} depth"})
    return tris


def scene_page(rng):
    classes = [("huge", 10, 1500.0), ("large", 20, 400.0), ("medium", 35, 150.0), ("small", 45, 60.0)]
    load = np.zeros((15, 20), int)
    tris = []
    for cls, count, h in classes:
        made = 0
        while made < count:
            c = np.array([rng.uniform(0, 640), rng.uniform(0, 480)])
            ck = rng.choice(["int", "16", "float"], p=[0.2, 0.2, 0.6])
            pts = snap(rng, c + rng.uniform(-h, h, (3, 2)), ck)
            x, y = f32(pts[:, 0]), f32(pts[:, 1])
            if abs((x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])) < h * h / 3:
                continue
            if x.max() < 0 or y.max() < 0 or x.min() >= 640 or y.min() >= 480:
                continue
            tx0, tx1 = int(np.clip(np.floor(x.min() / 32), 0, 19)), int(np.clip(np.floor(x.max() / 32), 0, 19))
            ty0, ty1 = int(np.clip(np.floor(y.min() / 32), 0, 14)), int(np.clip(np.floor(y.max() / 32), 0, 14))
            if load[ty0:ty1 + 1, tx0:tx1 + 1].max() >= 40:
                continue
            load[ty0:ty1 + 1, tx0:tx1 + 1] += 1
            dk = rng.choice(["flat", "gentle", "medium", "steep", "wide"], p=[0.15, 0.2, 0.2, 0.3, 0.15])
            tris.append({"x": x.tolist(), "y": y.tolist(), "z": depths(rng, dk).tolist(), "group": f"scene {cls}: {ck} corners, {dk} depth"})
            made += 1
    return tris


def flatedge_pages(rng, scan=False):
    geo = []
    for cell in range(300):
        cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
        if cell < 100:
            geo.append((np.array([cx, cx + 32, cx]), np.array([cy, cy, cy + 32]), "quad"))
            continue
        ck = rng.choice(["int", "16", "float"], p=[0.3, 0.3, 0.4])
        while True:
            pts = snap(rng, rng.uniform(0.0, 32.0, (3, 2)), ck)
            x, y = f32(cx + pts[:, 0]), f32(cy + pts[:, 1])
            if abs((x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])) > 250:
                break
        geo.append((x, y, ck))
    pages = []
    fixed = [(int(rng.integers(8, 4088)), int(rng.integers(1, 8)), int(rng.integers(0, 6))) for _ in geo]
    for p in range(PAGES):
        tris = []
        for n, (x, y, ck) in enumerate(geo):
            # a step of the reading: a multiple of 2^11 in the 24 bit mantissa; then k units of the 24th bit above it
            j = int(rng.integers(8, 4088))
            e = int(rng.integers(1, 8))
            k = int(rng.integers(-40, 640))
            if scan:        # the same step on every page; k walks through 0 (even cells) or through 256 (odd cells)
                j, e, r = fixed[n]
                k = (-20 if n % 2 == 0 else 236) + 6 * p + r
            z = float(np.ldexp((4096 + j) * 2048 + k, e - 24))
            assert z == float(np.float32(z))
            tris.append({"x": x.tolist(), "y": y.tolist(), "z": [z, z, z], "group": f"cell: {ck} corners, flat depth", "k": k})
        pages.append(tris)
    return pages


def slopes_pages(rng):
    """The upper triangle of the quad over each cell, with exactly known slopes: the depth differences along x
    and y are a small integer times a power of two, from 2^-22 to 2^-9 of the depth. The depth at the first
    corner is chosen so that a step of the reading runs through the middle of the triangle: where exactly the
    reading changes shows how the TSP adds A x + B y + C."""
    pages = []
    for p in range(PAGES):
        tris = []
        for cell in range(300):
            cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
            while True:
                e = int(rng.integers(1, 8))
                j = int(rng.integers(8, 4088))
                step = float(np.ldexp((4096 + j) * 2048, e - 24))
                unit = float(np.ldexp(1.0, e - 24))                       # the 24th mantissa bit of the depth
                qx, qy = int(rng.integers(2, 16)), int(rng.integers(2, 16))
                dzx = float(rng.choice([-1, 1]) * int(rng.integers(1, 8)) * unit * 2.0 ** qx) if rng.uniform() > 0.1 else 0.0
                dzy = float(rng.choice([-1, 1]) * int(rng.integers(1, 8)) * unit * 2.0 ** qy) if rng.uniform() > 0.1 else 0.0
                # the plane is step at about a third into the triangle
                gx, gy = float(rng.integers(6, 14)) + 0.5, float(rng.integers(6, 14)) + 0.5
                z1 = float(np.float32(step - dzx * gx / 32 - dzy * gy / 32))
                z = np.array([z1, z1 + dzx, z1 + dzy])
                if (f32(z) == z).all() and z.min() > 1.1 and z.max() < 250:
                    break
            tris.append({"x": [cx, cx + 32, cx], "y": [cy, cy, cy + 32], "z": z.tolist(), "group": "cell: quad corners, slopes depth",
                         "dzx": dzx / unit, "dzy": dzy / unit, "step": step})
        pages.append(tris)
    return pages


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--mix", default="general", choices=["general", "flatedge", "flatscan", "slopes"])
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)
    if args.mix == "slopes":
        pages = slopes_pages(rng)
    elif args.mix in ("flatedge", "flatscan"):
        pages = flatedge_pages(rng, args.mix == "flatscan")
    else:
        pages = [cells_page(rng) if p < 6 else scene_page(rng) for p in range(PAGES)]
    write(HERE, {"seed": args.seed, "mix": args.mix, "pages": pages})
    print(f"seed {args.seed}: {PAGES} pages, {sum(len(p) for p in pages)} triangles")


def write(folder, data):
    """cases.h and cases.json from the description of a set (also used to run an archived set again).

    Every float goes into cases.h as the bit pattern of the single precision number: the test program never
    converts or calculates one, so a PC and the console send the same bits."""
    pages = data["pages"]
    fl = lambda v: "0x%08xu" % int(np.float32(v).view(np.uint32))
    lines = ["/* Generated by gen.py - do not edit. Floats are bit patterns. */", "#include <stdint.h>", f"#define NUM_TRIS {sum(len(p) for p in pages)}",
             "typedef struct { unsigned char page; uint32_t v[3][3]; } tri_t;   /* x, y, z */", "static const tri_t tris[NUM_TRIS] = {"]
    for p, tris in enumerate(pages):
        for t in tris:
            v = ", ".join("{ " + ", ".join(fl(q) for q in (t["x"][k], t["y"][k], t["z"][k])) + " }" for k in range(3))
            lines.append("    { %d, { %s } }," % (p, v))
    lines.append("};")
    (folder / "cases.h").write_bytes((chr(10).join(lines) + chr(10)).encode())
    (folder / "cases.json").write_text(json.dumps(data))


if __name__ == "__main__":
    main()
