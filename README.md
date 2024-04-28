# bst
MT Binary Search Tree Implementation exploring concurrency control techniques

## Build
gcc -Ofast -std=gnu2x -pthread test_bst.c -o out/test_bst

# Usage
## BST Single Thread
$ test_bst st <inserts>

## BST Multi Thread Global Mutex
$ test_bst gmtx <threads> <inserts>

## BST Multi Thread Local Mutex
$ test_bst lmtx <threads> <inserts>

## BST Multi Thread CAS
$ test_bst cas <threads> <inserts>