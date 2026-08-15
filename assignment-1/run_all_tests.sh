#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Too few arguments!"
    exit 1
elif [ ! -f $2 ]; then
    echo "File does not exist!"
    exit 1
elif [ ! -f "$(pwd)/run_tests.sh" ]; then
    echo "$(pwd)/run_tests.sh doesn't exist!"
    exit 1
fi

# Clear out the file
cat /dev/null > $2

# Run all the tests and store the results in the file
codes=("CHECKSUM" "CRC8" "CRC10" "CRC16" "CRC32")

for code in "${codes[@]}"; do
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~$code~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" >> $2
    $(pwd)/run_tests.sh $code $1 &>> $2
done
