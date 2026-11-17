#!/bin/bash
# Blend test: run on the Dreamcast (dc-load) and in nulldc3 (refsw2).
#   bash run.sh <dreamcast ip | none> [emulator build dir | none]
# Results in /tmp/pvr_tests/blend: set<s>_view<v>.hw.raw (hardware) and set<s>_view<v>.emu.png (emulator).
# Then:  python3 compare.py /tmp/pvr_tests/blend        (needs numpy and Pillow)
#
# The test uses the 32 bits per pixel framebuffer (KOS PM_RGB0888). refsw2 needs its framebuffer write
# fixed for that mode, or it asserts.
source /opt/toolchains/dc/kos/environ.sh
HERE=$(cd "$(dirname "$0")" && pwd)
DCTOOL=${DCTOOL:-/home/skmp/projects/fpgadc/shrike4-rtl/build/tools/dcload-ip/host-src/tool/dc-tool-ip}
IP=${1:-none}; EMU=${2:-/tmp/nulldc3-888}
W=/tmp/pvr_tests/blend; mkdir -p $W/p $W/build

if [ "$EMU" != "none" ]; then
  for s in 0 1 2 3; do for v in 0 1 2 3; do
    n=set${s}_view$v
    ( kos-cc -DSET=$s -DVIEW=$v -c $HERE/main_kos.c -o $W/build/$n.o && kos-cc -o $W/build/$n.elf $W/build/$n.o -lm || exit 1
      mkdir -p $W/p/$n
      cd $EMU && timeout 100 ./nulldc3 --portable $W/p/$n --config config:Headless=yes , config:Exit.AfterVblanks=200 , config:Exit.FramePng=$W/$n.emu.png , config:Dreamcast.RTC=1000000000 , audio:backend=none , config:pvr.backend=refsw2 $W/build/$n.elf > $W/build/$n.log 2>&1 || echo "$n: emulator exit $?" ) &
  done; wait; done
fi
if [ "$IP" != "none" ]; then
  kos-cc "-DDUMP_DIR=\"/pc$W\"" -c $HERE/main_kos.c -o $W/build/hw.o && kos-cc -o $W/build/hw.elf $W/build/hw.o -lm || exit 1
  rm -f $W/*.hw.raw
  timeout 300 $DCTOOL -t $IP -x $W/build/hw.elf > $W/build/hw.log 2>&1
  echo "hardware: $(grep -ac 'dump: wrote' $W/build/hw.log) dumps"
fi
echo "emulator frames: $(ls $W/*.emu.png 2>/dev/null | wc -l), hardware dumps: $(ls $W/*.hw.raw 2>/dev/null | wc -l)"
