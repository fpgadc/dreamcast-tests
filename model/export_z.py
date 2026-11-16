"""Archived zmeas runs (fit/zmeas_data/<set>.cases.json + .meas.json) as flat binary files for the C++ probes.

    python export_z.py [set ...]        ->  fit/zmeas_data/<set>.zme   (all sets when none is named)

Layout, little endian: "PVRZME1\0"; int32 records; per record float32 x[3], y[3], z[3], px, py; float64 lo, hi:
the console's stored depth at the pixel is at least lo and below hi (NaN: no bound on that side).
"""
import json
import struct
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
DATA = HERE.parent / "fit" / "zmeas_data"


def quad_tri(cell, px, py):
    """zmeas/zmeas.h, QuadTri: the triangle of the full-cell quad (strip TL, TR, BL, BR) that holds the pixel"""
    cx, cy = (cell % 20) * 32.0, (cell // 20) * 32.0
    s = (px - cx) + (py - cy)
    if s == 32.0:
        return None
    if s < 32.0:
        return [cx, cx + 32, cx], [cy, cy, cy + 32]
    return [cx, cx + 32, cx + 32], [cy + 32, cy, cy + 32]


def export(name):
    cases = json.loads((DATA / f"{name}.cases.json").read_text())["cases"]
    meas = json.loads((DATA / f"{name}.meas.json").read_text())["cases"]
    out = bytearray()
    n = 0
    nan = float("nan")
    for c, m in zip(cases, meas):
        for p in m["pixels"]:
            if not p["monotone"] or (p["lo"] is None and p["hi"] is None):
                continue
            if c["quad"]:
                t = quad_tri(c["cell"], p["px"], p["py"])
                if t is None:
                    continue
                x, y = t
            else:
                x, y = c["x"], c["y"]
            out += struct.pack("<11f2d", *x, *y, *c["z"], p["px"], p["py"], nan if p["lo"] is None else p["lo"], nan if p["hi"] is None else p["hi"])
            n += 1
    (DATA / f"{name}.zme").write_bytes(b"PVRZME1\0" + struct.pack("<i", n) + out)
    return n


if __name__ == "__main__":
    names = sys.argv[1:] or sorted(p.name[:-11] for p in DATA.glob("*.cases.json"))
    for name in names:
        print(f"{name}: {export(name)} pixels")
