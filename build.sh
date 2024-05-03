#!/usr/bin/env bash
rm -rf out/
cmake -B out/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=make -DCMAKE_C_COMPILER=gcc
cmake --build out --target all