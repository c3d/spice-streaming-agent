#!/bin/sh

set -e # exit on errors

autoreconf --verbose --force --install

if [ -z "$NOCONFIGURE" ]; then
    ./configure ${1+"$@"}
fi
