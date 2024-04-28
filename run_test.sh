#!/bin/bash
USAGE="$ run_test.sh <bst_type> <threads> <inserts> <test_count>"

if [[ -z $1 || -z $2 || -z $3 || -z $4 ]]; then
    echo "Usage:"
    echo $USAGE
else
    build.sh
    if [ "$1" == "st" ]; then
        ./out/test_bst $1 $3 $4
    else
        ./out/test_bst $1 $2 $3 $4
    fi
fi