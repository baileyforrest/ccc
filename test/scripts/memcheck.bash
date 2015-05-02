#!/bin/bash

CC=./bin/ccc
CFLAGS="-Itest/runtime --dump_ir"

for file in "$@"
do
    valgrind $CC $CFLAGS $file > dump.c
done
