"""Which way does the console combine the four texels of a bilinear sample? (pvr_tests/sample page 1)"""
from pathlib import Path
import numpy as np

a = np.fromfile(Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/sample/page1.hw.raw"), dtype="<u4").reshape(480, 640)
HW = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)
e5 = lambda v: (v << 3) | (v >> 2)
e6 = lambda v: (v << 2) | (v >> 4)
TEX = np.zeros((8, 8, 3), dtype=int)
for j in range(8):
    for i in range(8):
        TEX[j, i] = (e5(i * 4 + 2), e6(j * 8 + 4), e5(31 if (i ^ j) & 1 else 0))


def coord(x):           # 1/256 texel, boundary ties go down (as fitted for point sampling)
    return int(np.ceil(x - 1 / 32) - 1) if x > 0 else int(np.floor(x + 1 / 32))


def sample(u, v, how):
    ui, vi = coord(u * 8 * 256) - 128, coord(v * 8 * 256) - 128
    iu, iv, fu, fv = ui >> 8, vi >> 8, (ui & 255) >> 3, (vi & 255) >> 3
    t = lambda di, dj: TEX[(iv + dj) & 7, (iu + di) & 7]
    c00, c10, c01, c11 = t(0, 0), t(1, 0), t(0, 1), t(1, 1)
    if how == "joint":
        return (c00 * (32 - fu) * (32 - fv) + c10 * fu * (32 - fv) + c01 * (32 - fu) * fv + c11 * fu * fv) >> 10
    if how == "h then v":
        top = (c00 * (32 - fu) + c10 * fu) >> 5
        bot = (c01 * (32 - fu) + c11 * fu) >> 5
        return (top * (32 - fv) + bot * fv) >> 5
    if how == "v then h":
        l = (c00 * (32 - fv) + c01 * fv) >> 5
        r = (c10 * (32 - fv) + c11 * fv) >> 5
        return (l * (32 - fu) + r * fu) >> 5
    if how == "each term":
        return ((c00 * (32 - fu) * (32 - fv)) >> 10) + ((c10 * fu * (32 - fv)) >> 10) + ((c01 * (32 - fu) * fv) >> 10) + ((c11 * fu * fv) >> 10)
    if how == "joint, weights x8":
        return (c00 * (32 - fu) * (32 - fv) + c10 * fu * (32 - fv) + c01 * (32 - fu) * fv + c11 * fu * fv + 0) // 1024


cells = [(c, (c - 8) / 8.0, 0.0) for c in range(17)] + [(20 + c, 0.0, (c - 8) / 8.0) for c in range(17)] + [(40 + c, (c - 8) / 8.0, (c - 8) / 8.0) for c in range(17)]
for how in ("joint", "h then v", "v then h", "each term"):
    ok = tot = 0
    per = []
    for cell, du, dv in cells:
        cx, cy = (cell % 20) * 32, (cell // 20) * 32
        good = 0
        for py in range(32):
            for px in range(32):
                got = HW[cy + py, cx + px]
                want = sample((px + 0.5 + du) / 32, (py + 0.5 + dv) / 32, how)
                good += int((got == want).all())
        ok += good; tot += 1024
        per.append(good)
    print(f"{how:10s} {ok} of {tot} pixels;  per cell (u shifts): {per[:17]}")
