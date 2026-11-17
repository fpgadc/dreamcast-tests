"""Collect the uvmeas readings.

    python read.py [folder, default /tmp/pvr_tests/uvmeas] [hw | emu | off] [out.npz]

hw: a console's dumps; emu: the emulator's screenshots; off: the dumps of refsw2-offline (run2.sh).

Data: tx, ty, tu (T, 3) the triangles in rasterizer order; group (T,); m (T,) the magnitude class;
ti, px, py, t: triangle index, pixel center and the texel number read (0..1023; -1 where the pixel is not a
texel color). Pixels at least 1.5 pixels inside the triangle.
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).parent
MARGIN = 1.5


def load(d, page, kind):
    if kind in ("hw", "off"):
        a = np.fromfile(d / f"page{page}.{kind}.raw", dtype="<u4").reshape(480, 640)
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
    return np.asarray(Image.open(d / f"page{page}.emu.png").convert("RGB")).astype(int)


def main():
    args = [a for a in sys.argv[1:] if a not in ("hw", "emu", "off")]
    kind = "emu" if "emu" in sys.argv else "off" if "off" in sys.argv else "hw"
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/uvmeas")
    out = Path(args[1]) if len(args) > 1 else d / ("uvmeas.npz" if kind == "hw" else f"uvmeas.{kind}.npz")
    # the run's own tables (run2.sh uvmeas@<set>), else the ones in the test's folder
    pages = json.loads(((d / "cases.json") if (d / "cases.json").exists() else (HERE / "cases.json")).read_text())["pages"]
    gy, gx = np.mgrid[0:480, 0:640]
    gx, gy = gx + 0.5, gy + 0.5
    TX, TY, TU, TZ, GRP, MM, TI, PX, PY, TT = [], [], [], [], [], [], [], [], [], []
    TZB = []                    # the depths as bits: a NaN does not survive json or float64
    for p, tris in enumerate(pages):
        im = load(d, p, kind)
        tex = ((im[:, :, 1] >> 2) << 5) | (im[:, :, 2] >> 3)
        tex[(im[:, :, 0] != 0) | ((im[:, :, 1] >> 2) > 31)] = -1
        for t in tris:
            x, y = np.array(t["x"]), np.array(t["y"])
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            # the triangle's bounding box on the screen (a cell's 32x32 for the generated ones; a game frame's
            # triangle, mix dump, can be anywhere and larger than the screen)
            x0, y0 = max(int(np.floor(x.min())), 0), max(int(np.floor(y.min())), 0)
            x1, y1 = min(int(np.ceil(x.max())), 640), min(int(np.ceil(y.max())), 480)
            if x1 - x0 <= 32 and y1 - y0 <= 32:
                x0, y0 = min(x0, 608), min(y0, 448)
                x1, y1 = x0 + 32, y0 + 32
            sx, sy = gx[y0:y1, x0:x1], gy[y0:y1, x0:x1]
            dist = np.full(sx.shape, np.inf)
            for i in range(3):
                j = (i + 1) % 3
                ex, ey = x[j] - x[i], y[j] - y[i]
                dist = np.minimum(dist, (ex * (sy - y[i]) - ey * (sx - x[i])) / np.hypot(ex, ey) * np.sign(area))
            rows, cols = np.nonzero(dist > MARGIN)
            TI.append(np.full(len(rows), len(TX))); PX.append(x0 + cols + 0.5); PY.append(y0 + rows + 0.5); TT.append(tex[y0 + rows, x0 + cols])
            TX.append(t["x"]); TY.append(t["y"]); TU.append(t["u"]); TZ.append(t.get("z", [1.0, 1.0, 1.0])); GRP.append(t["group"]); MM.append(t["m"])
            TZB.append(t["zbits"] if "zbits" in t else [int(np.float32(q).view(np.uint32)) for q in t.get("z", [1.0, 1.0, 1.0])])
    np.savez_compressed(out, tx=np.array(TX), ty=np.array(TY), tu=np.array(TU), tz=np.array(TZ), group=np.array(GRP), m=np.array(MM),
                        tzbits=np.array(TZB, dtype=np.uint32),
                        ti=np.concatenate(TI), px=np.concatenate(PX), py=np.concatenate(PY), t=np.concatenate(TT))
    tt = np.concatenate(TT)
    print(f"{kind}: {len(TX)} triangles, {len(tt)} readings ({int((tt < 0).sum())} not a texel color) -> {out}")


if __name__ == "__main__":
    main()
