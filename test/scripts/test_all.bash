#!/bin/bash

TEST_15411=test/15411
RUNTIME_15411=test/runtime/15411

TESTS=test/tests
RUNTIME=test/runtime

SCRIPT_DIR=test/scripts
JOBS=16

CC="./bin/ccc"

# Run 15411 Tests if present
if [ -d "$TEST_15411" ]; then
    $SCRIPT_DIR/test_runner.py -r $RUNTIME_15411 --llvm -j$JOBS "$CC -S -emit-llvm" $TEST_15411/*/*.c
fi

# Run tests
$SCRIPT_DIR/test_runner.py -r $RUNTIME -j$JOBS "$CC" $TESTS/*/*.c

$SCRIPT_DIR/test_parse.bash
