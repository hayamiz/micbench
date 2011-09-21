#!/bin/sh

export BASE_DIR="`dirname $0`"
export TEST_DIR="$(readlink -f $(dirname $0))"
top_dir="$BASE_DIR/.."

if test -z "$NO_MAKE"; then
    make -C $top_dir > /dev/null || exit 1
fi

if test -z "$CUTTER"; then
    CUTTER="`make -s -C $BASE_DIR echo-cutter`"
fi

if ! test -z "$DEBUG"; then
    WRAPPER="gdb --args"
elif ! test -z "$MEMCHECK"; then
    WRAPPER="valgrind --leak-check=summary"
fi

$WRAPPER $CUTTER -s $BASE_DIR "$@" $BASE_DIR
