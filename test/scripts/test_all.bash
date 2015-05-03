#!/bin/bash

TEST_15411=./test/15411
RUNTIME_15411=./test/runtime/15411
SCRIPT_DIR=./test/scripts
JOBS=16

CC="./bin/ccc -S -emit-llvm"

if [ -d "$TEST_15411" ]; then
    $SCRIPT_DIR/test_runner.py -r $RUNTIME_15411 --llvm -j$JOBS "$CC" $TEST_15411/*/*.c
fi

$SCRIPT_DIR/test_parse.bash
