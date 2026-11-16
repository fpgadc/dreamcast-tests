"""Per tile or per triangle? Score both readings of the block-exponent depth on the ztile dumps."""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import zfit3 as z3
import zgroup as g

D = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/ztile")
f32 = lambda v: float(np.float32(v))
R = g.R


def green(page, kind):
    if kind == "hw":
        a = np.fromfile(D / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
        return ((a >> 8) & 255) > 128
    from PIL import Image
    return np.asarray(Image.open(D / f"page{page}.emu.png").convert("RGB"))[..., 1] > 128


rows = []
for page in range(6):
    gp = green(page, "hw")
    step = 1.0 / 262144.0
    if page < 2:
        zl, zr = f32(40.0), f32(0.52)
        center = zl + (zr - zl) * (15.5 / 16.0)
        cells = [(c, (c % 20) * 32.0, (c // 20) * 32.0, c - 120) for c in range(240)]
    else:
        zl, zr = f32(32.0), f32(0.52)
        center = zl + (zr - zl) * (63.5 / 64.0)
        zm = f32(zl + (zr - zl) * 0.5)
        cells = [(c, (c % 10) * 64.0, (c // 10) * 32.0, c - 60) for c in range(120)]
    for c, x, y, j in cells:
        zb = f32(center + j * step)
        if page < 2:
            quadsA = [(x + 16, 16.0, zl, zr)]
            bx = x + 28
        elif page < 4:
            quadsA = [(x, 64.0, zl, zr)]
            bx = x + 60
        else:
            quadsA = [(x, 32.0, zl, zm), (x + 32, 32.0, zm, zr)]
            bx = x + 60
        B = z3.strip_tris(bx, y, 4, 32)
        for dx in (0, 1, 2, 3):
            for dy in (2, 8, 14):
                px, py = bx + dx + 0.5, y + dy + 0.5
                tb = next((t for t in B if z3.inside(t[0], px, py)), None)
                ta = None
                for qx, qw, ql, qr in quadsA:
                    for t in z3.strip_tris(qx, y, qw, 32):
                        if z3.inside(t[0], px, py):
                            ta = (t, [qr if vx > qx else ql for vx, vy in t[0]], qx)
                if ta is None or tb is None:
                    continue
                (tri, par), za, qx = ta
                rows.append(([v for p in tri for v in p], za, par, [v for p in tb[0] for v in p], [zb] * 3, tb[1],
                             2 if page & 1 else 0, bool(gp[int(py), int(px)]), px, py, page, qx, y))
TA = np.array([r[0] for r in rows]); ZA = np.array([r[1] for r in rows]); PA = np.array([r[2] for r in rows])
TB = np.array([r[3] for r in rows]); ZB = np.array([r[4] for r in rows]); PB = np.array([r[5] for r in rows])
MODE = np.array([r[6] for r in rows]); DRAWN = np.array([r[7] for r in rows]); X = np.array([r[8] for r in rows]); Y = np.array([r[9] for r in rows])
PAGE = np.array([r[10] for r in rows]); QX = np.array([r[11] for r in rows]); QY = np.array([r[12] for r in rows])
CA, CB = g.coefs(TA, ZA, PA), g.coefs(TB, ZB, PB)


def depth(C, ox, oy):
    """Block interpolation started at the pixel center (ox + 0.5, oy + 0.5)."""
    A, B, Cc, _ = C
    first = R(R(R(A * (ox + 0.5)) + R(B * (oy + 0.5))) + Cc)
    e = np.frexp(first)[1]
    unit = lambda b: np.ldexp(1.0, e - b)
    start = np.trunc(first / unit(20)) * unit(20)
    sl = lambda s: np.trunc(z3.make_rnd(20, "trunc")(s) / unit(26)) * unit(26)
    v = start + sl(A) * (X - ox - 0.5) + sl(B) * (Y - oy - 0.5)
    return z3.make_rnd(19, "trunc")(v)


TX, TY = np.floor(X / 32) * 32, np.floor(Y / 32) * 32
BX = np.floor(X / 4) * 4          # B's own left edge (it is 4 wide, aligned to 4)
variants = {
    "per tile: start at the first pixel of the tile being drawn": (lambda: depth(CA, TX, TY), lambda: depth(CB, TX, TY)),
    "per triangle: start at the polygon's own top left pixel": (lambda: depth(CA, QX, QY), lambda: depth(CB, BX, TY)),
    "per triangle: start at the first pixel of the first tile the polygon touches": (lambda: depth(CA, np.floor(QX / 32) * 32, TY), lambda: depth(CB, TX, TY)),
}
print(len(rows), "observations; per page", {int(p): int((PAGE == p).sum()) for p in range(6)})
for name, (fa, fb) in variants.items():
    with np.errstate(all="ignore"):
        da, db = fa(), fb()
    ok = np.where(MODE == 0, db > da, db == da) == DRAWN
    print(f"{int(ok.sum()):6d}  {name}   by page {[int(ok[PAGE == p].sum()) for p in range(6)]}")
