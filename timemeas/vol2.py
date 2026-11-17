#!/usr/bin/env python3
"""Tables of the vol2off and vol2pitch scenes (REPORT.md, "An offset colour of 0 costs nothing"):
python3 vol2.py <timemeas log>."""
import sys

t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 16 or f[0] != "T" or f[1] not in ("vol2off", "vol2pitch"):
        continue
    t[(f[1],) + tuple(int(x) for x in f[2:6])] = (int(f[9]), f[14], f[15])
kn = {0: "one volume ", 1: "two volumes"}
on = {0: "offset colour 0          ", 1: "offset colour 0x202020   ", 2: "offset colour 0x101010(k+1)"}
fn = {2: "565       ", 4: "565 offset"}
print("vol2off: setup per quad (one seen triangle): 1024-quad tile minus 1-quad tile, / 1023; pixels (0,0) (20,20)")
for k in (0, 1):
    for o in (0, 1, 2):
        for f in (2, 4):
            row = []
            for m in (0, 1):
                big, small = t[("vol2off", k, o, f, 50 + m)], t[("vol2off", k, o, f, 20 + m)]
                row.append(f"{'inside ' if m else 'outside'} {(big[0] - small[0]) / 2.0 / 1023:6.2f} px {small[1]} {small[2]}")
            print(f"  {kn[k]} {on[o]} {fn[f]}: " + "   ".join(row))
print("vol2pitch: clocks a run start on lines 24-31, both passes; pixels")
for f in (2, 4):
    for o in (0, 1):
        r = [(t[("vol2pitch", f, o, 32, ps)][0] - t[("vol2pitch", f, o, 24, ps)][0]) / 2.0 / 64 / 240 for ps in (0, 1)]
        b = t[("vol2pitch", f, o, 32, 0)]
        print(f"  {fn[f]} offset colour {'0x202020' if o else '0       '}: {r[0]:5.2f} {r[1]:5.2f}   px {b[1]} {b[2]}")
