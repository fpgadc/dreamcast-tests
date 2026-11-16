"""Compare a test's hardware dumps with the emulator's frames, labeled region by region.

    python compare.py <test> [folder, default /tmp/pvr_tests/<test>] [-v] [-q]

For every page: how many labeled regions match exactly, and for those that do not, how many pixels differ,
the largest difference, and the hardware and emulator colors at the worst pixel. -v lists all of them
(default: the first 12 per page), -q only the totals. Writes page<n>.cmp.png: hardware | emulator | difference x16.
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

COLS = 20


def load(d, page):
    raw, raw16, png = d / f"page{page}.hw.raw", d / f"page{page}.hw16.raw", d / f"page{page}.emu.png"
    if not png.exists() or not (raw.exists() or raw16.exists()):
        return None, None
    emu = np.asarray(Image.open(png).convert("RGB"), dtype=np.int16)
    if raw.exists():
        a = np.fromfile(raw, dtype="<u4").reshape(480, 640)
        hw = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(np.int16)
    else:
        # A 16 bit framebuffer: compare at the framebuffer's own precision. The emulator's frame is whatever
        # it expands 5 or 6 bits to; taking its top bits back undoes either a shift or a replication.
        a = np.fromfile(raw16, dtype="<u2").reshape(480, 640).astype(np.int16)
        g6 = (d / "rgb565").exists()
        if g6:
            hw = np.stack([(a >> 11) & 31, (a >> 5) & 63, a & 31], axis=-1)
            emu = np.stack([emu[..., 0] >> 3, emu[..., 1] >> 2, emu[..., 2] >> 3], axis=-1)
        else:
            hw = np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1)
            emu = emu >> 3
    return hw, emu


def labels(d):
    out = {}
    f = d / "labels.txt"
    if f.exists():
        for line in f.read_text(errors="replace").splitlines():
            p = line.split(" ", 5)
            if len(p) == 6 and p[0] == "LABEL":
                out.setdefault(int(p[1]), []).append((int(p[2]), int(p[3]), int(p[4]), p[5].strip()))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    verbose, quiet = "-v" in sys.argv, "-q" in sys.argv
    test = args[0]
    d = Path(args[1]) if len(args) > 1 else Path("/tmp/pvr_tests") / test
    lab = labels(d)
    pages = sorted({int(p.name[4:].split(".")[0]) for p in d.glob("page*.hw*.raw")})
    total = bad_total = 0
    for page in pages:
        hw, emu = load(d, page)
        if hw is None:
            print(f"page {page}: missing emulator frame")
            continue
        diff = np.abs(hw - emu)
        Image.fromarray(np.concatenate([hw, emu, np.minimum(diff * 16, 255)], axis=1).clip(0, 255).astype(np.uint8)).save(d / f"page{page}.cmp.png")
        covered = np.zeros((480, 640), dtype=bool)
        rows = []
        regions = lab.get(page, [])
        for cell, cw, ch, text in sorted(regions):
            y, x = (cell // COLS) * 32, (cell % COLS) * 32
            sl = (slice(y, y + ch * 32), slice(x, x + cw * 32))
            covered[sl] = True
            dd = diff[sl]
            if dd.any():
                m = dd.max(axis=-1)
                wy, wx = np.unravel_index(m.argmax(), m.shape)
                rows.append((cell, text, int((m > 0).sum()), m.size, int(m.max()), (int(x + wx), int(y + wy)),
                             tuple(int(t) for t in hw[sl][wy, wx]), tuple(int(t) for t in emu[sl][wy, wx])))
        stray = int((diff.max(axis=-1) > 0)[~covered].sum())
        total += len(regions)
        bad_total += len(rows)
        print(f"page {page}: {len(regions) - len(rows)} of {len(regions)} regions match exactly, {len(rows)} differ"
              + (f"; {stray} differing pixels outside any labeled region" if stray else ""))
        if quiet:
            continue
        for cell, text, n, size, mx, at, h, e in (rows if verbose else rows[:12]):
            print(f"   cell {cell:3d}  {text:44s} {n:5d} of {size} px, max {mx:3d}; at {at}: hardware {h} emulator {e}")
        if not verbose and len(rows) > 12:
            print(f"   ... and {len(rows) - 12} more (-v)")
    print(f"{test}: {total - bad_total} of {total} regions match exactly")


if __name__ == "__main__":
    main()
