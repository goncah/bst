#!/usr/bin/env bash
for i in 1000 10000 100000 1000000 10000000
do
   ./out/bst -n $i -c -s insert -s write -s read -s read_write -r 10 -m 0 -t 1
   for j in {2..12..2}
   do
      ./out/bst -n $i -a -g -l -s insert -s write -s read -s read_write -r 10 -m 0 -t $j

      for z in {1..5}
      do
        ./out/bst -n $i -a -s insert -s write -s read -s read_write -r 10 -m $z -t $j
      done
   done
done
