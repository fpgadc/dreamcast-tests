"""Where did each page's render stop? Reads the console's dumps of pvr-tests/bumphang (run2.sh bumphang hw=<ip>).

    python3 pvr-tests/bumphang/analyze.py

A tile whose 4x4 marker does not have the page's colour was not written. The tiles are rendered column by
column: the first tile not written is where the render stopped."""
from pathlib import Path

import numpy as np

d = Path("/tmp/pvr_tests/bumphang")
names = [l.split(" ", 5)[5].strip() for l in (d / "labels.txt").read_text().splitlines() if l.startswith("LABEL ") and l.split(" ")[2] == "0"]
for page, name in enumerate(names):
    hw = np.fromfile(d / f"page{page}.hw.raw", dtype="<u4").reshape(480, 640) & 0xffffff
    want = (0x203040 + page * 0x0b0705) & 0xffffff
    order = [(x, y) for x in range(20) for y in range(15)]
    stale = [i for i, (x, y) in enumerate(order) if hw[y * 32 + 1, x * 32 + 1] != want]
    if not stale:
        print(f"page {page:2d} {name}: every tile written")
    else:
        i = stale[0]
        after = 300 - i - len(stale)
        print(f"page {page:2d} {name}: stops at tile {i} (column {order[i][0]}, row {order[i][1]}); {after} tiles written after it")
