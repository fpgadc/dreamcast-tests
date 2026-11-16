"""Collect the mipmeas dumps.

    python3 read.py [folder, default /tmp/pvr_tests/mipmeas] [hw | off] [out.npz]

hw: a console's dumps; off: the dumps of refsw2-offline (run2.sh). Data, one row per quad: su, sv, bias, mode,
z (1/w of the four corners), rgb (24 x 24 pixels from the middle of the cell, 3 channels) and corner (6 x 6
pixels at (2..7, 2..7) of the cell: inside the triangles of the det mix) and tip (2 x 2 pixels at (1..2, 1..2)).
"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent


def load(d, page, kind):
    a = np.fromfile(d / f"page{page}.{kind}.raw", dtype="<u4").reshape(480, 640)
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(np.uint8)


def main():
    args = [a for a in sys.argv[1:] if a not in ("hw", "off")]
    kind = "off" if "off" in sys.argv else "hw"
    d = Path(args[0]) if args else Path("/tmp/pvr_tests/mipmeas")
    out = Path(args[1]) if len(args) > 1 else d / ("mipmeas.npz" if kind == "hw" else f"mipmeas.{kind}.npz")
    # the run's own tables (run2.sh mipmeas@<set>), else the ones in the test's folder
    pages = json.loads(((d / "cases.json") if (d / "cases.json").exists() else (HERE / "cases.json")).read_text())["pages"]
    rows, rgb, corner, tip = [], [], [], []
    for p, cells in enumerate(pages):
        im = load(d, p, kind)
        for c, q in enumerate(cells):
            x0, y0 = (c % 20) * 32, (c // 20) * 32
            rgb.append(im[y0 + 4:y0 + 28, x0 + 4:x0 + 28].reshape(-1, 3))
            corner.append(im[y0 + 2:y0 + 8, x0 + 2:x0 + 8].reshape(-1, 3))
            tip.append(im[y0 + 1:y0 + 3, x0 + 1:x0 + 3].reshape(-1, 3))
            z = q["z"] if isinstance(q["z"], list) else [q["z"]] * 4
            rows.append((q["su"], q["sv"], q["bias"], q["mode"], *z))
    r = np.array(rows)
    np.savez_compressed(out, su=r[:, 0], sv=r[:, 1], bias=r[:, 2], mode=r[:, 3], z=r[:, 4:8], rgb=np.stack(rgb), corner=np.stack(corner), tip=np.stack(tip))
    flat = sum(len(np.unique(q, axis=0)) == 1 for q in rgb)
    print(f"{kind}: {len(rows)} quads, {flat} of them one colour all over -> {out}")


if __name__ == "__main__":
    main()
