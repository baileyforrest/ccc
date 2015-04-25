#!/bin/bash

SRC=src
BIN_NAME=./bin/ccc
OUTPUT_C=dump.c
OUTPUT_O=dump.o

FILES=$(find $SRC -name "*.c")

for i in $FILES
do
    $BIN_NAME -I$SRC --dump_ast $i > $OUTPUT_C
    if [ $? -ne 0 ]
    then
        echo "$i: Parsing failed!"
        continue
    fi
    gcc -c -std=c11 $OUTPUT_C -o $OUTPUT_O
    if [ $? -ne 0 ]
    then
        echo "$i: Compile failed!"
    fi
done

valgrind --leak-check=full ./bin/ccc -I$SRC --dump_ast $FILES > $OUTPUT_C
