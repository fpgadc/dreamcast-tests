"""Collect the covmeas dumps.

    python read.py [folder, default /tmp/pvr_tests/covmeas] [hw | emu | off] [out.npz]

hw: a console's dumps; emu: the emulator's screenshots; off: the dumps of refsw2-offline (run2.sh).

Data: cov (pages, 480, 640) whether the pixel was drawn; tx, ty (T, 3) the triangles in the order submitted;
page, group, dir (T, 2), p0 (T, 2), edge (T, 2: the two vertices of the lattice edge).
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).parent


def load(d, page, kind):
    if kind in ("hw", "off"):
        a = np.fromfile(d / f"page{page}.{kind}.raw", dtype="<u4").reshape(480, 640)
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
    return np.asarray(Image.open(d / f"page{page}.emu.png").convert("RGB")).astype(int)


def main():
    args = [a for a in sys.argv[1:] if a not in ("hw", "emu", "off")]
    kind = "emu" if "emu" in sys.argv else "off" if "off" in sys.argv else "hw"
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/covmeas")
    out = Path(args[1]) if len(args) > 1 else d / ("covmeas.npz" if kind == "hw" else f"covmeas.{kind}.npz")
    # the run's own tables (run2.sh covmeas@<set>), else the ones in the test's folder
    data = json.loads(((d / "cases.json") if (d / "cases.json").exists() else (HERE / "cases.json")).read_text())
    pages = data["pages"]
    prim = data.get("prim", "tri")
    if prim == "modvol":        # white where nothing covers, half as bright under a volume
        cov = np.stack([load(d, p, kind).max(axis=-1) < 192 for p in range(len(pages))])
    else:       # anything that is not the black background: a sliver's planes are garbage and so is its color
        cov = np.stack([load(d, p, kind).max(axis=-1) > 0 for p in range(len(pages))])
    extra = {}
    if prim == "zprobe":        # red: drawn; green: not covered by the ISP; black: covered, not shaded
        ims = [load(d, p, kind) for p in range(len(pages))]
        cov = np.stack([(im[..., 0] > 128) & (im[..., 1] < 128) for im in ims])
        extra["isp"] = np.stack([im[..., 1] < 128 for im in ims])
    tris = [(p, t) for p, ts in enumerate(pages) for t in ts]
    np.savez_compressed(out, cov=cov, **extra, tx=np.array([t["x"] for _, t in tris]), ty=np.array([t["y"] for _, t in tris]),
                        prim=prim, list=data.get("list", "op"), halfofs=data.get("halfofs", 7),
                        page=np.array([p for p, _ in tris]), group=np.array([t["group"] for _, t in tris]),
                        dir=np.array([t["dir"] for _, t in tris]), p0=np.array([t["p0"] for _, t in tris]), edge=np.array([t["edge"] for _, t in tris]))
    print(f"{kind}: {len(tris)} triangles, {int(cov.sum())} pixels drawn -> {out}")


if __name__ == "__main__":
    main()
