#!/usr/bin/env python3
"""Compare two runs of ta-tests case by case, and decode what differs.

    python3 ta-tests/ta-tests/compare_dumps.py [A] [B] [-q] [-v] [--case N] [--test ta-probe]
        A: hw (default), ref or off; B: off (default) or hw; --test: another test on this harness (ta-probe)

hw: the last console run (/tmp/pvr_tests/ta-tests), ref: the archived console run (fit/hw/ta-tests), off:
refsw2-offline's (nulldc3's tacore.cpp). Per case the TA state (TA_ISP_CURRENT, TA_NEXT_OPB, list-done and error
interrupts), the parameters and the object lists. -q: only the last line; -v: the decoded object lists of every
differing case; --case N: one case, in full."""
import gzip
import re
import zlib
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
TEST = sys.argv[sys.argv.index("--test") + 1] if "--test" in sys.argv else "ta-tests"
RUN = Path("/tmp/pvr_tests") / TEST
AREA, AREA_WORDS = 0x100000, 8192
OPB_WORDS = [0, 8, 16, 32]
LISTS = ["opaque", "opaque mod", "translucent", "translucent mod", "punch through"]
EVENTS = {7: "OP done", 8: "OM done", 9: "TR done", 10: "TM done", 21: "PT done"}
ERRORS = {0: "ISP cache", 1: "strip halt", 2: "parameters full", 3: "object lists full", 4: "illegal parameter", 5: "FIFO overflow"}

args = [a for i, a in enumerate(sys.argv[1:]) if not a.startswith("-") and sys.argv[i] != "--test"]
quiet, verbose = "-q" in sys.argv, "-v" in sys.argv
only = int(sys.argv[sys.argv.index("--case") + 1]) if "--case" in sys.argv else None
if only is not None:
    args = [a for a in args if a != str(only)]
A = args[0] if args else "hw"
B = args[1] if len(args) > 1 else "off"


def load(kind, name):
    if kind == "ref":
        f = HERE.parent.parent / "fit/hw" / TEST / f"{name}.hw.raw.gz"
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
            m = re.fullmatch(r"TACFG (\d+)((?: -?\d+){8}(?: [0-9a-f]+){5})", l.strip())
            if m:
                v = m.group(2).split()          # (a line that stderr got into does not match)
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


FEATS = ["sprite", "after a modifier volume", "clip mode 1", "clip without a rectangle", "a list without blocks",
         "growing down", "intensity", "two volumes", "clip rectangle", "clip inside or outside"]
KNOWN = 1 | 2 | 4 | 8 | 16 | 32 | 64          # features with a difference known from the fixed cases
feats = {}
for log in (RUN / "off.log",):
    if log.exists():
        for l in log.read_text(errors="replace").splitlines():
            m = re.fullmatch(r"TAFEAT (\d+) ([0-9a-f]+)", l.strip())
            if m:
                feats[int(m.group(1))] = int(m.group(2), 16)
fuzz_rows = []                                  # (case, same, features)
def hashes(kind):
    """{case: crc} of the long fuzz"""
    if kind == "ref":
        f = HERE.parent.parent / "fit/hw" / TEST / "hashes.txt.gz"
        text = gzip.open(f, "rt").read() if f.exists() else ""
    else:
        f = RUN / ("build/hw.log" if kind == "hw" else "off.log")
        text = f.read_text(errors="replace") if f.exists() else ""
    out = {}
    for l in text.splitlines():
        m = re.fullmatch(r"PVRIO hash c(\d+) ([0-9a-f]{8})", l.strip())
        if m:
            out[int(m.group(1))] = int(m.group(2), 16)
    return out


def scans(kind):
    """{case: [(address, word), ...] or [("count", n)]}: writes outside the area (ta-probe SCAN cases)"""
    f = RUN / ("build/hw.log" if kind in ("hw", "ref") else "off.log")
    if kind == "ref":
        f = HERE.parent.parent / "fit/hw" / TEST / "scans.txt.gz"
        text = gzip.open(f, "rt").read() if f.exists() else ""
    else:
        text = f.read_text(errors="replace") if f.exists() else ""
    out = {}
    for l in text.splitlines():
        m = re.fullmatch(r"PVRIO scan c(\d+) (\S+) (\S+)", l.strip())
        if m:
            out.setdefault(int(m.group(1)), []).append((m.group(2), m.group(3)))
    return out


def extras(kind, n):
    """the other dumps of a case: c<n>r (TA_OL_POINTERS), c<n>m<k>, c<n>m<k>s, c<n>m<k>r (in the middle of it)"""
    names = []
    if kind == "ref":
        found = (HERE.parent.parent / "fit/hw" / TEST).glob(f"c{n:03d}[mr]*.hw.raw.gz")
        names = [f.name[:-len(".hw.raw.gz")] for f in found]
    else:
        found = RUN.glob(f"c{n:03d}[mr]*.{kind}.raw")
        names = [f.name[:-len(f".{kind}.raw")] for f in found]
    return sorted(names)


