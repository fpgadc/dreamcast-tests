from pathlib import Path
import itertools
import numpy as np
T = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests/texfmt")


def load(page):
    a = np.fromfile(T / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640)
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)


def sq(im, n):
    y, x = (n // 4 // 20) * 32 + ((n >> 1) & 1) * 16 + 8, ((n // 4) % 20) * 32 + (n & 1) * 16 + 8
    return im[y, x]


p2 = load(2)
U = [(u, sq(p2, u)) for u in range(256)]            # Y = 128, V = 128
V = [(v, sq(p2, 256 + v)) for v in range(256)]
p1 = load(1)
Ysweep = [(y, sq(p1, y)) for y in range(256)]
grid = []
n = 256
for yv in (0, 128, 255):
    for u in (0, 64, 128, 192, 255):
        for v in (0, 64, 128, 192, 255):
            grid.append((yv, u, v, sq(p1, n)))
            n += 1
print("Y sweep exact:", all(tuple(p) == (y, y, y) for y, p in Ysweep))
print("B - 128 for U = 0, 1, 2, 3, 4, 126..130, 252..255:", [int(p[2]) - 128 for u, p in U if u < 5 or 126 <= u <= 130 or u > 251])
print("G - 128 for the same U:", [int(p[1]) - 128 for u, p in U if u < 5 or 126 <= u <= 130 or u > 251])
print("R - 128 for V = 0..4, 126..130, 252..255:", [int(p[0]) - 128 for v, p in V if v < 5 or 126 <= v <= 130 or v > 251])
print("G - 128 for the same V:", [int(p[1]) - 128 for v, p in V if v < 5 or 126 <= v <= 130 or v > 251])
fl = lambda a, b: a // b                       # floor division
tr = lambda a, b: int(a / b)                   # toward zero
sh = lambda a, s: a >> s
cands = {}
for name, f in (("floor", fl), ("trunc", tr)):
    cands[f"B = Y + {name}(u*110/64)"] = ("B", lambda y, u, v, f=f: y + f(u * 110, 64))
    cands[f"B = Y + {name}(u*55/32)"] = ("B", lambda y, u, v, f=f: y + f(u * 55, 32))
    cands[f"B = Y + u + {name}(u*23/32)"] = ("B", lambda y, u, v, f=f: y + u + f(u * 23, 32))
    cands[f"R = Y + {name}(v*11/8)"] = ("R", lambda y, u, v, f=f: y + f(v * 11, 8))
    cands[f"R = Y + v + {name}(v*3/8)"] = ("R", lambda y, u, v, f=f: y + v + f(v * 3, 8))
    cands[f"G = Y - {name}((u*11 + v*22)/32)"] = ("G", lambda y, u, v, f=f: y - f(u * 11 + v * 22, 32))
    cands[f"G = Y - {name}(u*11/32) - {name}(v*11/16)"] = ("G", lambda y, u, v, f=f: y - f(u * 11, 32) - f(v * 11, 16))
    cands[f"G = Y + {name}(-(u*11 + v*22)/32)"] = ("G", lambda y, u, v, f=f: y + f(-(u * 11 + v * 22), 32))
    cands[f"G = Y + {name}(-u*11/32) + {name}(-v*11/16)"] = ("G", lambda y, u, v, f=f: y + f(-u * 11, 32) + f(-v * 11, 16))
samples = [(128, u, 128, p) for u, p in U] + [(128, 128, v, p) for v, p in V] + grid
clamp = lambda x: max(0, min(255, x))
for name, (ch, f) in cands.items():
    i = "RGB".index(ch)
    ok = sum(clamp(f(y, u - 128, v - 128)) == int(p[i]) for y, u, v, p in samples)
    print(f"{ok:4d} of {len(samples)}  {name}")
print("u    :", list(range(-12, 13)))
print("B-128:", [int(p[2]) - 128 for u, p in U if 116 <= u <= 140])
print("G-128:", [int(p[1]) - 128 for u, p in U if 116 <= u <= 140])
print("v    :", list(range(-12, 13)))
print("R-128:", [int(p[0]) - 128 for v, p in V if 116 <= v <= 140])
print("G-128:", [int(p[1]) - 128 for v, p in V if 116 <= v <= 140])
print("u 40..56 B-128:", [int(p[2]) - 128 for u, p in U if 168 <= u <= 184])
print("v 40..56 R-128:", [int(p[0]) - 128 for v, p in V if 168 <= v <= 184])
