#!/bin/bash
# hwdumps.sh [scene ...] hw=<ip>: polly2 dumps (dreamster's polly2-data) rendered by refsw3-offline and by a console
# (dumprender.sh), one after the other, the frames compared pixel by pixel: those named, else every <scene>.pvrdmp
# of the dumps folder. Each dump's frames (frame.off.raw, frame.hw.raw) and logs in /tmp/pvr_tests/hwdumps/<scene>; a
# line each in /tmp/pvr_tests/hwdumps/summary.txt. Not with another dumprender.sh at the same time (they share
# /tmp/pvr_tests/dumprender).
HERE=$(cd "$(dirname "$0")" && pwd)
DUMPS=${DUMPS:-$(cd "$HERE/../dreamster/polly2-rtl/polly2-data/dumps" 2>/dev/null && pwd)}
[ -n "$DUMPS" ] || { echo "hwdumps: no dumps (DUMPS=<folder>)"; exit 1; }
HW=""; SCENES=()
for a in "$@"; do case $a in hw=*) HW=$a ;; *) SCENES+=("$a") ;; esac; done
[ -n "$HW" ] || { sed -n 2,6p "$0"; exit 1; }
[ ${#SCENES[@]} -eq 0 ] && for f in "$DUMPS"/*.pvrdmp; do SCENES+=("$(basename "$f" .pvrdmp)"); done
OUT=/tmp/pvr_tests/hwdumps
W=/tmp/pvr_tests/dumprender
mkdir -p $OUT
for s in "${SCENES[@]}"; do
  start=$(date +%s)
  DUMPS="$DUMPS" bash "$HERE/dumprender.sh" "$s" $HW > $OUT/$s.log 2>&1
  mkdir -p $OUT/$s
  cp $W/frame.off.raw $W/frame.hw.raw $OUT/$s/ 2> /dev/null
  cp $W/off.log $OUT/$s/off.log 2> /dev/null
  cp $W/build/hw.log $OUT/$s/hw.log 2> /dev/null
  cmpline=$(grep -a "refsw3 / console now" $OUT/$s.log | sed "s/^$s: refsw3 \/ console now: //")
  errors=$(grep -ac "reports errors" $W/build/hw.log 2> /dev/null)
  echo "$s: ${cmpline:-no comparison}; PVR errors ${errors:-?}; $(( $(date +%s) - start )) s" | tee -a $OUT/summary.txt
done
