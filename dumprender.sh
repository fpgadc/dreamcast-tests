#!/bin/bash
# dumprender.sh <scene> [x,y ...] [drop=<tag>,...] [reg=<offset>:<value> ...] [poke=<address>:<value> ...] [nolists=<mask>] [z] [hw=<ip>]: a polly2 dump (dreamster's polly2-data) rendered by refsw2-offline
# and, with hw=, by a console: the whole frame, or only the region array entries of the tiles given (a partial
# render; drop=: the objects of these refsw2 tags left out, hex, as its render log prints them; needs tiles).
# poke=: a word of video memory written after the dump's (32 bit area address, hex: e.g. a vertex's colour in the
# parameters, to read something else out of the same polygon). nolists=: lists left out of the kept entries (1 OP, 2 OP volumes, 4 TR, 8 TR volumes, 16 PT). reg=: a register written after the dump's, hex (reg=f4:400: no vertical scaling). The framebuffers are compared pixel by pixel, and without tiles also with the console render that
# polly2-data keeps (vram_out_<scene>.bin). z: the depth buffer read out instead (common/zexfil.h), of every tile or
# of the tiles given, 27 renders a tile; depth.off.raw and depth.hw.raw compared depth by depth.
# A scene can also be <scene>.pvrdmp (a PVR core dump; main.c inflates it, common/gunzip.h). pvr-tests/dumprender/main.c;
# output in /tmp/pvr_tests/dumprender.
HERE=$(cd "$(dirname "$0")" && pwd)
DUMPS=${DUMPS:-$(cd "$HERE/../dreamster/polly2-rtl/polly2-data/dumps" 2>/dev/null && pwd)}
[ -n "$DUMPS" ] || { echo "dumprender: no dumps (DUMPS=<folder>)"; exit 1; }
SCENE=$1; shift
# a PVR core dump (<scene>.pvrdmp: nulldc3's dump key, Script.File) when there is no vram_<scene>.bin: main.c with PVRDMP
PVRDMP=""
[ ! -f "$DUMPS/vram_$SCENE.bin" ] && [ -f "$DUMPS/$SCENE.pvrdmp" ] && PVRDMP=1
TILES=(); HW=""; ZX=""
DROP=""; SETREGS=(); NOL=""; POKES=()
for a in "$@"; do case $a in hw=*) HW=$a ;; z) ZX=1 ;; drop=*) DROP=${a#drop=} ;; nolists=*) NOL=${a#nolists=} ;; reg=*) r=${a#reg=}; SETREGS+=("0x${r%%:*}" "0x${r#*:}") ;; poke=*) r=${a#poke=}; POKES+=("0x${r%%:*}" "0x${r#*:}") ;; *,*) TILES+=("$a") ;; *) echo "dumprender: what is $a?"; exit 1 ;; esac; done
DEFS="-DDUMPS=\"$DUMPS\" -DSCENE=\"$SCENE\""
[ ${#TILES[@]} -gt 0 ] && DEFS="$DEFS -DTILES={$(IFS=,; echo "${TILES[*]}")} -DNTILES=${#TILES[@]}"
[ -n "$NOL" ] && DEFS="$DEFS -DNOLISTS=$NOL"
[ -n "$ZX" ] && DEFS="$DEFS -DZEXFIL"
[ -n "$PVRDMP" ] && DEFS="$DEFS -DPVRDMP"
[ ${#SETREGS[@]} -gt 0 ] && DEFS="$DEFS -DSETREGS={$(IFS=,; echo "${SETREGS[*]}")}"
[ ${#POKES[@]} -gt 0 ] && DEFS="$DEFS -DPOKES={$(IFS=,; echo "${POKES[*]}")}"
[ -n "$DROP" ] && DEFS="$DEFS -DDROP={$(echo $DROP | sed 's/\([0-9a-fA-F]\+\)/0x\1/g')}"
W=/tmp/pvr_tests/dumprender
rm -f $W/frame.*
TEST_DEFS="$DEFS" bash "$HERE/run2.sh" dumprender off | grep -v "offline: 0 of 0 pages"
grep -a "dumprender:" $W/off.log
[ -n "$HW" ] && { TEST_DEFS="$DEFS" bash "$HERE/run2.sh" dumprender $HW | grep -v "console: 0 of 0 pages"; grep -a "dumprender:\|pvrio:" $W/build/hw.log | tr -d '\r'; }
if [ -n "$ZX" ]; then
  python3 - "$W" "$SCENE" <<'PY'
import sys, os, numpy as np
W, scene = sys.argv[1], sys.argv[2]
for name in ("depth", "depthr"):
    a, b = (f"{W}/{name}.{k}.raw" for k in ("hw", "off"))
    if not os.path.exists(a) or not os.path.exists(b):
        continue
    hw, off = (np.fromfile(f, dtype="<u4").reshape(480, 640) for f in (a, b))
    d = np.argwhere(hw != off)
    print(f"{scene} {name}: {len(d)} of {hw.size} depths differ (console / refsw3)")
    tiles = {}
    for y, x in d:
        tiles.setdefault((x // 32 + (20 if name == "depthr" else 0), y // 32), []).append((x, y))
    for (tx, ty), px in sorted(tiles.items(), key=lambda t: -len(t[1]))[:12]:
        x, y = px[0]
        print(f"  tile ({tx}, {ty}): {len(px)}; first ({x + (640 if name == 'depthr' else 0)}, {y}): hw {hw[y, x]:08x} off {off[y, x]:08x}")
PY
  # pictures (depthpng.py): each side on refsw3's scale, and the console's with the depths that differ boxed in red
  for n in depth depthr; do
    [ -f $W/$n.off.raw ] || continue
    python3 "$HERE/depthpng.py" $W/$n.off.raw > /dev/null
    [ -f $W/$n.hw.raw ] && python3 "$HERE/depthpng.py" $W/$n.hw.raw --like $W/$n.off.raw > /dev/null \
                        && python3 "$HERE/depthpng.py" $W/$n.hw.raw $W/$n.diff.png --like $W/$n.off.raw --diff $W/$n.off.raw > /dev/null
  done
  echo "pictures: $(cd $W && ls depth*.png | tr '\n' ' ')(in $W)"
  exit 0
fi
DR_SETREGS="$(for ((i = 0; i < ${#SETREGS[@]}; i += 2)); do printf "%x:%x " ${SETREGS[i]} ${SETREGS[i+1]}; done)" python3 - "$DUMPS" "$SCENE" "$W" "${#TILES[@]}" <<'PY'
import sys, os, numpy as np
dumps, scene, W, ntiles = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
if os.path.exists(f"{dumps}/pvr_regs_{scene}.bin"):
    r = np.fromfile(f"{dumps}/pvr_regs_{scene}.bin", dtype=np.uint32)
else:                                       # a .pvrdmp: the registers are its second gzip member
    import zlib
    d = zlib.decompressobj(31); d.decompress(open(f"{dumps}/{scene}.pvrdmp", "rb").read()[11:])
    r = np.frombuffer(zlib.decompressobj(31).decompress(d.unused_data), dtype=np.uint32).copy()
for a in os.environ.get('DR_SETREGS', '').split():
    k, v = a.split(':'); r[int(k, 16) // 4] = int(v, 16)
sof = int(r[0x60 // 4]) & 0xfffffc
def frame(path):
    return np.fromfile(path, dtype=np.uint8) if os.path.exists(path) else None
off, hw = frame(f"{W}/frame.off.raw"), frame(f"{W}/frame.hw.raw")
ref = None
if ntiles == 0 and off is not None and os.path.exists(f"{dumps}/vram_out_{scene}.bin"):     # polly2-data's console render, if it has one
    ref = np.fromfile(f"{dumps}/vram_out_{scene}.bin", dtype=np.uint8)[sof:sof + len(off)]
bpp = {0: 2, 1: 2, 2: 2, 3: 2, 4: 3, 5: 4, 6: 4}[int(r[0x48 // 4]) & 7]
def px(a): return a[:len(a) // bpp * bpp].reshape(-1, bpp)
for name, a, b in (("refsw3 / console now", off, hw), ("refsw3 / polly2-data's console render", off, ref), ("console now / polly2-data's", hw, ref)):
    if a is None or b is None: continue
    d = (px(a) != px(b)).any(-1)
    print(f"{scene}: {name}: {int(d.sum())} of {len(d)} pixels differ")
PY
