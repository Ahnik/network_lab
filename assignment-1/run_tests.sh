#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Too few arguments!"
    exit 1
fi

n=7
for i in $(seq 1 1 $n); do
    echo "----------------- TEST #$i -----------------------"
    ./bin/sender.out $1 $2 ./tests/test$i.bin > /dev/null
done
