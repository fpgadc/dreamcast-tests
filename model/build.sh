#!/bin/bash
# Build the standalone probes and the test generators against the model's single source in nulldc3 (refsw3/refpvr).
#   bash model/build.sh [test...]
#   probes       model/covprobe, depthprobe, tspprobe, reachgen
#   generators   pvr-tests/<test>/gen from its gen.cpp, pvr-tests/<test>/read from its read.cpp (ta-tests/ too):
#                of the tests named, or of every test that has one
# Only what is older than its sources is built (the refsw3-core headers and model/gen_common.h count for all).
HERE=$(cd "$(dirname "$0")" && pwd)
REFSW=${REFSW:-$HOME/projects/fpgadc/nulldc3/refsw3/refpvr}
newer() {       # is any source newer than the program $1?
  [ ! -x "$1" ] && return 0
  for f in "${@:2}" $REFSW/pvr_*.h $HERE/gen_common.h; do [ "$f" -nt "$1" ] && return 0; done
  return 1
}
FAILED=0
for t in covprobe depthprobe tspprobe reachgen; do
  newer "$HERE/$t" "$HERE/$t.cpp" || continue
  g++ -O2 -std=c++17 -Wall -I "$REFSW" "$HERE/$t.cpp" -o "$HERE/$t.new.$$" && mv "$HERE/$t.new.$$" "$HERE/$t" && echo "built model/$t" || { rm -f "$HERE/$t.new.$$"; FAILED=1; }
done
if [ $# -gt 0 ]; then
  SRC=""; for t in "$@"; do for g in pvr-tests ta-tests; do SRC="$SRC $HERE/../$g/$t/gen.cpp $HERE/../$g/$t/read.cpp"; done; done
else
  SRC=$(ls "$HERE"/../*-tests/*/gen.cpp "$HERE"/../*-tests/*/read.cpp 2> /dev/null)
fi
for src in $SRC; do
  [ -f "$src" ] || continue
  out=${src%.cpp}
  deps=$(ls "$(dirname "$src")"/*.h 2> /dev/null | grep -v '/cases.h$')      # not the tables a generator writes
  newer "$out" "$src" $deps || continue
  g++ -O2 -std=c++17 -Wall -Wextra -I "$REFSW" -I "$HERE" -I "$(dirname "$src")" "$src" -o "$out.new.$$" && mv "$out.new.$$" "$out" \
    && echo "built $(basename "$(dirname "$src")")/$(basename "$out")" || { rm -f "$out.new.$$"; FAILED=1; }
done
exit $FAILED
