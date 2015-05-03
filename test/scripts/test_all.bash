#!/bin/bash

TEST_15411=./test/15411
SCRIPT_DIR=./test/scripts
CC="./bin/ccc -S -emit-llvm"
JOBS=16

if [ -d "$TEST_15411" ]; then
    $SCRIPT_DIR/test_runner.py --llvm "-j$JOBS" "$CC" $TEST_15411/*/*.c
fi

$SCRIPT_DIR/test_parse.bash
