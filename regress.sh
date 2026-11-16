#!/bin/bash
# regress.sh [test...]: refsw2-offline against the console, without a console and without the emulator.
#   Every test that has console dumps in fit/hw (archive_hw.sh) is built for the PC, rendered by
#   refsw2-offline (in parallel, one per CPU core) and compared region by region with those dumps; then the
#   archived sets of the measuring tests (archive_check.sh all). About half a minute for everything.
# A test whose input is no longer what the console was given (its source or the harness changed since) is
# marked: its console dumps are stale until it runs on the console again (run2.sh <test> hw=<ip>, archive_hw.sh).
cd "$(dirname "$0")"
TESTS="$@"
[ -z "$TESTS" ] && TESTS=$(ls fit/hw)
bash run2.sh $TESTS off | grep -v 'offline: [0-9]* of [0-9]* pages$'
for t in $TESTS; do
  W=/tmp/pvr_tests/$t
  note=""; cmp -s $W/off.crc fit/hw/$t/hw.crc || note="   <-- NOT the input the console was given: run it on the console again"
  echo "$(python3 compare2.py $t ref off -q 2>&1 | tail -n 1)$note"
done
[ $# -eq 0 ] && bash archive_check.sh all
exit 0
