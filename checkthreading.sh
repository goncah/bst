#!/usr/bin/env bash
rm -rf out/
cmake -B out/ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=make -DCMAKE_C_COMPILER=gcc
cmake --build out --target all
valgrind --tool=helgrind \
         --verbose \
         --log-file=out/helgrind-out.txt \
         ./out/bst -o 1000 -g -l -s insert -s write -s read -s read_write -r 2 -t $(nproc --all)
