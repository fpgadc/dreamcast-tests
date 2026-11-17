"""Collect the wmeas readings into one file.

    python read.py [folder, default /tmp/pvr_tests/wmeas] [hw | emu] [out.npz]

For every page: which triangle is on top at each pixel, and whether the pixel is at least 1.5 pixels inside it
and not within 1.5 pixels of an edge of a triangle drawn later. Those pixels are the data:
    tx, ty, tz   (T, 3)  the triangles (rasterizer order)        group (T,) text
    ti, px, py, g        triangle index, pixel center, gray level read
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).parent
MARGIN = 1.5


def load(d, page, kind):
    if kind in ("hw", "off"):       # a console's dumps, or those of refsw2-offline (run2.sh)
        a = np.fromfile(d / f"page{page}.{kind}.raw", dtype="<u4").reshape(480, 640)
        return ((a >> 8) & 255).astype(int)
    return np.asarray(Image.open(d / f"page{page}.emu.png").convert("RGB")).astype(int)[:, :, 1]


def main():
    args = [a for a in sys.argv[1:] if a not in ("hw", "emu", "off")]
    kind = "emu" if "emu" in sys.argv else "off" if "off" in sys.argv else "hw"
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/wmeas")
    out = Path(args[1]) if len(args) > 1 else d / ("wmeas.npz" if kind == "hw" else f"wmeas.{kind}.npz")
    # the run's own tables (run2.sh <test>@<set>), else the ones in the test's folder
    pages = json.loads(((d / "cases.json") if (d / "cases.json").exists() else (HERE / "cases.json")).read_text())["pages"]
    gy, gx = np.mgrid[0:480, 0:640]
    gx, gy = gx + 0.5, gy + 0.5
    TX, TY, TZ, GRP, TI, PX, PY, G = [], [], [], [], [], [], [], []
    for p, tris in enumerate(pages):
        im = load(d, p, kind)
        owner = np.full((480, 640), -1)
        clean = np.zeros((480, 640), bool)
        for t in tris:
            x, y = np.array(t["x"]), np.array(t["y"])
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            dist = np.full((480, 640), np.inf)
            for i in range(3):
                j = (i + 1) % 3
                ex, ey = x[j] - x[i], y[j] - y[i]
                dist = np.minimum(dist, (ex * (gy - y[i]) - ey * (gx - x[i])) / np.hypot(ex, ey) * np.sign(area))
            inside = dist > MARGIN
            owner[inside] = len(TX)
            clean[inside] = True
            clean[(dist > -MARGIN) & ~inside] = False
            TX.append(t["x"]); TY.append(t["y"]); TZ.append(t["z"]); GRP.append(t["group"])
        rows, cols = np.nonzero(clean)
        TI.append(owner[rows, cols]); PX.append(cols + 0.5); PY.append(rows + 0.5); G.append(im[rows, cols])
    np.savez_compressed(out, tx=np.array(TX), ty=np.array(TY), tz=np.array(TZ), group=np.array(GRP),
                        ti=np.concatenate(TI), px=np.concatenate(PX), py=np.concatenate(PY), g=np.concatenate(G))
    print(f"{kind}: {len(TX)} triangles, {sum(len(t) for t in TI)} readings -> {out}")


if __name__ == "__main__":
    main()
