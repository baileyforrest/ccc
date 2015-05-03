#!/bin/bash

CC=./bin/ccc
CFLAGS="--dump_ir"
runtime=$1
shift

for file in "$@"
do
    valgrind $CC -I $runtime $CFLAGS $file > dump.c
done
