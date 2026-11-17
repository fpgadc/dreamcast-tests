"""Cases for uvrun: uvmeas's steep mix, page 6 (slopes along x with 24 significant bits, so that the rounding
leaves something to add up along a run; slopes of a few bits, as on its other pages, are exact on the grid and
cannot tell where a run starts), of two seeds, repeated for every kind of page.

    python3 pvr-tests/uvrun/gen.py [--seed N]       ->  pvr-tests/uvrun/cases.h, pvr-tests/uvrun/cases.json"""
import argparse
import importlib.util
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
KINDS = 6
spec = importlib.util.spec_from_file_location("uvgen", HERE.parent / "uvmeas" / "gen.py")
uv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(uv)

ap = argparse.ArgumentParser()
ap.add_argument("--seed", type=int, default=1)
args = ap.parse_args()
steep = [uv.steep_pages(np.random.default_rng(args.seed + k))[6] for k in range(2)]
pages = [steep[p % 2] for p in range(2 * KINDS)]
for t in (t for pg in pages for t in pg):
    t.setdefault("z", [1.0, 1.0, 1.0])
uv.write(HERE, {"seed": args.seed, "mix": "uvrun", "pages": pages})
print(f"seed {args.seed}: {len(pages)} pages, {sum(len(p) for p in pages)} triangles")
