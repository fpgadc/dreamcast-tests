#!/usr/bin/env python3
"""Tables of the offs2 and offs3 scenes (REPORT.md, "What counts as 0", "Not shared with mipmapping"):
python3 offs.py <timemeas log>."""
import sys
t = {}
for l in open(sys.argv[1]):
    f = l.split()
    if len(f) < 16 or f[0] != "T" or f[1] not in ("offs2", "offs3"):
        continue
    t[(f[1],) + tuple(int(x) for x in f[2:6])] = (int(f[9]), f[14], f[15])
fn = {2: "565 flat", 3: "565 Gouraud", 4: "565 flat offset", 5: "565 Gouraud offset", 7: "565 Gouraud mip point",
      48: "565 Gouraud offset mip point"}
print("offs2: setup per seen triangle (1024 1x1 quads minus 1 quad, / 1023) and run pitch (lines 24-31, both passes)")
for f in (3, 5, 7, 48):
    su = (t[("offs2", f, 0, 5, 0)][0] - t[("offs2", f, 0, 2, 0)][0]) / 2.0 / 1023
    pi = [(t[("offs2", f, 1, 32, ps)][0] - t[("offs2", f, 1, 24, ps)][0]) / 2.0 / 64 / 240 for ps in (0, 1)]
    print(f"  {fn[f]:30s}: setup {su:6.2f}   pitch {pi[0]:5.2f} {pi[1]:5.2f}")
on = {0: "0", 1: "0x202020 all", 3: "0xff000000 all", 4: "0x000020 all", 5: "0x202020 at v0 only", 6: "0x202020 at v3 only"}
print("offs3: per quad of 2x2 (two seen triangles): 256-quad tile minus 1 quad, / 255; extra over offset colour 0")
for f in (2, 3, 4, 5):
    base = None
    for o in (0, 1, 3, 4, 5, 6):
        if ("offs3", f, o, 11, 0) not in t:
            continue
        q = (t[("offs3", f, o, 11, 0)][0] - t[("offs3", f, o, 2, 0)][0]) / 2.0 / 255
        if base is None:
            base = q
        px = t[("offs3", f, o, 2, 0)]
        print(f"  {fn[f]:20s} {on[o]:20s}: {q:7.2f}  extra {q - base:+6.2f}   px {px[1]} {px[2]}")
