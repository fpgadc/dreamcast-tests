#!/bin/bash
# Re-run every test in the emulator only and compare with the hardware dumps already in /tmp/pvr_tests.
#   bash score.sh <emulator build dir> [python with numpy and Pillow] [tests...]
HERE=$(cd "$(dirname "$0")" && pwd)
EMU=${1:-/tmp/nulldc3-fix}; PY=${2:-python3}; shift 2
TESTS=${@:-texfmt sample shade depth raster env alphatest fog bump mip modvol sort sprite fb565 fb555 precision huge}
( cd $HERE/blend && bash run.sh none $EMU > /tmp/pvr_tests/blend/score.log 2>&1 ) &
for t in $TESTS; do
  bash $HERE/run.sh $t none $EMU > /tmp/pvr_tests/$t.score.log 2>&1
  grep -a -h ' in [A-Za-z_]* ->' /tmp/pvr_tests/$t/build/page*.log 2>/dev/null | sort | uniq -c | sed "s/^/   $t assert:/"
done
wait
for t in $TESTS; do $PY $HERE/compare.py $t -q | tail -n 1; done
$PY $HERE/blend/compare.py /tmp/pvr_tests/blend | sed -n 2p
