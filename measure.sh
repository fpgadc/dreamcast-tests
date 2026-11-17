#!/bin/bash
# measure.sh <test> <set name> [generator arguments...]: one round of measuring on a console.
#   generates the cases (<test>/gen, or gen.py), runs them on the console and in refsw2-offline, archives the
#   console's readings as fit/<test>_data/<set name>.{npz,cases.json} and says how refsw2 compares.
# For the measuring tests on pvrtest2.h (covmeas, uvmeas, uvspan, wmeas, cmeas). DC=<ip> for another console.
cd "$(dirname "$0")"
# a test's folder: pvr-tests/<test> (the core's tests) or ta-tests/<test> (the tile accelerator's)
testdir() { local g; for g in pvr-tests ta-tests; do [ -d "$g/$1" ] && { echo "$g/$1"; return 0; }; done; return 1; }
T=$1; NAME=$2; shift 2
TD=$(testdir $T) || { echo "no such test: $T"; exit 1; }
DC=${DC:-192.168.178.107}
[ -e fit/${T}_data/$NAME.npz ] && { echo "fit/${T}_data/$NAME.npz exists: pick another name"; exit 1; }
GEN="python3 $TD/gen.py"; READ="python3 $TD/read.py"     # in C++ where the test has a gen.cpp / read.cpp (model/build.sh)
[ -f $TD/gen.cpp ] || [ -f $TD/read.cpp ] && { bash model/build.sh $T > /dev/null || exit 1; }
[ -f $TD/gen.cpp ] && GEN="$TD/gen"
[ -f $TD/read.cpp ] && READ="$TD/read"
OUT=$($GEN "$@" 2>&1) || { echo "generator failed: $(echo "$OUT" | tail -n 1)"; exit 1; }
echo "$OUT" | tail -n 1 | cut -c1-150
W=/tmp/pvr_tests/$T
rm -f $W/cases.json                       # the reader then takes the test folder's, which the generator just wrote
bash run2.sh $T off hw=$DC 2>&1 | grep -v 'Clock\|modification' | grep -v 'of [0-9]* pages$'
ls $W/page0.hw*.raw > /dev/null 2>&1 || { echo "no dumps from the console"; exit 1; }
mkdir -p fit/${T}_data
$READ $W hw fit/${T}_data/$NAME.npz > /dev/null && cp $TD/cases.json fit/${T}_data/$NAME.cases.json || exit 1
$READ $W off > /dev/null && python3 compare_archive.py $T $NAME off $W
