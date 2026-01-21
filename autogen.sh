#!/bin/sh

# Exit on error
set -eu

# Manage out-of-tree call to autogen.sh
srcdir="$(dirname "$0")"
if [ "x$srcdir" != x ]
then
    cd "$srcdir"
fi

autoreconf -fvi
