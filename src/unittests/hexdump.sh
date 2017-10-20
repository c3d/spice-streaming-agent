#!/bin/sh

set -e

# avoid weird language handling which could affect
# ascii part of the dump
export LANG=C

for f in hexdump*.in; do
    out=`echo $f | sed 's,\.in,.out,'`
    rm -f $out.test
    ./test-hexdump $out.test < $f
    cmp $out.test $out
    rm -f $out.test
done
