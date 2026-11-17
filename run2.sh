#!/bin/bash
# Run tests built on common/pvrtest2.h (the PVR driven directly, no KallistiOS driver).
#   bash run2.sh [-j jobs] <test>[@<set>]... [off] [hw=<dreamcast ip>] [emu[=<emulator build dir>]]
#
#   off   the test built for the PC, piped into nulldc3's refsw2-offline  ->  page<n>.off.raw
#   hw    the test built for the Dreamcast, run on a console (dc-load)    ->  page<n>.hw.raw
#   emu   the same Dreamcast program in nulldc3 (refsw2), one page each   ->  page<n>.emu.png
#
# Several tests: the offline runs go in parallel, one per CPU core (-j: another number); the console does
# one after the other; the emulator's pages run as many at a time as there are cores.
# <test>@<set>: the test with the tables of an archived set, written by regen.py <test> <set> <folder>
# into the run's own folder (the test includes CASES_H), so that sets of one test can run side by side.
#
# Results in /tmp/pvr_tests/<test>[@<set>]. A run also leaves the checksums of what it sent to the PVR
# (off.crc, hw.crc): the same lines mean that the console and the offline renderer got the same input.
# TEST_DEFS="-DX=1 ...": extra defines for the PC and the console build alike.
HERE=$(cd "$(dirname "$0")" && pwd)
NULLDC3=${NULLDC3:-$HERE/../nulldc3}
OFFLINE=${OFFLINE:-/tmp/refsw2-offline}
DCTOOL=${DCTOOL:-/home/skmp/projects/fpgadc/shrike4-rtl/build/tools/dcload-ip/host-src/tool/dc-tool-ip}
JOBS=${JOBS:-$(nproc)}

