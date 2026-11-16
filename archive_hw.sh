#!/bin/bash
# archive_hw.sh <test>...: keep the console's dumps of a test's last run (/tmp/pvr_tests/<test>) in
# fit/hw/<test>, with the checksums of what the console was sent. regress.sh scores refsw2-offline against
# these, so that a change to refsw2 can be checked without the console.
cd "$(dirname "$0")"
for t in "$@"; do
  W=/tmp/pvr_tests/$t
  ls $W/page0.hw*.raw > /dev/null 2>&1 && [ -s $W/hw.crc ] || { echo "$t: no console run in $W"; continue; }
  rm -rf fit/hw/$t; mkdir -p fit/hw/$t
  for f in $W/page*.hw.raw $W/page*.hw16.raw; do [ -f $f ] && gzip -c $f > fit/hw/$t/$(basename $f).gz; done
  cp $W/hw.crc fit/hw/$t/hw.crc
  echo "$t: $(ls fit/hw/$t/*.gz | wc -l) pages archived"
done
