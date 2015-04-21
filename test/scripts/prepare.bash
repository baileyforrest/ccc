#!/bin/bash

for file in "$@"
do
    sed -i 's/main/__test/g' $file
    sed -i "2i #include \"runtime.h\"" $file
    mv $file $file.c
done
