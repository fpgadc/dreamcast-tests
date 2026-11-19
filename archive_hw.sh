#!/bin/bash
# archive_hw.sh <test>...: keep the console's dumps of a test's last run (${PVR_TESTS:-/tmp/pvr_tests}/<test>) in
# fit/hw/<test>, with the checksums of what the console was sent. regress.sh scores refsw2-offline against
# these, so that a change to refsw2 can be checked without the console. gzip -n: no name and no time in the
# archive, so that archiving the same pictures again gives the same bytes (git sees no change).
cd "$(dirname "$0")"
for t in "$@"; do
  W=${PVR_TESTS:-/tmp/pvr_tests}/$t
  ls $W/*.hw*.raw > /dev/null 2>&1 && [ -s $W/hw.crc ] || { echo "$t: no console run in $W"; continue; }
  rm -rf fit/hw/$t; mkdir -p fit/hw/$t
  for f in $W/*.hw.raw $W/*.hw16.raw; do [ -f $f ] && gzip -n -c $f > fit/hw/$t/$(basename $f).gz; done
  cp $W/hw.crc fit/hw/$t/hw.crc
  grep -a '^PVRIO hash' $W/build/hw.log 2>/dev/null | tr -d '\r' | gzip -n > fit/hw/$t/hashes.txt.gz      # hashed cases (ta-tests)
  [ -n "$(zcat fit/hw/$t/hashes.txt.gz | head -1)" ] || rm -f fit/hw/$t/hashes.txt.gz
  grep -a "^PVRIO scan" $W/build/hw.log 2>/dev/null | tr -d '\r' | gzip -n > fit/hw/$t/scans.txt.gz      # writes outside the area (ta-probe)
  [ -n "$(zcat fit/hw/$t/scans.txt.gz | head -1)" ] || rm -f fit/hw/$t/scans.txt.gz
  echo "$t: $(ls fit/hw/$t/*.gz | wc -l) pages archived"
done
