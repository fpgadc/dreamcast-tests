"""lencells: the cells of mipk3 and mipk4 whose numerators are exact, for lenfit.cpp (the console's length of K).

    python3 pvr-tests/mipk/lencells.py > cells.txt

A line a cell: set row hi lo_num lo_exp bias tk reached, in units of the longer numerator's 24 bit grid (hi its
mantissa; lo = lo_num * 2^lo_exp); reached: the console's K = L * bias / 4 reaches 130 * 2^tk (a change of level).
mipk3: its rows whose step is seen (K8 130 with K >= 2; the others show one colour all along), from
fit/mipk_data/mipk3-seed3.observed.txt. mipk4: every row, from the console's archived pages (fit/hw/mipk4): the higher
of a row's two levels is reached."""
import gzip
import math
import re
from fractions import Fraction
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
LEVEL = {0xff0000: 0, 0x00ff00: 1, 0x0000ff: 2, 0xffff00: 3, 0x00ffff: 4, 0xff00ff: 5, 0xffffff: 6}


def level(c):
    r, g, b = (c >> 16) & 255, (c >> 8) & 255, c & 255
    return LEVEL[((r > 127) * 0xff0000) | ((g > 127) * 0x00ff00) | ((b > 127) * 0x0000ff)]


def mipk3():
    quads = [l for l in (ROOT / "pvr-tests/mipk3/cases.h").read_text().splitlines() if l.strip().startswith("{ ") and "0x" in l]
    k = {}
    for l in (ROOT / "pvr-tests/mipk3/cases.txt").read_text().splitlines():
        if not l.startswith("#"):
            a = l.split()
            k[(int(a[0]), int(a[1]))] = float(a[5])
    obs = {}
    for l in (ROOT / "fit/mipk_data/mipk3-seed3.observed.txt").read_text().splitlines():
        a = l.split()
        obs[(int(a[0]), int(a[1]))] = a[2]
    for r in range(120):
        page, c0 = r // 15, (r % 15) * 20 + 10
        kk = k[(page, c0)]
        if kk < 2 or int(kk / 2 ** (math.floor(math.log2(kk)) - 7)) != 130:
            continue                                    # not a change of level
        for s in range(20):
            words = re.findall(r"0x([0-9a-f]+)u", quads[page * 300 + (r % 15) * 20 + s])
            u1, v1 = int(words[6], 16), int(words[7], 16)
            mu, eu = (u1 & 0x7fffff) | 0x800000, (u1 >> 23) & 255
            mv, ev = (v1 & 0x7fffff) | 0x800000, (v1 >> 23) & 255
            tk = 17 if mu + Fraction(mv) * Fraction(2) ** (ev - eu) * Fraction(27146, 65536) >= 2 ** 24 else 16
            o = obs[(page, (r % 15) * 20 + s)]
            reached = (s >= 10 and o == "0") or (s < 10 and o == "+1")
            print(3, r, mu, mv, ev - eu, 4, tk, int(reached))


def mipk4():
    cases = {}
    for l in (ROOT / "pvr-tests/mipk4/cases.txt").read_text().splitlines():
        if not l.startswith("#"):
            a = l.split()
            cases[(int(a[0]), int(a[1]))] = (int(a[6]), int(a[8]), int(a[9]), int(a[10]))      # bias hi n b
    for p in range(8):
        page = np.frombuffer(gzip.open(ROOT / f"fit/hw/mipk4/page{p}.hw.raw.gz").read(), dtype="<u4").reshape(480, 640) & 0xffffff
        for r in range(15):
            levels = [level(int(page[r * 32 + 6, c * 32 + 6])) for c in range(20)]
            for s in range(20):
                bias, hi, n, b = cases[(p, r * 20 + s)]
                print(4, p * 15 + r, hi, n, b - 23, bias, 17, int(levels[s] == max(levels)))


if __name__ == "__main__":
    mipk3()
    mipk4()
