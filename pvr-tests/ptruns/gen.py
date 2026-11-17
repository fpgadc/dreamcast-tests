"""Cases for ptruns: uvmeas's steep mix, page 6 (slopes along x with 24 significant bits: where a run starts
shows in the readings), of two seeds, for every kind of page (as pvr-tests/uvrun).

    python3 pvr-tests/ptruns/gen.py [--seed N]      ->  pvr-tests/ptruns/cases.h, pvr-tests/ptruns/cases.json"""
import argparse
import importlib.util
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
KINDS = 4
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
uv.write(HERE, {"seed": args.seed, "mix": "ptruns", "pages": pages})
print(f"seed {args.seed}: {len(pages)} pages, {sum(len(p) for p in pages)} triangles")
