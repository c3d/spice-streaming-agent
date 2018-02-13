#!/bin/sh

set -e

SRCDIR=`dirname $0`

# avoid weird language handling which could affect
# ascii part of the dump
export LANG=C

for f in "$SRCDIR"/hexdump*.in; do
    reference=`echo $f | sed 's,\.in,.out,'`
    out=`basename $reference`.test
    rm -f $out
    ./hexdump $out < $f
    cmp $out $reference
    rm -f $out
done
