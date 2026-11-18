"""mipk: the console's pages against refsw3-offline's, per cell, for one or more runs of the model.

    python3 pvr-tests/mipk/analyze.py <console folder> <name>=<offline folder> ...
    python3 pvr-tests/mipk/analyze.py observe <test> <console folder> <model> <K8+1> <K8-1> > observed.txt

observe (stair tests whose cases.txt gives each cell's model K and the row's step K, as gen's detstair): each cell's
console pixels against the offline renders with the model's K8 and with K8 one step up and down (run2.sh <test> off
with MIP_K=0, 98, 99, copied away): whether the console's K8 reaches the row's step, a line a cell "page cell 1|0|?"
(?: the renders that would tell are the same, or the console is like neither).

Each folder has page<n>.hw.raw (console) or page<n>.off.raw (offline; e.g. run2.sh mipk off with MIP_K=<variant>,
copied away). Per run: cells whose every pixel is the console's, by page and by K's distance from its step of 8 bits
(cases.txt), and the pixels that differ."""
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent


def pages(folder, kind):
    return [np.fromfile(Path(folder) / f"page{p}.{kind}.raw", dtype="<u4").reshape(480, 640) & 0xffffff for p in range(8)]


def observe(test, hwf, base, up, down):
    hw, b, u, d = pages(hwf, "hw"), pages(base, "off"), pages(up, "off"), pages(down, "off")
    for l in (HERE.parent / test / "cases.txt").read_text().splitlines():
        if l.startswith("#"):
            continue
        c = l.split()
        page, cell, k, kstep = int(c[0]), int(c[1]), float(c[7]), float(c[8])
        x0, y0 = (cell % 20) * 32, (cell // 20) * 32
        cut = lambda a: a[page][y0:y0 + 32, x0:x0 + 32]
        model = k >= kstep                              # the model's K8 reaches the step here
        other = d if model else u                       # the render on the other side of the step
        same_base, same_other = (cut(hw) == cut(b)).all(), (cut(hw) == cut(other)).all()
        if (cut(b) == cut(other)).all() or same_base == same_other:
            r = "?"
        else:
            r = str(int(model if same_base else not model))
        print(page, cell, r)


def main():
    if sys.argv[1] == "observe":
        return observe(*sys.argv[2:7])
    hw = pages(sys.argv[1], "hw")
    cases = [l.split() for l in (HERE / "cases.txt").read_text().splitlines() if l and not l.startswith("#")]
    for arg in sys.argv[2:]:
        name, folder = arg.split("=", 1)
        off = pages(folder, "off")
        ok, total, px = {}, {}, 0
        for c in cases:
            page, cell, dist = int(c[0]), int(c[1]), float(c[7])
            x0, y0 = (cell % 20) * 32, (cell // 20) * 32
            d = int((hw[page][y0:y0 + 32, x0:x0 + 32] != off[page][y0:y0 + 32, x0:x0 + 32]).sum())
            px += d
            b = "2^-18..-13" if dist < 2 ** -13 else "2^-13..-10" if dist < 2 ** -10 else "2^-10..-4"
            for key in (f"page {page}", b):
                total[key] = total.get(key, 0) + 1
                ok[key] = ok.get(key, 0) + (d == 0)
        print(f"{name}: {sum(ok[f'page {p}'] for p in range(8))} of {len(cases)} cells the console's, {px} pixels differ")
        print("    " + "  ".join(f"{k}: {ok[k]}/{total[k]}" for k in sorted(total)))


if __name__ == "__main__":
    main()
