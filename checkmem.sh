#!/usr/bin/env bash
rm -rf out/
cmake -B out/ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=make -DCMAKE_C_COMPILER=gcc
cmake --build out --target all
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=out/valgrind-out.txt \
         ./out/bst -o 1000 -c -g -l -s insert -s write -s read -s read_write -r 2 -t $(nproc --all)