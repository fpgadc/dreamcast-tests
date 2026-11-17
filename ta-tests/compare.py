#!/usr/bin/env python3
"""Compare two runs of ta-tests case by case, and decode what differs.

    python3 ta-tests/compare.py [A] [B] [-q] [-v] [--case N]     A: hw (default), ref or off; B: off (default) or hw

hw: the last console run (/tmp/pvr_tests/ta-tests), ref: the archived console run (fit/hw/ta-tests), off:
refsw2-offline's (nulldc3's tacore.cpp). Per case the TA state (TA_ISP_CURRENT, TA_NEXT_OPB, list-done and error
interrupts), the parameters and the object lists. -q: only the last line; -v: the decoded object lists of every
differing case; --case N: one case, in full."""
import gzip
import re
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
RUN = Path("/tmp/pvr_tests/ta-tests")
AREA, AREA_WORDS = 0x100000, 8192
OPB_WORDS = [0, 8, 16, 32]
LISTS = ["opaque", "opaque mod", "translucent", "translucent mod", "punch through"]
EVENTS = {7: "OP done", 8: "OM done", 9: "TR done", 10: "TM done", 21: "PT done"}
ERRORS = {0: "ISP cache", 1: "strip halt", 2: "parameters full", 3: "object lists full", 4: "illegal parameter", 5: "FIFO overflow"}

args = [a for a in sys.argv[1:] if not a.startswith("-")]
quiet, verbose = "-q" in sys.argv, "-v" in sys.argv
only = int(sys.argv[sys.argv.index("--case") + 1]) if "--case" in sys.argv else None
if only is not None:
    args = [a for a in args if a != str(only)]
A = args[0] if args else "hw"
B = args[1] if len(args) > 1 else "off"


def load(kind, name):
    if kind == "ref":
        f = HERE.parent / "fit/hw/ta-tests" / f"{name}.hw.raw.gz"
        return np.frombuffer(gzip.open(f).read(), dtype="<u4") if f.exists() else None
    f = RUN / f"{name}.{kind}.raw"
    return np.fromfile(f, dtype="<u4") if f.exists() else None


labels = {}
for l in (RUN / "labels.txt").read_text().splitlines():
    f = l.split()
    if len(f) == 4 and f[0] == "CASE":
        labels[int(f[1])] = (f[2], int(f[3]))
cfgs = {}
for log in (RUN / "off.log", RUN / "build/hw.log"):
    if log.exists():
        for l in log.read_text(errors="replace").splitlines():
            m = re.match(r"TACFG (\d+) (.*)", l.strip())
            if m:
                v = m.group(2).split()
                cfgs[int(m.group(1))] = dict(tx=int(v[0]), ty=int(v[1]), opb=[int(x) for x in v[2:7]], down=int(v[7]),
                                             ol_base=int(v[8], 16), ol_limit=int(v[9], 16), opb_init=int(v[10], 16),
                                             isp_base=int(v[11], 16), isp_limit=int(v[12], 16))


def word(mem, addr):
    i = (addr - AREA) // 4
    return int(mem[i]) if 0 <= i < len(mem) else None


def entry(w):
    t = w >> 29
    sh = " sh" if w >> 24 & 1 else ""
    skip, off = w >> 21 & 7, w & 0x1FFFFF
    if w >> 31 == 0:
        return f"strip {w >> 25 & 0x3F:06b}{sh} skip {skip} @{off:x}"
    if t == 4:
        return f"tris {(w >> 25 & 15) + 1}{sh} skip {skip} @{off:x}"
    if t == 5:
        return f"quads {(w >> 25 & 15) + 1}{sh} skip {skip} @{off:x}"
    if w >> 28 == 0xF:
        return "end"
    if t == 7:
        return f"link {w & 0xFFFFFC:x}"
    return f"? {w:08x}"


