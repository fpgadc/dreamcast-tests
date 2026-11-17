#!/bin/bash
# dumprender.sh <scene> [x,y ...] [hw=<ip>]: a polly2 dump (dreamster's polly2-data) rendered by refsw2-offline
# and, with hw=, by a console: the whole frame, or only the region array entries of the tiles given (a partial
# render). The framebuffers are compared pixel by pixel, and without tiles also with the console render that
# polly2-data keeps (vram_out_<scene>.bin). pvr-tests/dumprender/main.c; output in /tmp/pvr_tests/dumprender.
HERE=$(cd "$(dirname "$0")" && pwd)
DUMPS=${DUMPS:-$(cd "$HERE/../dreamster/polly2-rtl/polly2-data/dumps" 2>/dev/null && pwd)}
[ -n "$DUMPS" ] || { echo "dumprender: no dumps (DUMPS=<folder>)"; exit 1; }
SCENE=$1; shift
TILES=(); HW=""
for a in "$@"; do case $a in hw=*) HW=$a ;; *,*) TILES+=("$a") ;; *) echo "dumprender: what is $a?"; exit 1 ;; esac; done
DEFS="-DDUMPS=\"$DUMPS\" -DSCENE=\"$SCENE\""
[ ${#TILES[@]} -gt 0 ] && DEFS="$DEFS -DTILES={$(IFS=,; echo "${TILES[*]}")} -DNTILES=${#TILES[@]}"
W=/tmp/pvr_tests/dumprender
rm -f $W/frame.*
TEST_DEFS="$DEFS" bash "$HERE/run2.sh" dumprender off | grep -v "offline: 0 of 0 pages"
grep -a "dumprender:" $W/off.log
[ -n "$HW" ] && { TEST_DEFS="$DEFS" bash "$HERE/run2.sh" dumprender $HW | grep -v "console: 0 of 0 pages"; grep -a "dumprender:\|pvrio:" $W/build/hw.log | tr -d '\r'; }
python3 - "$DUMPS" "$SCENE" "$W" "${#TILES[@]}" <<'PY'
import sys, os, numpy as np
dumps, scene, W, ntiles = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
r = np.fromfile(f"{dumps}/pvr_regs_{scene}.bin", dtype=np.uint32)
sof = int(r[0x60 // 4]) & 0xfffffc
def frame(path):
    return np.fromfile(path, dtype=np.uint8) if os.path.exists(path) else None
off, hw = frame(f"{W}/frame.off.raw"), frame(f"{W}/frame.hw.raw")
ref = None
if ntiles == 0 and off is not None:
    ref = np.fromfile(f"{dumps}/vram_out_{scene}.bin", dtype=np.uint8)[sof:sof + len(off)]
bpp = {0: 2, 1: 2, 2: 2, 3: 2, 4: 3, 5: 4, 6: 4}[int(r[0x48 // 4]) & 7]
def px(a): return a[:len(a) // bpp * bpp].reshape(-1, bpp)
for name, a, b in (("refsw2 / console now", off, hw), ("refsw2 / polly2-data's console render", off, ref), ("console now / polly2-data's", hw, ref)):
    if a is None or b is None: continue
    d = (px(a) != px(b)).any(-1)
    print(f"{scene}: {name}: {int(d.sum())} of {len(d)} pixels differ")
PY
