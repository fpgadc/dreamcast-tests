"""Score every alternative on the console's results of the ops test.

    python analyze.py [folder, default /tmp/pvr_tests/ops]

An alternative is the model with one field of one of its configurations set to another value (gen.cpp makes
the list; cases.json names each by unit and field, and gives the value). For each: how many cases can tell
it from the fitted model, and on how many of those the console agrees with the model, with the alternative,
or with neither. Also how the console compares with the model overall, and how refsw2-offline's frames
(run2.sh ops off) compare, if present.
"""
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent


def load(d, page, kind):
    f = d / f"page{page}.{kind}.raw"            # hw: a console; off: refsw2-offline
    if not f.exists():
        return None
    a = np.fromfile(f, dtype="<u4").reshape(480, 640)
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], axis=-1).astype(int)


def observe(im, c):
    p = im[int(c["py"]), int(c["px"])]
    if c["kind"] == "depth":
        return int(p[1] > 128)              # green: the quad compared equal
    if c["kind"] == "cover":
        return int(p.max() > 128)
    return int(p[1])                         # fog readout / gray level


def main():
    d = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/pvr_tests/ops")
    data = json.loads((HERE / "cases.json").read_text())
    alts, cases = data["alternatives"], data["cases"]
    for kind in ("hw", "off"):
        ims = {}
        obs = []
        for c in cases:
            if c["page"] not in ims:
                ims[c["page"]] = load(d, c["page"], kind)
            obs.append(None if ims[c["page"]] is None else observe(ims[c["page"]], c))
        if all(o is None for o in obs):
            continue
        name = "CONSOLE" if kind == "hw" else "REFSW2-OFFLINE"
        print(f"===== {name}")
        for k in ("depth", "fog", "color", "cover"):
            sel = [(c, o) for c, o in zip(cases, obs) if c["kind"] == k and o is not None]
            ok = sum(o == c["expect"] for c, o in sel)
            near = sum(abs(o - c["expect"]) <= 1 for c, o in sel)
            print(f"  {k:6s} model agrees on {ok} of {len(sel)} cases" + (f" ({near} within 1)" if k in ("fog", "color") else ""))
        if kind == "off":
            continue
        print()
        print("  unit and field / value                                    cases  model  alternative  neither")
        last = None
        for a, (op, what) in enumerate(alts):
            sel = [(c, o) for c, o in zip(cases, obs) if str(a) in c["tests"] and o is not None]
            if op != last:
                print(f"  {op}")
                last = op
            if not sel:
                print(f"      {what:52s}     0   (no input found that tells it apart)")
                continue
            m = sum(o == c["expect"] for c, o in sel)
            al = sum(o == c["tests"][str(a)] for c, o in sel)
            print(f"      {what:52s} {len(sel):5d} {m:6d} {al:12d} {len(sel) - m - al:8d}")


if __name__ == "__main__":
    main()
