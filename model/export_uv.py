"""Archived uvmeas / uvspan runs (fit/<test>_data/<set>.npz) as flat binary files for tspprobe.

    python3 export_uv.py [--test uvmeas|uvspan] [set ...]      ->  fit/<test>_data/<set>.uvm   (all sets when none is named)

Layout, little endian: "PVRUVM1\0"; int32 triangles, readings; per triangle float32 x[3], y[3], u[3], z[3];
then for all readings int32 triangle index, int16 pixel x, int16 pixel y, int16 texel number read on the console.
"""
import struct
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent


def export(data, name):
    z = np.load(data / f"{name}.npz")
    tz = z["tz"] if "tz" in z.files else np.ones_like(z["tx"])
    tris = np.concatenate([z["tx"], z["ty"], z["tu"], tz], axis=1).astype("<f4")
    if "tzbits" in z.files:             # the depths bit for bit (NaNs)
        tris[:, 9:12] = z["tzbits"].astype("<u4").view("<f4")
    out = bytearray(b"PVRUVM1\0") + struct.pack("<2i", len(tris), len(z["ti"]))
    out += tris.tobytes() + z["ti"].astype("<i4").tobytes() + np.floor(z["px"]).astype("<i2").tobytes()
    out += np.floor(z["py"]).astype("<i2").tobytes() + z["t"].astype("<i2").tobytes()
    (data / f"{name}.uvm").write_bytes(out)
    return len(tris), len(z["ti"])


if __name__ == "__main__":
    args = sys.argv[1:]
    test = "uvmeas"
    if args and args[0] == "--test":
        test, args = args[1], args[2:]
    data = HERE.parent / "fit" / f"{test}_data"
    for name in args or sorted(p.name[:-4] for p in data.glob("*.npz")):
        print(name, *export(data, name))
