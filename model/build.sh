#!/bin/bash
# Build the standalone probes and the test generators against the model's single source in nulldc3 (refsw2).
#   bash model/build.sh [test...]
#   probes       model/covprobe, depthprobe, tspprobe
#   generators   <test>/gen from <test>/gen.cpp, <test>/read from <test>/read.cpp: of the tests named, or of
#                every test that has one
# Only what is older than its sources is built (the refsw2 headers and model/gen_common.h count for all).
HERE=$(cd "$(dirname "$0")" && pwd)
REFSW=${REFSW:-$HOME/projects/fpgadc/nulldc3/libswirl/rend/refsw2}
newer() {       # is any source newer than the program $1?
  [ ! -x "$1" ] && return 0
  for f in "${@:2}" $REFSW/pvr_*.h $HERE/gen_common.h; do [ "$f" -nt "$1" ] && return 0; done
  return 1
}
FAILED=0
for t in covprobe depthprobe tspprobe; do
  newer "$HERE/$t" "$HERE/$t.cpp" || continue
  g++ -O2 -std=c++17 -Wall -I "$REFSW" "$HERE/$t.cpp" -o "$HERE/$t.new.$$" && mv "$HERE/$t.new.$$" "$HERE/$t" && echo "built model/$t" || { rm -f "$HERE/$t.new.$$"; FAILED=1; }
done
if [ $# -gt 0 ]; then
  SRC=""; for t in "$@"; do SRC="$SRC $HERE/../$t/gen.cpp $HERE/../$t/read.cpp"; done
else
  SRC=$(ls "$HERE"/../*/gen.cpp "$HERE"/../*/read.cpp 2> /dev/null)
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
