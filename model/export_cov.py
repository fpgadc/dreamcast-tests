"""Archived covmeas runs (fit/covmeas_data/<set>.npz + .cases.json) as flat binary files for the C++ probes.

    python export_cov.py [set ...]        ->  fit/covmeas_data/<set>.cov   (all sets when none is named)

Layout, little endian: "PVRCOV2\0"; int32 prim (0 triangle, 1 strip of two, 2 sprite, 3 modifier volume, 4 depth
probe), halfofs, pages, polygons; float32 depth of each corner (3) and of the probe polygon; per polygon int32 page, int32 corners, float32 x[4], float32 y[4]; then
pages * 480 * 640 bytes, 1 where the console drew (for the depth probe: where the ISP covered).
"""
import json
import struct
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
DATA = HERE.parent / "fit" / "covmeas_data"
PRIMS = ["tri", "strip", "sprite", "modvol", "zprobe"]


def export(name):
    z = np.load(DATA / f"{name}.npz")
    meta = json.loads((DATA / f"{name}.cases.json").read_text())
    prim = meta.get("prim", "tri")
    cov = z["isp"] if prim == "zprobe" else z["cov"]
    tx, ty, page = z["tx"].astype(np.float32), z["ty"].astype(np.float32), z["page"]
    zs = [float(v) for v in (meta.get("zs") or ",".join([str(meta.get("z", 1.0))] * 3)).split(",")]
    out = bytearray(b"PVRCOV2\0")
    out += struct.pack("<4i4f", PRIMS.index(prim), int(meta.get("halfofs", 7)), cov.shape[0], len(tx), *zs, float(meta.get("probez", 0.5)))
    n = tx.shape[1]
    for k in range(len(tx)):
        xs = list(tx[k]) + [0.0] * (4 - n)
        ys = list(ty[k]) + [0.0] * (4 - n)
        out += struct.pack("<2i8f", int(page[k]), n, *xs, *ys)
    out += cov.astype(np.uint8).tobytes()
    (DATA / f"{name}.cov").write_bytes(out)
    return len(tx)


if __name__ == "__main__":
    names = sys.argv[1:] or sorted(p.name[:-4] for p in DATA.glob("*.npz"))
    for name in names:
        print(f"{name}: {export(name)} polygons")
