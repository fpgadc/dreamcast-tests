#!/bin/bash
# archive_check.sh <test> [set names...] | all: archived sets of a test (fit/<test>_data/<set>.cases.json; default:
# all of them) through refsw2-offline, each compared with the console's archived result (compare_archive.py).
# In parallel, one set per CPU core; the dumps are removed again. For tests on pvrtest2.h whose generator can write the tables of a set (regen.py).
cd "$(dirname "$0")"
if [ "$1" = all ]; then       # every test on pvrtest2.h that has archives
  for t in $(ls -d fit/*_data | sed 's|fit/||; s|_data||'); do
    grep -q pvrtest2.h $t/main.c 2>/dev/null && bash "$0" $t > /tmp/pvr_tests/archive_check.$t.txt 2>&1
    [ -f /tmp/pvr_tests/archive_check.$t.txt ] && echo "$t: $(grep -c "archived console" /tmp/pvr_tests/archive_check.$t.txt) sets, $(grep -c DIFFER /tmp/pvr_tests/archive_check.$t.txt) differ from the console" && grep DIFFER /tmp/pvr_tests/archive_check.$t.txt | sed "s/^/    /"
    rm -f /tmp/pvr_tests/archive_check.$t.txt
  done
  exit 0
fi
T=$1; shift
READ="python3 $T/read.py"                 # a test with a read.cpp has its reader in C++ (model/build.sh)
[ -f $T/read.cpp ] && { bash model/build.sh $T > /dev/null || exit 1; READ="$T/read"; }
SETS="$@"
[ -z "$SETS" ] && SETS=$(ls fit/${T}_data/*.cases.json | xargs -n 1 basename | sed 's/.cases.json//')
bash run2.sh $(for s in $SETS; do echo $T@$s; done) off | grep -v 'offline: [0-9]* of [0-9]* pages' 
for s in $SETS; do echo $s; done | xargs -P $(nproc) -I{} sh -c \
  "$READ /tmp/pvr_tests/$T@{} off > /dev/null && python3 compare_archive.py $T {} off /tmp/pvr_tests/$T@{}; rm -f /tmp/pvr_tests/$T@{}/*.off.raw" 2>&1 | sort
