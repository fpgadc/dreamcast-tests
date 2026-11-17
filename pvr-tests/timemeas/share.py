#!/usr/bin/env python3
"""Tables of the share scenes (REPORT.md, "Do setup and run starts share the arithmetic?"): python3 share.py <log>.
A line's cost is the slope over L (8, 16, 24, 32) through both passes, TSP end, clocks a tile."""
import sys

t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 16 or f[0] != "T" or f[1] != "share":
        continue
    t[tuple(int(x) for x in f[2:6])] = (int(f[9]) / 2.0 / 64, int(f[7]) / 2.0 / 64)
LS = (8, 16, 24, 32)


def slope(f, v, col=0):
    ys = [(L, t[(f, v, L, ps)][col]) for L in LS for ps in (0, 1)]
    mx = sum(x for x, y in ys) / len(ys)
    my = sum(y for x, y in ys) / len(ys)
    return sum((x - mx) * (y - my) for x, y in ys) / sum((x - mx) ** 2 for x, y in ys)


names = ["15 cuts", "16 cuts", "15 cuts + sprite", "sprite only", "1 cut only", "15 cuts + tall sprite", "tall sprite only"]
for f, fn in ((2, "565"), (4, "565 with offset colour")):
    s = [slope(f, v) for v in range(5)]
    si = [slope(f, v, 1) for v in range(5)]
    sp = max(abs(t[(f, v, L, 0)][0] - t[(f, v, L, 1)][0]) for v in range(5) for L in LS)
    print(f"{fn}: a line over a plain one (TSP end; ISP end in brackets); passes within {sp:.2f} a tile")
    for v in range(5):
        print(f"  v {v} {names[v]:18s} {s[v]:7.2f}  ({si[v]:7.2f})")
    print(f"  16th cut, run-start-paced (v1 - v0) {s[1] - s[0]:6.2f}")
    print(f"  sprite instead of 16th cut, run-start-paced (v2 - v1) {s[2] - s[1]:6.2f}")
    print(f"  sprite instead of 1 cut, shading-paced (v3 - v4) {s[3] - s[4]:6.2f}")
    if (f, 5, 8, 0) in t:
        s5, s6 = slope(f, 5), slope(f, 6)
        print(f"  v 5 15 cuts + tall sprite {s5:7.2f}   v 6 tall sprite only {s6:7.2f}")
        print(f"  triangle changes, run-start-paced (v5 - v1) {s5 - s[1]:6.2f}; a setup a line on top (v2 - v5) {s[2] - s5:6.2f}")
        print(f"  triangle changes, shading-paced (v6 - v4) {s6 - s[4]:6.2f}; a setup a line on top (v3 - v6) {s[3] - s6:6.2f}")