scan_a, scan_b = scans(A), scans(B)
cases = sorted(labels)
ran = same = 0
groups = {}                                     # name without its number: [cases, same]
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
    sized = len(ma) == len(mb)
    if not sized:                               # an archive from before the case's area changed: compare what both have
        k = min(len(ma), len(mb))
        ma, mb = ma[:k], mb[:k]
    st_same = sa is not None and sb is not None and np.array_equal(sa, sb)
    diff = np.nonzero(ma != mb)[0]
    extra_diff = []                             # (name, words that differ)
    for x in extras(A, n):
        xa, xb = load(A, x), load(B, x)
        if xb is None or len(xa) != len(xb) or not np.array_equal(xa, xb):
            extra_diff.append((x, xa, xb))
    scan_diff = scan_a.get(n, []) != scan_b.get(n, [])
    in_prm = [i for i in diff if c and c["isp_base"] <= AREA + 4 * i < c["isp_limit"]]
    in_ol = [i for i in diff if i not in set(in_prm)]
    ok = st_same and not len(diff) and sized and not extra_diff and not scan_diff
    same += ok
    gname = re.sub(r"_\d+$", "", name) if re.search(r"_\d{3}$", name) else "fixed"
    groups.setdefault(gname, [0, 0])
    groups[gname][0] += 1
    groups[gname][1] += ok
    if n in feats:
        fuzz_rows.append((n, ok, feats[n]))
    tally["state"] += st_same
    tally["params"] += not in_prm
    tally["lists"] += not in_ol
    if quiet:
        continue
    if ok and only is None:
        print(f"c{n:03d} {name:22s} same")
        continue
    print(f"c{n:03d} {name:22s} {'same' if ok else 'DIFFERENT'}" + ("" if sized else " (dumps of different sizes)"))
    if not st_same or only is not None:
        print(f"      {A:3s}: {state(sa)}\n      {B:3s}: {state(sb)}")
    for x, xa, xb in extra_diff:
        if xb is None:
            print(f"      {x}: only {A} has it")
            continue
        d = np.nonzero(xa[:len(xb)] != xb[:len(xa)])[0]
        what = "TA state" if x.endswith("s") else "TA_OL_POINTERS" if x.endswith("r") else "area"
        print(f"      {x} ({what}): {len(d)} words differ: " +
              " ".join(f"[{i}] {int(xa[i]):08x}/{int(xb[i]):08x}" for i in d[:8]))
    if scan_diff or (only is not None and (scan_a.get(n) or scan_b.get(n))):
        print(f"      writes outside the area, {A}: {scan_a.get(n, [])[:10]}\n      {B}: {scan_b.get(n, [])[:10]}")
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
# ---- the long fuzz: hashes ----
ha, hb = hashes(A), hashes(B)
long_same = sum(1 for k in ha if k in hb and ha[k] == hb[k])
long_both = sum(1 for k in ha if k in hb)
if ha and hb and not quiet:
    long_cases = sorted(k for k in ha if k in hb)
    first_long = long_cases[0]
    check = [n for n, (nm, _) in labels.items() if nm == "lfuzz_check"]
    for kind, hs in ((A, ha), (B, hb)):         # the dumped repeat of the first long case: its CRC as the hash says
        if check:
            m, st = load(kind, f"c{check[0]:03d}"), load(kind, f"c{check[0]:03d}s")
            if m is not None and st is not None:
                crc = zlib.crc32(np.concatenate([m, st]).astype("<u4").tobytes())
                print(f"long fuzz, CRC check ({kind}): dump of the first case again {crc:08x}, its hash {hs.get(first_long, 0):08x}"
                      + ("  ok" if crc == hs.get(first_long) else "  <-- DIFFERENT"))
    bad = [k for k in long_cases if ha[k] != hb[k]]
    print(f"long fuzz: {A} = {B} in {long_same} of {long_both} hashed cases")
    if bad:
        print("  the different ones by what they used:")
        for b, name in enumerate(FEATS):
            d = sum(1 for k in bad if feats.get(k, 0) >> b & 1)
            print(f"    {name:28s} {d:5d} of {len(bad)}")
        une = [k for k in bad if not feats.get(k, 0) & KNOWN]
        print(f"    with none of the known differences' features: {len(une)}: {une[:30]}")
if fuzz_rows and not quiet:
    print("fuzz cases by what they used (different / same):")
    for b, name in enumerate(FEATS):
        d = sum(1 for _, ok, f in fuzz_rows if f >> b & 1 and not ok)
        sm = sum(1 for _, ok, f in fuzz_rows if f >> b & 1 and ok)
        print(f"    {name:28s} {d:4d} / {sm:4d}")
    unexplained = [n for n, ok, f in fuzz_rows if not ok and not f & KNOWN]
    print(f"    different with none of the known differences' features: {len(unexplained)}: {unexplained[:40]}")
if not quiet:
    print("by group: " + ", ".join(f"{g} {v[1]} of {v[0]}" for g, v in groups.items()))
print(f"{TEST}: {A} = {B} in {same} of {ran} cases (state {tally['state']}, parameters {tally['params']}, object lists {tally['lists']})"
      + (f"; long fuzz {long_same} of {long_both}" if long_both else ""))
