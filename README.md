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

$ gcc -Wall -Wextra -shared -fPIC -o out/libbst_st.so bst_st_t.c -lpthread

$ gcc -Wall -Wextra -shared -fPIC -o out/libbst_mt_gmtx.so bst_mt_gmtx_t.c -lpthread

$ gcc -Wall -Wextra -shared -fPIC -o out/libbst_mt_lmtx.so bst_mt_lmtx_t.c -lpthread

$ gcc -Wall -Wextra -shared -fPIC -o out/libbst_mt_cas.so bst_mt_cas.c -lpthread

### Compile the test executable

gcc -Wall -Wextra -Lout/ -o out/test_bst main.c -pthread -lbst_st -lbst_mt_gmtx -lbst_mt_lmtx -lbst_mt_cas

## Test executable usage

### Add out directory to LD load path

$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:out

### BST Single Thread

$ test_bst st <#inserts>

### BST Multi Thread Global Mutex

$ test_bst gmtx <#threads> <#inserts>

### BST Multi Thread Local Mutex

$ test_bst lmtx <#threads> <#inserts>

### BST Multi Thread CAS

$ test_bst cas <#threads> <#inserts>