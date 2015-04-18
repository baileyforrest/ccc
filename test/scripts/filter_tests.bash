#!/bin/bash

RUNNER=./test/scripts/test_runner.py
CC="clang -std=c11"
FAIL_DIR=./test/filtered

mkdir -p $FAIL_DIR

for file in "$@"
do
    $RUNNER $CC $file
    if [ $? -ne 0 ]
    then
        mv $file $FAIL_DIR
    fi
done
