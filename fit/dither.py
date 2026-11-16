from pathlib import Path
import numpy as np
T = Path("//wsl.localhost/Ubuntu/tmp/pvr_tests")
for test, gbits in (("fb565", 6), ("fb555", 5)):
    a = np.fromfile(T / test / "page1.hw16.raw", dtype="<u2").reshape(480, 640).astype(int)
    if gbits == 6:
        ch = {"r": ((a >> 11) & 31, 3), "g": ((a >> 5) & 63, 2), "b": (a & 31, 3)}
    else:
        ch = {"r": ((a >> 10) & 31, 3), "g": ((a >> 5) & 31, 3), "b": (a & 31, 3)}
    for name, (img, sh) in ch.items():
        base = {"r": 256, "g": 512, "b": 768}[name]
        thr = np.full((4, 4), -1)
        ok = True
        for py in range(4):
            for px in range(4):
                # find T so that out == min((v + T) >> sh, max) for every v
                cands = []
                for t in range(1 << sh):
                    good = True
                    for v in range(256):
                        n = base + v
                        y, x = (n // 4 // 20) * 32 + ((n >> 1) & 1) * 16 + py, ((n // 4) % 20) * 32 + (n & 1) * 16 + px
                        if img[y, x] != min((v + t) >> sh, (256 >> sh) - 1):
                            good = False
                            break
                    if good:
                        cands.append(t)
                thr[py, px] = cands[0] if len(cands) == 1 else -1
                ok &= len(cands) == 1
        print(test, name, "shift", sh, "fits (v + T) >> shift:", ok, thr.tolist())
    # gray channel consistency: does gray use the same thresholds? check one value
