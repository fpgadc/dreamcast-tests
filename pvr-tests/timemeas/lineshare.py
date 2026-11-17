#!/usr/bin/env python3
"""Tables of the lineshare scenes (REPORT.md, "Is part of a run start kept for the line?"):
python3 lineshare.py <timemeas log>. Times are the TSP's end, in clocks a tile; a scene's cost of a pattern
line is the slope over L (8, 16, 24, 32) through both passes."""
import sys

t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 12 or f[0] != "T" or not f[1].startswith("lineshare"):
        continue
    t[(f[1], int(f[2]), int(f[3]), int(f[4]), int(f[5]))] = int(f[9]) / 2.0 / 64
LS = (8, 16, 24, 32)


def slope(*key):
    ys = [(L, t[key + (L, ps)]) for L in LS for ps in (0, 1)]
    mx = sum(x for x, y in ys) / len(ys)
    my = sum(y for x, y in ys) / len(ys)
    return sum((x - mx) * (y - my) for x, y in ys) / sum((x - mx) ** 2 for x, y in ys)


def have(*key):
    return key + (8, 0) in t


names = {0: "untextured", 2: "565", 4: "565 with offset colour"}
for f in (0, 2, 4):
    for K in (3, 4):
        if have("lineshare2", f, 10 * K):
            r, d = slope("lineshare2", f, 10 * K), slope("lineshare2", f, 10 * K + 1)
            print(f"lineshare2 {names[f]:24s} K {K}: R {r:6.2f} D {d:6.2f} a line, R - D {r - d:+6.2f}")
for s in (0, 1):
    if not have("lineshare5", s, 10):
        continue
    print(f"lineshare5, first run {s + 1} px: R = T..T, R' = T..U, D = U0..U")
    print("  M      R     R'      D   R'-R   D-R'")
    for M in range(1, 9):
        r, rr, d = (slope("lineshare5", s, 10 * M + v) for v in range(3))
        print(f"  {M}  {r:6.2f} {rr:6.2f} {d:6.2f} {rr - r:6.2f} {d - rr:6.2f}")
