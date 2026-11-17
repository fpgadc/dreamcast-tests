"""Collect the cmeas readings.

    python read.py [folder, default /tmp/pvr_tests/cmeas] [hw | emu] [out.npz]

Data: tx, ty (T, 3), tc (T, 3, 3) corner colors [corner, channel] in rasterizer order; group (T,);
ti, px, py, rgb (N, 3): triangle index, pixel center, the color read. Pixels at least 1.5 pixels inside.
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
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
    return np.asarray(Image.open(d / f"page{page}.emu.png").convert("RGB")).astype(int)


def main():
    args = [a for a in sys.argv[1:] if a not in ("hw", "emu", "off")]
    kind = "emu" if "emu" in sys.argv else "off" if "off" in sys.argv else "hw"
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/cmeas")
    out = Path(args[1]) if len(args) > 1 else d / ("cmeas.npz" if kind == "hw" else f"cmeas.{kind}.npz")
    # the run's own tables (run2.sh <test>@<set>), else the ones in the test's folder
    pages = json.loads(((d / "cases.json") if (d / "cases.json").exists() else (HERE / "cases.json")).read_text())["pages"]
    gy, gx = np.mgrid[0:32, 0:32]
    TX, TY, TC, GRP, TI, PX, PY, RGB = [], [], [], [], [], [], [], []
    TZB = []                    # the depths' bits
    for p, tris in enumerate(pages):
        im = load(d, p, kind)
        for t in tris:
            x, y = np.array(t["x"]), np.array(t["y"])
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            x0, y0 = int(x.min()), int(y.min())
            sx, sy = x0 + gx + 0.5, y0 + gy + 0.5
            dist = np.full(sx.shape, np.inf)
            for i in range(3):
                j = (i + 1) % 3
                ex, ey = x[j] - x[i], y[j] - y[i]
                dist = np.minimum(dist, (ex * (sy - y[i]) - ey * (sx - x[i])) / np.hypot(ex, ey) * np.sign(area))
            rows, cols = np.nonzero(dist > MARGIN)
            TI.append(np.full(len(rows), len(TX))); PX.append(x0 + cols + 0.5); PY.append(y0 + rows + 0.5); RGB.append(im[y0 + rows, x0 + cols])
            TX.append(t["x"]); TY.append(t["y"]); TC.append(t["col"]); GRP.append(t["group"])
            TZB.append(t.get("zbits", [0x3f800000] * 3))
    np.savez_compressed(out, tx=np.array(TX), ty=np.array(TY), tc=np.array(TC), group=np.array(GRP), tzbits=np.array(TZB, dtype=np.uint32),
                        ti=np.concatenate(TI), px=np.concatenate(PX), py=np.concatenate(PY), rgb=np.concatenate(RGB))
    print(f"{kind}: {len(TX)} triangles, {sum(len(t) for t in TI)} pixels -> {out}")


if __name__ == "__main__":
    main()
