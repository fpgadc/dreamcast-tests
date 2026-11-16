#!/bin/bash
# measure.sh <test> <set name> [generator arguments...]: one round of measuring on a console.
#   generates the cases (<test>/gen, or gen.py), runs them on the console and in refsw2-offline, archives the
#   console's readings as fit/<test>_data/<set name>.{npz,cases.json} and says how refsw2 compares.
# For the measuring tests on pvrtest2.h (covmeas, uvmeas, uvspan, wmeas, cmeas). DC=<ip> for another console.
cd "$(dirname "$0")"
T=$1; NAME=$2; shift 2
DC=${DC:-192.168.178.107}
[ -e fit/${T}_data/$NAME.npz ] && { echo "fit/${T}_data/$NAME.npz exists: pick another name"; exit 1; }
GEN="python3 $T/gen.py"; READ="python3 $T/read.py"       # in C++ where the test has a gen.cpp / read.cpp (model/build.sh)
[ -f $T/gen.cpp ] || [ -f $T/read.cpp ] && { bash model/build.sh $T > /dev/null || exit 1; }
[ -f $T/gen.cpp ] && GEN="$T/gen"
[ -f $T/read.cpp ] && READ="$T/read"
OUT=$($GEN "$@" 2>&1) || { echo "generator failed: $(echo "$OUT" | tail -n 1)"; exit 1; }
echo "$OUT" | tail -n 1 | cut -c1-150
W=/tmp/pvr_tests/$T
rm -f $W/cases.json                       # the reader then takes the test folder's, which the generator just wrote
bash run2.sh $T off hw=$DC 2>&1 | grep -v 'Clock\|modification' | grep -v 'of [0-9]* pages$'
ls $W/page0.hw*.raw > /dev/null 2>&1 || { echo "no dumps from the console"; exit 1; }
mkdir -p fit/${T}_data
$READ $W hw fit/${T}_data/$NAME.npz > /dev/null && cp $T/cases.json fit/${T}_data/$NAME.cases.json || exit 1
$READ $W off > /dev/null && python3 compare_archive.py $T $NAME off $W
