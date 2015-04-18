#!/bin/bash

for file in "$@"
do
    sed -i 's/main/__test/g' $file
    mv $file $file.c
done
