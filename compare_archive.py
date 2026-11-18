"""Compare a run of a measuring test with the console's archived run of the same set.

    python3 compare_archive.py <test> <set in fit/<test>_data> [off | emu | hw] [folder, default /tmp/pvr_tests/<test>]

Reads <test>.<kind>.npz (<test>.npz for hw), written by the test's read.py."""
import os
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
# test: (what has to be the same for the two runs to be of the same cases, what was measured)
TESTS = {
    "covmeas": (("tx", "ty"), ("cov", "isp")),
    "uvmeas": (("tx", "ty", "tu"), ("t",)),
    "uvspan": (("tx", "ty", "tu"), ("t",)),
    "wmeas": (("tx", "ty", "tz"), ("g",)),
    "cmeas": (("tx", "ty", "tc"), ("rgb",)),
    "mipmeas": (("su", "sv", "bias", "mode", "z"), ("rgb",)),
}
WHAT = dict(cov="pixels drawn or not", isp="pixels with depth written or not", t="texel readings", g="gray levels", rgb="colour values")

test, name = sys.argv[1], sys.argv[2]
kind = sys.argv[3] if len(sys.argv) > 3 else "off"
d = Path(sys.argv[4]) if len(sys.argv) > 4 else Path("/tmp/pvr_tests") / test
who = dict(off=os.environ.get("OFFLINE_TOOL", "refsw3-offline"), emu="emulator", hw="this console run")[kind]
if test == "zmeas":         # its readings are brackets (lo, hi) of the stored depth per pixel, in meas.json
    import json
    h = json.loads((HERE / f"fit/zmeas_data/{name}.meas.json").read_text())["cases"]
    e = json.loads((d / ("meas.json" if kind == "hw" else f"meas.{kind}.json")).read_text())["cases"]
    pairs = [(a, b) for x, y in zip(h, e) for a, b in zip(x["pixels"], y["pixels"])]
    if len(h) != len(e) or any((a["px"], a["py"]) != (b["px"], b["py"]) for a, b in pairs):
        sys.exit(f"zmeas {name}: this run is not of the archived cases")
    n = sum((a["lo"], a["hi"]) == (b["lo"], b["hi"]) for a, b in pairs)
    print(f"zmeas {name}: {who} = archived console on {n} of {len(pairs)} depth brackets" + ("" if n == len(pairs) else f" ({len(pairs) - n} DIFFER)"))
    sys.exit(0)
h = np.load(HERE / f"fit/{test}_data/{name}.npz")
e = np.load(d / (f"{test}.npz" if kind == "hw" else f"{test}.{kind}.npz"))
same, measured = TESTS[test]
if any(h[k].shape != e[k].shape or not np.array_equal(h[k], e[k], equal_nan=h[k].dtype.kind == "f") for k in same):      # a NaN depth is the same case
    sys.exit(f"{test} {name}: this run is not of the archived cases")
parts = []
for k in measured:
    if k in h.files and k in e.files:
        n = int((h[k] == e[k]).sum())
        parts.append(f"{n} of {h[k].size} {WHAT[k]}" + ("" if n == h[k].size else f" ({h[k].size - n} DIFFER)"))
print(f"{test} {name}: {who} = archived console on " + "; ".join(parts))
