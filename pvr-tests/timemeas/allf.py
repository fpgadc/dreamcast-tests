#!/usr/bin/env python3
"""Table of the allf scenes (REPORT.md, "Every feature, and two volumes"): python3 allf.py <timemeas log>.

TSP clocks (10 ns) of a scene against its twin (the same polygons behind the background), least of 11 renders,
per seen triangle; both passes."""
import sys

t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 16 or f[0] != "T" or f[1] != "allf":
        continue
    t[tuple(int(x) for x in f[2:6])] = (int(f[9]), f[14], f[15])
names = ["565 gouraud", "+ offset colour", "+ mipmapped bilinear", "+ fog table", "+ DCalcCtrl 1 (all)",
         "all, two volumes, outside", "all, two volumes, inside", "untextured gouraud"]
tris = {2: 2, 11: 512, 5: 1024}
print("%-28s %22s %22s %22s   pixels (0,0) (20,20) seen" % ("", "1 quad 32x32 (2 tri)", "256 quads 2x2 (512)", "1024 quads 1x1 (1024)"))
for c, n in enumerate(names):
    row = []
    for L in (2, 11, 5):
        per = []
        for p in (0, 1):
            if (c, L, 0, p) in t and (c, L, 1, p) in t:
                per.append((t[(c, L, 0, p)][0] - t[(c, L, 1, p)][0]) / 2.0)
        row.append("%7.0f = %6.1f /tri" % (per[0], per[0] / tris[L]) + ("" if len(per) < 2 else " (%+.0f)" % (per[1] - per[0])) if per else "-")
    px = t.get((c, 5, 0, 0), (0, "?", "?"))
    print("%-28s %s   %s %s" % (n, "  ".join("%22s" % r for r in row), px[1], px[2]))
