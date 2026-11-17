#!/usr/bin/env python3
"""Tables of the spantex3 scenes (REPORT.md, "Is it the write-out?"): python3 spantex3.py <log>."""
import sys

t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 12 or f[0] != "T" or f[1] != "spantex3":
        continue
    t[(int(f[2]), int(f[3]), int(f[4]))] = int(f[9]) / 2.0 / 64
LS = (0, 2, 4, 6, 8, 10, 12)
wn = {0: "16 bit, written out", 1: "16 bit, NOT written out", 2: "0888, written out"}
rn = {0: "all 565 (no sprite)", 1: "565 sprite lines 0-19", 2: "8 texels lines 0-3", 3: "8 texels lines 0-19"}
for w in (0, 1, 2):
    print(f"{wn[w]}: clocks the burst of L cut lines adds to a tile (both passes)")
    print("   lines before            tile L0 " + "".join(f"{L:7d}" for L in LS[1:]) + "   steps per 2 lines" + "   passes")
    for r in (0, 1, 2, 3):
        k = 100 * w + r
        if (k, 0, 0) not in t:
            continue
        a = {L: (t[(k, L, 0)] + t[(k, L, 1)]) / 2 for L in LS}
        d = [a[L] - a[0] for L in LS]
        st = [d[i + 1] - d[i] for i in range(len(LS) - 1)]
        sp = max(abs(t[(k, L, 0)] - t[(k, L, 1)]) for L in LS)
        print(f"   {rn[r]:24s}{a[0]:7.1f} " + "".join(f"{x:7.0f}" for x in d[1:]) + "   " + " ".join(f"{x:4.0f}" for x in st) + f"   {sp:4.1f}")
