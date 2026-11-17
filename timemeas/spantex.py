#!/usr/bin/env python3
"""Tables of the spantex scenes (REPORT.md, "Is texturing after the queue?"): python3 spantex.py <log>.
Clocks a tile (TSP end); a burst of L cut lines adds 30 L runs of one pixel."""
import sys

t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 12 or f[0] != "T" or f[1] != "spantex":
        continue
    t[(int(f[2]), int(f[3]), int(f[4]))] = int(f[9]) / 2.0 / 64
LS = (0, 2, 4, 6, 8, 10, 12)
names = {3: "565 (1.01 a pixel)", 20: "YUV422 bilinear (2.00)", 23: "565, 8 texels a pixel (3.27)"}
for f in (3, 20, 23):
    if (f, 0, 0) not in t:
        continue
    base = [(t[(f, 0, 0)] + t[(f, 0, 1)]) / 2]
    d = {L: (t[(f, L, 0)] + t[(f, L, 1)]) / 2 - base[0] for L in LS}
    sp = max(abs(t[(f, L, 0)] - t[(f, L, 1)]) for L in LS)
    full = (d[12] - d[8]) / 4                    # a cut line once nothing is hidden
    print(f"{names[f]:30s}: tile {base[0]:7.1f}; passes within {sp:.1f}; a cut line at the end {full:6.1f} ({full / 30:4.2f} a run)")
    print("   L        " + "".join(f"{L:8d}" for L in LS))
    print("   added    " + "".join(f"{d[L]:8.1f}" for L in LS))
    print("   hidden   " + "".join(f"{full * L - d[L]:8.1f}" for L in LS) + "   (clocks: a full line each, less what was added)")
    print("   runs     " + "".join(f"{(full * L - d[L]) / (full / 30):8.1f}" for L in LS))
k2 = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) >= 12 and f[0] == "T" and f[1] == "spantex2":
        k2[(int(f[2]), int(f[3]), int(f[4]))] = int(f[9]) / 2.0 / 64
if k2:
    print("spantex2: lines 0 .. K-1 with 8 texels a pixel, K .. 19 565, burst at 20: clocks the burst adds (both passes)")
    print("   K \ L   " + "".join(f"{L:8d}" for L in (4, 8, 12)) + "     tile, L 0    passes within")
    for K in (0, 4, 10, 16, 20):
        b = (k2[(K, 0, 0)] + k2[(K, 0, 1)]) / 2
        row = "".join(f"{(k2[(K, L, 0)] + k2[(K, L, 1)]) / 2 - b:8.1f}" for L in (4, 8, 12))
        sp = max(abs(k2[(K, L, 0)] - k2[(K, L, 1)]) for L in (0, 4, 8, 12))
        print(f"   {K:2d}      {row}     {b:7.1f}      {sp:.1f}")
