# Concurrent Binary Search Tree

This is a final university degree project exploring different concurrency control mechanisms and their performance under
multiple scenarios.

## Building with cmake

### Load CMake Project and setup cmake cache

$ cmake -B out -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=<yourmakegen> -DCMAKE_C_COMPILER=<yourccompiler>

#### Example:

$ cmake -B out -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=make -DCMAKE_C_COMPILER=gcc

### Build libraries and test executable

$ cmake --build out --target all

## Test executable usage

### Add out directory to LD load path

$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:out

### Execute tests
$ bst < options >

Options:

-n Set the number of operations

-o Set write probability over read operations. Ex: 60 for 60% inserts
   This option is ignored for insert, write and read strategies.

-t Set the number of threads, operations are evenly distributed. Ignored for BST ST type.

-r Set the number of test repetitions, applies to each strat and each BST type

-s < strategy > Set the test strategy. Multiple strategies can be set, example -s 1 -s 2 -s 3. Available strategies are:
   insert     - Inserts only with random generated numbers.
   write      - Random inserts, deletes with random generated numbers.
   read       - Random search, min, max, height and width. -o sets the number of elements in the read.
   read_write - Random inserts, deletes, search, min, max, height and width with random generated numbers.

-a Set the BST type to Atomic, can be set with -c, -g and -l to test multiple BST types

-c Set the BST type to ST, can be set with -a, -g and -l to test multiple BST types

-g Set the BST type to MT Global RwLock, can be set with -a, -c and -l to test multiple BST types

-l Set the BST type to MT Local RwLock, can be set with -a, -c and -g to test multiple BST types

-m <#> Sets the memory order for atomic operations, default is 5 "memory_order_seq_cst", possible values are: 
   0 - memory_order_relaxed, 
   1 - memory_order_consume, 
   2 - memory_order_acquire, 
   3 - memory_order_release,
   4 - memory_order_acq_rel,
   5 - memory_order_seq_cst

### Output
#### Output is csv format with the following columns:
<bst_type>,<strategy>,<#operations>,<#threads>,<#tree_node_count>,<tree_min>,<tree_max>,<tree_height>,<tree_width>,<time_taken>,<#inserts>,<#searches>,<#mins>,<#maxs>,<#heights>,<#widths>,<#deletes>,<#rebalances>

### Examples
#### Run 100000 operations for all BST types, only insert strategy and do not repeat
$ bst -o 100000 -g -l -c -s insert -r 1 -t 20

#### Run 100000 operations for all BST types, insert and write strategy and do not repeat
$ bst -o 100000 -g -l -c -s insert -s write -r 1 -t 20

#### Run 10000 operations only for BST ST, only read strategy and do not repeat
$ bst -o 10000 -c -s read -r 1

## Compiled and tested with
Ubuntu 24.04 LTS

gcc: Ubuntu 13.2.0-23ubuntu4

glibc: Ubuntu GLIBC 2.39-0ubuntu8.1

kernel: 6.8.0-31-generic

