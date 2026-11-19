"""setupfit.py <TM_ORDER log> [--sorted]: the render time of the order scenes against a cache of K set-up primitives
(least recently used goes): clocks = B a tile + M a miss + F more for a primitive's first miss + P a pixel, the
runs taken in raster order (SPAN_SORT_CFG 0: the span sorter off) or in the span sorter's order (--sorted).
Every scene is 1024 pixels a tile, so B and P are one number. README "spansortcfg".

    python3 pvr-tests/timemeas/setupfit.py fit/timemeas_logs/timemeas_spansort0_order.log
    python3 pvr-tests/timemeas/setupfit.py fit/timemeas_logs/timemeas_spansort101_order.log --sorted"""
import os, sys, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import order
R = order.load(sys.argv[1])[0]
srt = "--sorted" in sys.argv
def feats(k, ntiles, K):
    miss = first = px = 0
    for t in range(ntiles):
        runs = order.runs_of(order.tags(k, t))
        if srt: runs = order.sort_runs(runs)[0]
        lru, seen = [], set()
        for (p, y, x0, n) in runs:
            px += n
            if p in lru: lru.remove(p); lru.append(p); continue
            miss += 1
            if p not in seen: first += 1; seen.add(p)
            lru.append(p)
            if len(lru) > K: lru.pop(0)
    return [ntiles, miss, first, px]
keys = [key for key in sorted(R) if order.tags(key[1]) is not None]
y = np.array([R[key]["counts"] / 2 for key in keys])
best = None
for K in range(1, 33):
    A = np.array([feats(key[1], len(R[key]["tiles"]), K) for key in keys], float)
    coef, *_ = np.linalg.lstsq(A, y, rcond=None)
    err = np.abs(A @ coef - y)
    if best is None or err.max() < best[0]: best = (err.max(), K, coef, A)
    print(f"K {K:2d}: worst {err.max():7.0f}  rms {np.sqrt((err**2).mean()):6.0f}   B {coef[0]:6.0f} M {coef[1]:5.1f} F {coef[2]:5.1f} P {coef[3]:5.2f}")
e, K, coef, A = best
print(f"best K {K}")
for key, a, yy in zip(keys, A, y):
    print(f"  scene {key[1]:2d}: misses {int(a[1]):5d} first {int(a[2]):5d} px {int(a[3]):5d}  clocks {yy:7.0f} model {a @ coef:7.0f}  {yy - a @ coef:+6.0f}")
