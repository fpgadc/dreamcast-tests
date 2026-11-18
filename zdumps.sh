#!/bin/bash
# zdumps.sh [scene ...] hw=<ip>: the depth buffers of polly2 dumps (dreamster's polly2-data), read out on refsw3-offline
# and on a console (dumprender.sh z), one dump after the other: those named, else every dump whose ISP samples at
# pixel centers. Each dump's readouts, pictures and logs in /tmp/pvr_tests/zdumps/<scene>; a line each in
# /tmp/pvr_tests/zdumps/summary.txt. About 1.5 minutes a dump, both sides. Not with another dumprender.sh at the
# same time (they share /tmp/pvr_tests/dumprender).
HERE=$(cd "$(dirname "$0")" && pwd)
DUMPS=${DUMPS:-$(cd "$HERE/../dreamster/polly2-rtl/polly2-data/dumps" 2>/dev/null && pwd)}
[ -n "$DUMPS" ] || { echo "zdumps: no dumps (DUMPS=<folder>)"; exit 1; }
HW=""; SCENES=()
for a in "$@"; do case $a in hw=*) HW=$a ;; *) SCENES+=("$a") ;; esac; done
[ -n "$HW" ] || { sed -n 2,6p "$0"; exit 1; }
if [ ${#SCENES[@]} -eq 0 ]; then
  for f in "$DUMPS"/pvr_regs_*.bin; do
    s=$(basename "$f" .bin); s=${s#pvr_regs_}
    python3 -c "import numpy as np, sys; sys.exit(0 if int(np.fromfile('$f', dtype='<u4')[0x80 // 4]) & 1 else 1)" && SCENES+=("$s")
  done
fi
OUT=/tmp/pvr_tests/zdumps
W=/tmp/pvr_tests/dumprender
mkdir -p $OUT
for s in "${SCENES[@]}"; do
  start=$(date +%s)
  HW_TIMEOUT=${HW_TIMEOUT:-1500} bash "$HERE/dumprender.sh" "$s" z $HW > $OUT/$s.log 2>&1
  mkdir -p $OUT/$s
  cp $W/depth* $OUT/$s/ 2> /dev/null
  cp $W/off.log $OUT/$s/off.log 2> /dev/null
  cp $W/build/hw.log $OUT/$s/hw.log 2> /dev/null
  diff=$(grep -a " depth: " $OUT/$s.log | sed "s/^$s depth: //")
  off=$(grep -a "tiles read out" $W/off.log 2> /dev/null | cut -d, -f1)
  hw=$(grep -a "tiles read out" $W/build/hw.log 2> /dev/null | tr -d '\r' | cut -d, -f1)
  foreign=$(grep -a "tiles read out" $W/build/hw.log 2> /dev/null | tr -d '\r' | sed 's/.*, //;s/ readings.*//')
  errors=$(grep -ac "reports errors" $W/build/hw.log 2> /dev/null)
  echo "$s: ${diff:-no comparison}; offline ${off:-?}, console ${hw:-?} (another colour ${foreign:-?}, PVR errors ${errors:-?}); $(( $(date +%s) - start )) s" | tee -a $OUT/summary.txt
done