# ---- one test, one kind of run (the script calls itself for this) ----
if [ "$1" = --worker ]; then
  KIND=$2; RUN=$3; T=${RUN%%@*}; SET=""; [ "$RUN" != "$T" ] && SET=${RUN#*@}
  SRC=$HERE/$T/main.c
  PAGES=$(grep -m 1 '^#define NUM_PAGES' $SRC | awk '{print $3}')
  W=/tmp/pvr_tests/$RUN; mkdir -p $W/build
  DEF=($TEST_DEFS)                  # e.g. TEST_DEFS="-DTA_LONG=2000": extra defines for both builds
  if [ -n "$SET" ]; then
    python3 $HERE/regen.py $T $SET $W > $W/build/regen.log 2>&1 || { echo "$RUN: cannot make the tables of set $SET"; tail -n 2 $W/build/regen.log; exit 1; }
    DEF=("-DCASES_H=\"$W/cases.h\"")
  fi
  case $KIND in
  off)
    rm -f $W/*.off.raw $W/*.off16.raw
    gcc -O1 -Wall -Wno-unused-function -I$HERE/common "${DEF[@]}" -o $W/build/pc $SRC || exit 1
    $W/build/pc --labels > $W/labels.txt
    $W/build/pc $W | $OFFLINE/refsw2-offline - > $W/off.log 2>&1 || { echo "$RUN offline: failed"; tail -n 5 $W/off.log; }
    grep -a '^PVRIO crc' $W/off.log > $W/off.crc
    echo "$RUN offline: $(ls $W/*.off*.raw 2>/dev/null | wc -l) of $PAGES pages"
    ;;
  hw=*)
    source /opt/toolchains/dc/kos/environ.sh
    rm -f $W/*.hw.raw $W/*.hw16.raw $W/*.rle
    kos-cc -I$HERE/common -Wno-unused-function "-DDUMP_DIR=\"/pc$W\"" "${DEF[@]}" -c $SRC -o $W/build/hw.o && kos-cc -o $W/build/hw.elf $W/build/hw.o -lm || exit 1
    # one program on the console at a time, whoever else is running tests
    flock /tmp/pvr_tests/console.lock timeout 300 $DCTOOL -t ${KIND#hw=} -x $W/build/hw.elf > $W/build/hw.log 2>&1
    grep -a '^pvrio' $W/build/hw.log | head -n 5
    grep -a '^PVRIO crc' $W/build/hw.log | tr -d '\r' > $W/hw.crc
    python3 - $W <<'PY'
import sys, glob, os, array
for f in glob.glob(sys.argv[1] + "/*.rle"):            # pairs of (count, word)
    a = array.array("I"); a.frombytes(open(f, "rb").read())
    out = array.array("I")
    for i in range(0, len(a), 2):
        out.extend([a[i + 1]] * a[i])
    open(f[:-4], "wb").write(out.tobytes()); os.remove(f)
PY
    echo "$RUN console: $(ls $W/*.hw*.raw 2>/dev/null | wc -l) of $PAGES pages"
    ;;
  emu|emu=*)
    source /opt/toolchains/dc/kos/environ.sh
    EMU=/tmp/nulldc3-fix; [ "$KIND" != emu ] && EMU=${KIND#emu=}
    rm -f $W/*.emu.png
    for ((p = 0; p < PAGES; p++)); do
      ( kos-cc -I$HERE/common -Wno-unused-function -DPAGE=$p "${DEF[@]}" -c $SRC -o $W/build/page$p.o && kos-cc -o $W/build/page$p.elf $W/build/page$p.o -lm || exit 1
        mkdir -p $W/build/p_page$p
        cd $EMU && timeout 120 ./nulldc3 --portable $W/build/p_page$p --config config:Headless=yes , config:Exit.AfterVblanks=200 , config:Dreamcast.RTC=1000000000 , audio:backend=none , config:pvr.backend=refsw2 , config:Debug.SerialConsoleEnabled=yes , config:Exit.FramePng=$W/page$p.emu.png $W/build/page$p.elf > $W/build/page$p.log 2>&1 || echo "page $p: emulator exit $?" ) &
      (( p % JOBS == JOBS - 1 )) && wait
    done
    wait
    echo "$RUN emulator: $(ls $W/*.emu.png 2>/dev/null | wc -l) of $PAGES pages"
    ;;
  esac
  exit 0
fi

# ---- the command line ----
RUNS=(); KINDS=()
while [ $# -gt 0 ]; do
  case $1 in
  -j) JOBS=$2; shift ;;
  off|hw=*|emu|emu=*) KINDS+=("$1") ;;
  *) T=${1%%@*}
     [ -f "$HERE/$T/main.c" ] || { echo "no such test: $T"; exit 1; }
     grep -q 'pvrtest2.h\|pvrio.h' $HERE/$T/main.c || { echo "$T is not on pvrtest2.h or pvrio.h: use run.sh"; exit 1; }
     RUNS+=("$1") ;;
  esac
  shift
done
[ ${#RUNS[@]} -gt 0 ] && [ ${#KINDS[@]} -gt 0 ] || { sed -n 2,4p "$0"; exit 1; }
export JOBS NULLDC3 OFFLINE DCTOOL
for kind in "${KINDS[@]}"; do
  case $kind in
  off)
    mkdir -p /tmp/pvr_tests
    bash $NULLDC3/refsw2-offline/build.sh $OFFLINE > /tmp/pvr_tests/offline-build.log 2>&1 || { tail -n 5 /tmp/pvr_tests/offline-build.log; exit 1; }
    printf '%s\n' "${RUNS[@]}" | xargs -P $JOBS -I{} bash "$0" --worker off {}
    ;;
  *) for run in "${RUNS[@]}"; do bash "$0" --worker $kind $run; done ;;
  esac
done
for run in "${RUNS[@]}"; do
  W=/tmp/pvr_tests/$run
  if [[ " ${KINDS[*]}" == *" off"* ]] && [[ " ${KINDS[*]}" == *" hw="* ]] && [ -s $W/off.crc ] && [ -s $W/hw.crc ]; then
    cmp -s $W/off.crc $W/hw.crc && echo "$run: the console and the offline renderer got the same input ($(wc -l < $W/hw.crc) checksums)" \
                                || echo "$run: the console and the offline renderer did NOT get the same input (off.crc, hw.crc)"
  fi
done
