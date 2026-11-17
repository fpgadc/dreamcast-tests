#!/usr/bin/env python3
"""ta-perf: the console's times as costs. analyze.py [hw.log]

For each case: a least squares line through its three points, done (the end of list interrupt, least of the
repetitions) = a + b items, with the input by store queues (sq) and by DMA. Counts are of 5 ns. Also per 32 byte
block of input, and how much of the time the TA was still busy after the last block went in (done - sent)."""
import sys, collections

log = sys.argv[1] if len(sys.argv) > 1 else '/tmp/pvr_tests/ta-perf/build/hw.log'
pts = collections.defaultdict(list)
for line in open(log, errors='replace'):
    w = line.strip().split()
    if len(w) == 10 and w[0] == 'P':
        name, n, blocks, items, s0, s1, d0, d1, err = w[1], *map(int, w[2:9]), w[9]
        pts[name].append(dict(n=n, blocks=blocks, items=items, sent=s0, done=d0, done_med=d1, err=err))


def fit(xs, ys):
    k = len(xs)
    mx, my = sum(xs) / k, sum(ys) / k
    sxx = sum((x - mx) ** 2 for x in xs)
    if sxx == 0:
        return my, 0.0, 0.0
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx
    a = my - b * mx
    worst = max(abs(y - (a + b * x)) for x, y in zip(xs, ys))
    return a, b, worst


def row(name):
    p = pts[name]
    a, b, worst = fit([q['items'] for q in p], [q['done'] for q in p])
    _, bb, _ = fit([q['blocks'] for q in p], [q['done'] for q in p])
    last = p[-1]
    busy = last['done'] - last['sent']
    errs = ','.join(sorted(set(q['err'] for q in p)))
    return a, b, bb, worst, busy, errs


print('%-18s %26s   %26s' % ('', 'store queues', 'DMA'))
print('%-18s %8s %8s %8s   %8s %8s %8s  %s' % ('case', 'a', 'b/item', 'b/block', 'a', 'b/item', 'b/block', 'per item (DMA, ns)'))
for name in pts:
    if name.startswith('dma_') and name[4:] in pts:
        continue
    r1 = row(name)
    r2 = row('dma_' + name) if 'dma_' + name in pts else None
    s = '%-18s %8.0f %8.2f %8.2f' % (name, r1[0], r1[1], r1[2])
    if r2:
        s += '   %8.0f %8.2f %8.2f  %7.1f' % (r2[0], r2[1], r2[2], r2[1] * 5)
    if max(r1[3], r2[3] if r2 else 0) > 100:
        s += '   (off the line by %d)' % max(r1[3], r2[3] if r2 else 0)
    if r1[5] != '0' or (r2 and r2[5] != '0'):
        s += '   ERRORS %s %s' % (r1[5], r2[5] if r2 else '')
    print(s)
