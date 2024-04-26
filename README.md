# bst
MT Binary Search Tree Implementation exploring concurrency control techniques

## Build
gcc -std=gnu2x -pthread test_bst.c -o test_bst

# Usage
## BST Single Thread
$ test_bst st <inserts> <rebalance_every>

## BST Multi Thread Global Mutex
$ test_bst gmtx <inserts> <rebalance_every>