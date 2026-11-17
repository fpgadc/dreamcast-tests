"""The palette clock (timemeas TM_ORDER, REPORT.md "In what order the TSP shades a tile"): when each pixel of a
tile was shaded, and the span sorter model that orders them.

    python3 pvr-tests/timemeas/order.py <log> [scene ...]     the tiles as counters less the tile's least
    python3 pvr-tests/timemeas/order.py --fit <log>           every scene against the model (windows 92 .. 100)
    python3 pvr-tests/timemeas/order.py --drains <log> <scene> the model's drains and the console's times of them

A pixel's value is the number of palette writes since STARTRENDER when the TSP read the palette for it (about
25 counts, 12 clocks, a write). Glitch: about one pixel a scene holds a value 116 to 120 writes too old.

The model (span sorter): the tile's runs (maximal stretches of one primitive on one line) enter a window of 96
runs in raster order as soon as there is room. Repeatedly: take the oldest run in the window; shade it and every
run of the same primitive in the window, oldest first, including runs that enter while it is being drained."""
import sys

WINDOW = 96


def load(path):
    S, P, cur = {}, {}, None
    for line in open(path, errors="replace"):
        p = line.split()
        if not p:
            continue
        if p[0] == "O":
            cur = (p[1], int(p[2]))
            S[cur] = {"writes": int(p[5]), "counts": int(p[7]), "tiles": {}}
        elif p[0] == "R" and cur:
            S[cur]["tiles"].setdefault(int(p[1]), [None] * 32)[int(p[2])] = [int(x, 16) for x in p[3:35]]
        elif p[0] == "P":
            P[int(p[1])] = [int(x) for x in p[2:]]
    return S, P


def tags(k):
    """Which primitive each pixel of scene k shows (the identities only; see main.c TM_ORDER). None: unknown."""
    g = lambda f: [[f(x, y) for x in range(32)] for y in range(32)]
    return {
        0: g(lambda x, y: 0), 1: g(lambda x, y: 0), 2: g(lambda x, y: y),
        3: g(lambda x, y: x), 4: g(lambda x, y: x), 5: g(lambda x, y: x // 8),
        6: g(lambda x, y: 1 if x == 8 else 2 if x == 16 else 0),
        7: g(lambda x, y: 32 * y + x), 8: g(lambda x, y: 32 * y + x), 9: g(lambda x, y: (y // 8) * 4 + x // 8),
        10: g(lambda x, y: 0 if y >= 3 or (x, y) == (0, 0) else 1 + 32 * y + x),
        11: g(lambda x, y: (y // 16) * 32 + x), 12: g(lambda x, y: (y // 2) * 32 + x),
        13: g(lambda x, y: min(x, 16)), 14: g(lambda x, y: min(x, 8)),
        15: g(lambda x, y: 100 + y if y % 2 else x), 16: g(lambda x, y: 32 + 64 * y + x if y % 2 else x),
        17: g(lambda x, y: (y // 4) * 32 + x), 18: g(lambda x, y: x // 2 if x % 2 == 0 else 99),
        19: g(lambda x, y: x), 20: g(lambda x, y: (y // 16) * 32 + x), 21: g(lambda x, y: x if y < 16 else 32),
        22: g(lambda x, y: x),
    }.get(k)


def runs_of(tm):
    out = []
    for y in range(32):
        x = 0
        while x < 32:
            t, x0 = tm[y][x], x
            while x < 32 and tm[y][x] == t:
                x += 1
            out.append((t, y, x0, x - x0))
    return out


def sort_runs(runs, window=WINDOW):
    """The span sorter: the runs in the order they are shaded, and the drains (primitive, runs)."""
    buf, order, drains, nxt = [], [], [], 0

    def fill():
        nonlocal nxt
        while nxt < len(runs) and len(buf) < window:
            buf.append(runs[nxt])
            nxt += 1
    fill()
    while buf:
        t = buf[0][0]
        drains.append((t, []))
        while True:
            mine = [r for r in buf if r[0] == t]
            if not mine:
                break
            buf.remove(mine[0])
            order.append(mine[0])
            drains[-1][1].append(mine[0])
            fill()
    return order, drains


def violations(grid, order):
    """Runs the model puts after a run the console shaded later; glitch pixels apart (112 to 124 writes under both
    neighbours along the model's order: every glitch seen is 116 to 120 writes stale)."""
    seq = [grid[y][x0] for (t, y, x0, n) in order]
    bad = glitch = 0
    for i in range(1, len(seq)):
        if seq[i] < seq[i - 1]:
            nxt = seq[i + 1] if i + 1 < len(seq) else seq[i - 1]
            if 112 <= seq[i - 1] - seq[i] <= 124 and 112 <= nxt - seq[i] <= 124:
                glitch += 1
            else:
                bad += 1
    return bad, glitch


def main():
    a = sys.argv[1:]
    if a and a[0] == "--fit":
        S, _ = load(a[1])
        print("runs out of order along the model's order, by window; at 96: of them glitch pixels, and the rest")
        print("scene tile runs   92   94   95   96   97   98  100 | glitch  rest")
        tot = [0] * 9
        for (what, k), sc in sorted(S.items()):
            tm = tags(k)
            if tm is None:
                continue
            for t, grid in sorted(sc["tiles"].items()):
                runs = runs_of(tm)
                raw = [sum(violations(grid, sort_runs(runs, w)[0])) for w in (92, 94, 95, 96, 97, 98, 100)]
                bad, glitch = violations(grid, sort_runs(runs, WINDOW)[0])
                tot = [a + b for a, b in zip(tot, raw + [glitch, bad])]
                print(f"{k:5d} {t:4d} {len(runs):4d} " + " ".join(f"{v:4d}" for v in raw) + f" | {glitch:6d} {bad:5d}")
        print(" all           " + " ".join(f"{v:4d}" for v in tot[:7]) + f" | {tot[7]:6d} {tot[8]:5d}")
        return
    if a and a[0] == "--drains":
        S, _ = load(a[1])
        k = int(a[2])
        grid = S[("order", k)]["tiles"][0]
        lo = min(min(r) for r in grid)
        _, drains = sort_runs(runs_of(tags(k)))
        prev = -1
        for i, (t, rs) in enumerate(drains):
            ts = sorted(grid[y][x] - lo for (_, y, x, n) in rs)
            ts = [v for v in ts if v > ts[len(ts) // 2] - 60] or ts
            early = ts[0] < prev - 1
            print(f"  #{i:3d} primitive {t:4d} lines {rs[0][1]:2d}-{rs[-1][1]:2d} {len(rs):3d} runs: "
                  f"{ts[0]}-{ts[-1]}{'   <-- before the drains in front of it' if early else ''}")
            if not early:
                prev = max(prev, ts[-1])
        return
    S, _ = load(a[0])
    want = set(int(x) for x in a[1:])
    for (what, k), sc in sorted(S.items()):
        if want and k not in want:
            continue
        print(f"{what} {k}: {sc['writes']} writes in {sc['counts']} counts ({sc['counts'] / max(sc['writes'], 1):.1f} a write)")
        for t, g in sorted(sc["tiles"].items()):
            lo = min(min(r) for r in g)
            print(f"  tile {t}: least {lo}, most {max(max(r) for r in g)}")
            for y, r in enumerate(g):
                print(f"  {y:2d} " + " ".join(f"{v - lo:3d}" for v in r))


if __name__ == "__main__":
    main()
