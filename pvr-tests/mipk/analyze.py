"""mipk: the console's pages against refsw3-offline's, per cell, for one or more runs of the model.

    python3 pvr-tests/mipk/analyze.py <console folder> <name>=<offline folder> ...

Each folder has page<n>.hw.raw (console) or page<n>.off.raw (offline; e.g. run2.sh mipk off with MIP_K=<variant>,
copied away). Per run: cells whose every pixel is the console's, by page and by K's distance from its step of 8 bits
(cases.txt), and the pixels that differ."""
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent


def pages(folder, kind):
    return [np.fromfile(Path(folder) / f"page{p}.{kind}.raw", dtype="<u4").reshape(480, 640) & 0xffffff for p in range(8)]


def main():
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
