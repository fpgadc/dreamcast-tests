"""The depths a console stores differently from refsw3-core in a game frame's dump, as depthprobe records.

    python3 model/zmiss.py <scene> [<folder with depth.hw.raw and depth.off.raw>] [--all-near]

The folder defaults to /tmp/pvr_tests/zdumps/<scene> (zdumps.sh). Every depth that differs: the tiles are rendered
again by refsw3-offline with its render log, the fragments of the pixel are read from it, and the triangle whose
fragment is nearest refsw3's stored depth (the last one, when several are) is taken: its corners as the log prints
them (hexadecimal floats, the rasterizer's order). Output: fit/zdump_data/<scene>.zme, records with the console's
stored depth as the bracket [console, console + one step of the 19 bit grid), and <scene>.txt beside it, a line a
record. depthprobe on the file then tells whether the model gives refsw3's value for the triangle chosen (it
should, but for the record's console bracket).
"""
import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
DUMPS = Path(os.environ.get("DUMPS", HERE.parent.parent / "dreamster/polly2-rtl/polly2-data/dumps"))
OFFLINE = Path(os.environ.get("OFFLINE", "/tmp/refsw3-offline")) / "refsw3-offline"
OUT = HERE.parent / "fit" / "zdump_data"


def f32(u):
    return struct.unpack("<f", struct.pack("<I", int(u) & 0xffffffff))[0]


def step(u):
    """one step of the 19 bit grid above the stored depth u (its bits)"""
    e = (int(u) >> 23) & 0xff
    return f32(u) + 2.0 ** (e - 127 - 18)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    scene = args[0]
    folder = Path(args[1]) if len(args) > 1 else Path(f"/tmp/pvr_tests/zdumps/{scene}")
    hw, off = (np.fromfile(folder / f"depth.{k}.raw", dtype="<u4").reshape(480, 640) for k in ("hw", "off"))
    miss = [(int(x), int(y)) for y, x in np.argwhere(hw != off)]
    OUT.mkdir(parents=True, exist_ok=True)
    if not miss:
        print(f"{scene}: no depth differs")
        return 0
    tiles = sorted({(x // 32, y // 32) for x, y in miss})
    log = folder / "render.log"
    cmd = f"regs {DUMPS}/pvr_regs_{scene}.bin;vram {DUMPS}/vram_{scene}.bin;log {log} " + " ".join(f"{x},{y}" for x, y in tiles) + ";render"
    subprocess.run([str(OFFLINE), "-c", cmd], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # the log, tile by tile: ISP lines (index, x, y, depth, tag) and the corners of every tag
    frags, tris, tile = {}, {}, None
    for line in open(log, errors="replace"):
        t = line.split()
        if not t:
            continue
        if t[0] == "TILE:":                         # the region array entry: its control word holds the tile
            c = int(t[2], 16)
            tile = ((c >> 2) & 63, (c >> 8) & 63)
        elif t[0] == "ISP:" and len(t) >= 8:          # pixel index in the tile, ..., depth, tag
            frags.setdefault((tile, int(t[1])), []).append((float(t[6]), t[7], len(frags)))
        elif t[0] == "TRIA:":
            v = t[5:]
            n = len(v) // 3
            tris[t[1]] = [[float.fromhex(v[k * n + c]) for k in range(3)] for c in range(3)]      # x[3], y[3], z[3]
    recs, lines = [], []
    for x, y in miss:
        want = f32(off[y, x])
        cands = [(abs(z - want), -k, tag) for z, tag, k in frags.get(((x // 32, y // 32), (y % 32) * 32 + x % 32), []) if tag in tris]
        if not cands:
            lines.append(f"({x}, {y}): no fragment in the log")
            continue
        _, _, tag = min(cands)
        xs, ys, zs = tris[tag]
        recs.append((xs, ys, zs, x + 0.5, y + 0.5, f32(hw[y, x]), step(hw[y, x])))
        lines.append(f"({x}, {y}) tile ({x // 32}, {y // 32}) at ({x % 32}, {y % 32}): console {int(hw[y, x]):08x} refsw3 {int(off[y, x]):08x} tag {tag} "
                     f"x {' '.join(v.hex() for v in xs)} y {' '.join(v.hex() for v in ys)} z {' '.join(v.hex() for v in zs)}")
    with open(OUT / f"{scene}.zme", "wb") as o:
        o.write(b"PVRZME1\0" + struct.pack("<i", len(recs)))
        for xs, ys, zs, px, py, lo, hi in recs:
            o.write(struct.pack("<3f3f3fff", *xs, *ys, *zs, px, py) + struct.pack("<dd", lo, hi))
    (OUT / f"{scene}.txt").write_text("\n".join(lines) + "\n")
    print(f"{scene}: {len(miss)} depths differ, {len(recs)} records -> {OUT / (scene + '.zme')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
