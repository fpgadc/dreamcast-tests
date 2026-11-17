#!/bin/bash
# bash pvr-tests/regdump/run.sh <dreamcast ip | none> [emulator build dir | none] [extra compiler flags]
source /opt/toolchains/dc/kos/environ.sh
HERE=$(cd "$(dirname "$0")" && pwd)
DCTOOL=${DCTOOL:-/home/skmp/projects/fpgadc/shrike4-rtl/build/tools/dcload-ip/host-src/tool/dc-tool-ip}
IP=${1:-none}; EMU=${2:-none}; shift 2
W=/tmp/pvr_tests/regdump; mkdir -p $W
kos-cc "$@" -c $HERE/main.c -o $W/main.o && kos-cc -o $W/main.elf $W/main.o -lm || exit 1
if [ "$IP" != none ]; then
  timeout 120 $DCTOOL -t $IP -x $W/main.elf > $W/hw.log 2>&1
  echo "console: $(grep -a -c '^REG ' $W/hw.log) registers, $(grep -a -c 'REGDUMP DONE' $W/hw.log) done"
fi
if [ "$EMU" != none ]; then
  mkdir -p $W/p
  (cd $EMU && timeout 60 ./nulldc3 --portable $W/p --config config:Headless=yes , config:Exit.AfterVblanks=400 , config:Dreamcast.RTC=1000000000 , audio:backend=none , config:pvr.backend=refsw2 , config:Debug.SerialConsoleEnabled=yes $W/main.elf > $W/emu.log 2>&1)
  echo "emulator: $(grep -a -c '^REG ' $W/emu.log) registers, $(grep -a -c 'REGDUMP DONE' $W/emu.log) done"
fi