def chain(mem, addr, size):
    """the entries of one tile and list: from its block in the matrix, along the links"""
    out, seen = [], set()
    if word(mem, addr) == 0xA5A5A5A5:
        return ["untouched"]
    while addr not in seen and len(out) < 200:
        seen.add(addr)
        nxt = None
        for k in range(size):
            w = word(mem, addr + 4 * k)
            if w is None:
                out.append("(outside the dump)")
                return out
            if w >> 28 == 0xF:
                out.append("end")
                return out
            if w >> 29 == 7:
                nxt = w & 0xFFFFFC
                out.append(f"-> {nxt:x}")
                break
            out.append(entry(w))
        if nxt is None:
            out.append("(block full, no link)")
            return out
        addr = nxt
    return out


def lists(mem, c):
    """{(list, x, y): entries} for every list with blocks"""
    res, base = {}, c["ol_base"]
    for i in range(5):
        size = OPB_WORDS[c["opb"][i]]
        if not size:
            continue
        for y in range(c["ty"]):
            for x in range(c["tx"]):
                res[(i, x, y)] = chain(mem, base + 4 * size * (y * c["tx"] + x), size)
        base += 4 * size * c["tx"] * c["ty"]
    return res


def state(s):
    if s is None:
        return "(none)"
    ev = ",".join(n for b, n in EVENTS.items() if s[2] >> b & 1) or "-"
    er = ",".join(n for b, n in ERRORS.items() if s[3] >> b & 1) or "-"
    return f"TA_ISP_CURRENT {s[0]:x} TA_NEXT_OPB {s[1]:x} events {ev} errors {er}"


cases = sorted(labels)
ran = same = 0
tally = {"state": 0, "params": 0, "lists": 0}
for n in cases:
    if only is not None and n != only:
        continue
    name, err = labels[n]
    ma, mb = load(A, f"c{n:03d}"), load(B, f"c{n:03d}")
    sa, sb = load(A, f"c{n:03d}s"), load(B, f"c{n:03d}s")
    if ma is None or mb is None:
        continue
    ran += 1
    c = cfgs.get(n)
    st_same = sa is not None and sb is not None and np.array_equal(sa, sb)
    diff = np.nonzero(ma != mb)[0]
    in_prm = [i for i in diff if c and c["isp_base"] <= AREA + 4 * i < c["isp_limit"]]
    in_ol = [i for i in diff if i not in set(in_prm)]
    ok = st_same and not len(diff)
    same += ok
    tally["state"] += st_same
    tally["params"] += not in_prm
    tally["lists"] += not in_ol
    if quiet:
        continue
    if ok and only is None:
        print(f"c{n:03d} {name:22s} same")
        continue
    print(f"c{n:03d} {name:22s} {'same' if ok else 'DIFFERENT'}")
    if not st_same or only is not None:
        print(f"      {A:3s}: {state(sa)}\n      {B:3s}: {state(sb)}")
    if in_prm:
        print(f"      parameters: {len(in_prm)} words differ")
        for i in in_prm[:12]:
            print(f"        +{4 * i + AREA - c['isp_base']:5x}: {A} {int(ma[i]):08x}  {B} {int(mb[i]):08x}")
    if c and (in_ol or only is not None):
        la, lb = lists(ma, c), lists(mb, c)
        keys = [k for k in la if la[k] != lb.get(k)]
        print(f"      object lists: {len(in_ol)} words differ, {len(keys)} tile lists")
        for k in (keys if (verbose or only is not None) else keys[:6]):
            print(f"        {LISTS[k[0]]} tile ({k[1]}, {k[2]}):\n          {A}: {'; '.join(la[k])}\n          {B}: {'; '.join(lb.get(k, []))}")
        if only is not None:
            for k, v in la.items():
                if v not in (["end"], ["untouched"]):
                    print(f"        {LISTS[k[0]]} ({k[1]}, {k[2]}): {'; '.join(v)}")
    elif in_ol:
        print(f"      object lists: {len(in_ol)} words differ (no TACFG for this case)")
print(f"ta-tests: {A} = {B} in {same} of {ran} cases (state {tally['state']}, parameters {tally['params']}, object lists {tally['lists']})")
