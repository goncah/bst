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

## Building with gcc only

### Compile bst libraries

$ gcc -Wall -Wextra -Ofast -shared -fPIC -o out/libbst_st.so bst_st_t.c -lpthread

$ gcc -Wall -Wextra -Ofast -shared -fPIC -o out/libbst_mt_grwl.so bst_mt_grwl_t.c -lpthread

$ gcc -Wall -Wextra -Ofast -shared -fPIC -o out/libbst_mt_lmtx.so bst_mt_lrwl_t.c -lpthread

$ gcc -Wall -Wextra -Ofast -shared -fPIC -o out/libbst_mt_cas.so bst_mt_cas.c -lpthread

### Compile the test executable

gcc -Wall -Wextra -Ofast -Lout/ -o out/test_bst main.c -pthread -lbst_st -lbst_mt_grwl -lbst_mt_lmtx -lbst_mt_cas

## Test executable usage

### Add out directory to LD load path

$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:out

### Execute tests
$ bst < options >

Options:

-o Set the number of operations

-t Set the number of threads, operations are evenly distributed. Ignored for BST ST type.

-r Set the number of test repetitions, applies to each strat and each BST type

-s < strategy > Set the test strategy. Multiple strategies can be set, example -s 1 -s 2 -s 3. Available strategies are:
   insert     - Inserts only with random generated numbers.
   write      - Random inserts, deletes with random generated numbers.
   read       - Random search, min, max, height and width. -o sets the number of elements in the read.
   read_write - Random inserts, deletes, search, min, max, height and width with random generated numbers.

-c Set the BST type to ST, can be set with -g and -l to test multiple BST types

-g Set the BST type to MT Global RwLock, can be set with -c and -l to test multiple BST types

-l Set the BST type to MT Local RwLock, can be set with -c and -g to test multiple BST types

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
WSL Ubuntu 22.04.4 LTS

gcc: 11.4.0

ld: 2.38

glibc: (Ubuntu GLIBC 2.35-0ubuntu3.7) 2.35

kernel: 5.15.146.1-microsoft-standard-WSL2

