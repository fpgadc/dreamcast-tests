#!/bin/bash
# Run one test on the Dreamcast (dc-load) and in nulldc3 (refsw2).
#   bash run.sh <test> <dreamcast ip | none> [emulator build dir | none]
# Results in /tmp/pvr_tests/<test>: page<n>.hw.raw (hardware), page<n>.emu.png (emulator), labels.txt.
# Then:  python3 compare.py <test>
#
# The tests use the 32 bits per pixel framebuffer (KOS PM_RGB0888); refsw2 needs its framebuffer write fixed
# for that mode, or it asserts.
source /opt/toolchains/dc/kos/environ.sh
HERE=$(cd "$(dirname "$0")" && pwd)
# a test's folder: pvr-tests/<test> (the core's tests) or ta-tests/<test> (the tile accelerator's)
testdir() { local g; for g in pvr-tests ta-tests; do [ -d "$HERE/$g/$1" ] && { echo "$HERE/$g/$1"; return 0; }; done; return 1; }
DCTOOL=${DCTOOL:-/home/skmp/projects/fpgadc/shrike4-rtl/build/tools/dcload-ip/host-src/tool/dc-tool-ip}
T=$1; IP=${2:-none}; EMU=${3:-/tmp/nulldc3-888}
SRC=$(testdir $T)/main.c
[ -f "$SRC" ] || { echo "no such test: $T"; exit 1; }
grep -q pvrtest2.h "$SRC" && { echo "$T drives the PVR directly (pvrtest2.h): use run2.sh"; exit 1; }
PAGES=$(grep -m 1 '^#define NUM_PAGES' $SRC | awk '{print $3}')
W=/tmp/pvr_tests/$T; mkdir -p $W/build; rm -f $W/rgb565; grep -q PM_RGB565 $SRC && touch $W/rgb565
CC="kos-cc -I$HERE/common -Wno-unused-function"

build() {   # name, flags...
  n=$1; shift
  $CC "$@" -c $SRC -o $W/build/$n.o && kos-cc -o $W/build/$n.elf $W/build/$n.o -lm
}
emu() {     # name, vblanks, extra config...
  n=$1; vb=$2; shift 2
  mkdir -p $W/build/p_$n
  (cd $EMU && timeout 120 ./nulldc3 --portable $W/build/p_$n --config config:Headless=yes , config:Exit.AfterVblanks=$vb , config:Dreamcast.RTC=1000000000 , audio:backend=none , config:pvr.backend=refsw2 , config:Debug.SerialConsoleEnabled=yes "$@" $W/build/$n.elf > $W/build/$n.log 2>&1)
}

build hw "-DDUMP_DIR=\"/pc$W\"" || exit 1
if [ "$EMU" != "none" ]; then
  rm -f $W/*.emu.png
  ( build labels -DLABELS && emu labels 300; grep -a '^LABEL ' $W/build/labels.log | sort -u > $W/labels.txt ) &
  for ((p = 0; p < PAGES; p++)); do
    ( build page$p -DPAGE=$p && emu page$p 200 , config:Exit.FramePng=$W/page$p.emu.png || echo "page $p: emulator exit $?" ) &
    (( p % 8 == 7 )) && wait
  done
  wait
fi
if [ "$IP" != "none" ]; then
  rm -f $W/*.hw.raw $W/*.hw16.raw
  timeout 300 $DCTOOL -t $IP -x $W/build/hw.elf > $W/build/hw.log 2>&1
fi
echo "$T: $PAGES pages, emulator frames $(ls $W/*.emu.png 2>/dev/null | wc -l), hardware dumps $(ls $W/*.hw*.raw 2>/dev/null | wc -l), labels $(wc -l < $W/labels.txt 2>/dev/null)"
