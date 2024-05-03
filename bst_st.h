/*
Universidade Aberta
File: bst_st_t.h
Author: Hugo Gonçalves, 2100562

Single-thread BST

MIT License

Copyright (c) 2024 Hugo Gonçalves

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/
#ifndef BST_ST_H_
#define BST_ST_H_
#include <stdint.h>

#include "bst_common.h"

/**
 * Holds a tree node with pointer to both children as well as the parent node
 */
typedef struct bst_st_node {
    int64_t value;
    struct bst_st_node *left;
    struct bst_st_node *right;
} bst_st_node_t;

/**
 * The BST
 */
typedef struct bst_st {
    size_t count;
    bst_st_node_t *root;
} bst_st_t;

// Prototypes
/**
 * Allocates memory for a new BST ST returning the pointer to it.
 *
 * Check the bitmask of err for possible error combinations:
 * SUCCESS        - pointer to BST is returned.
 * MALLOC_FAILURE - malloc() failed to allocate memory for the BST.
 *
 * @param err NULL (no effect) or allocated pointer to store any errors.
 * @return bst or NULL if malloc() fails.
 */
bst_st_t *bst_st_new(BST_ERROR *err);

/**
 * Adds a new value to the BST ST.
 *
 * @param bst   the BST ST to add the value to.
 * @param value the value to add.
 * @return
 * SUCCESS        - Value added.
 *
 * BST_NULL       - when provided bst pointer is null.
 *
 * MALLOC_FAILURE - when malloc fails to allocate memory for a new tree node.
 *
 * VALUE_EXISTS   - when the value already exists.
 *
 * UNKNOWN        - should never get here.
 */
BST_ERROR bst_st_add(bst_st_t *bst, int64_t value);

/**
 * Searches the BST for the given value.
 *
 * @param bst   the BST to search the value.
 * @param value the value to search.
 * @return
 * BST_NULL          - when provided bst pointer is null.
 *
 * VALUE_EXISTS      - value exists in the BST.
 *
 * VALUE_NONEXISTENT - value does not exist in the BST.
 */
BST_ERROR bst_st_search(bst_st_t *bst, int64_t value);

/**
 * Finds and places in value the min value in the BST.
 *
 * @param bst   the BST to search the min value.
 * @param value non-null allocated pointer to store the min value.
 * @return
 * BST_NULL  - when provided bst pointer is null.
 *
 * BST_EMPTY - when provided bst is empty.
 *
 * SUCCESS   - min is placed in value if not NULL.
 */
BST_ERROR bst_st_min(bst_st_t *bst, int64_t *value);

/**
 * Finds and places in value the max value in the BST.
 *
 * @param bst   the BST to search the max value.
 * @param value pointer to store the max value, memory must be pre-allocated.
 * @return
 * BST_NULL  - when provided bst pointer is null.
 *
 * BST_EMPTY - when provided bst is empty.
 *
 * SUCCESS   - max is placed in value if not NULL.
 */
BST_ERROR bst_st_max(bst_st_t *bst, int64_t *value);

/**
 * Calculates and places in value the BST height.
 *
 * @param bst   the BST to calculate the height.
 * @param value pointer to store the BST height, memory must be pre-allocated.
 * @return
 * BST_NULL  - when provided bst pointer is null.
 *
 * BST_EMPTY - when provided bst is empty.
 *
 * SUCCESS   - height is placed in value if not NULL.
 */
BST_ERROR bst_st_height(bst_st_t *bst, size_t *value);

/**
 * Calculates and places in value the BST width.
 *
 * @param bst   the BST to calculate the width.
 * @param value pointer to store the BST width, memory must be pre-allocated.
 * @return
 * BST_NULL       - when provided bst pointer is null.
 *
 * MALLOC_FAILURE - a queue is allocated to traverse the tree, this error is
 *  returned when malloc() fails.
 *
 * BST_EMPTY      - when provided bst is empty.
 *
 * SUCCESS        - height is placed in value if not NULL.
 */
BST_ERROR bst_st_width(bst_st_t *bst, size_t *value);

/**
 * Traverse and print the BST nodes preorder.
 *
 * @param bst the BST to traverse.
 * @return
 * BST_NULL  - when provided bst pointer is null.
 *
 * BST_EMPTY - when provided bst is empty.
 *
 * SUCCESS   - elements are written to stdout.
 */
BST_ERROR bst_st_traverse_preorder(bst_st_t *bst);

/**
 * Traverse and print the BST nodes inorder.
 *
 * @param bst the BST to traverse.
 * @return
 * BST_NULL  - when provided bst pointer is null.
 *
 * BST_EMPTY - when provided bst is empty.
 *
 * SUCCESS   - elements are written to stdout.
 */
BST_ERROR bst_st_traverse_inorder(bst_st_t *bst);

/**
 * Traverse and print the BST nodes postorder.
 *
 * @param bst the BST to traverse.
 * @return
 * BST_NULL  - when provided bst pointer is null.
 *
 * BST_EMPTY - when provided bst is empty.
 *
 * SUCCESS   - elements are written to stdout.
 */
BST_ERROR bst_st_traverse_postorder(bst_st_t *bst);

/**
 * Attempt to find and delete value from bst.
 *
 * @param bst   the BST to find and delete the value from.
 * @param value the value to delete.
 * @return
 * BST_NULL          - when provided bst pointer is null.
 *
 * BST_EMPTY         - when provided bst is empty.
 *
 * VALUE_NONEXISTENT - value not found.
 *
 * SUCCESS           - value found and deleted.
 */
BST_ERROR bst_st_delete(bst_st_t *bst, int64_t value);

/**
 * Height rebalance BST ST.
 *
 * @param bst the bst to rebalance.
 * @return
 * BST_NULL       - when provided bst pointer is null.
 *
 * BST_EMPTY      - when provided bst is empty.
 *
 * MALLOC_FAILURE - when malloc() fails. can happen for when allocating the
 *  array to store values during rebalance or when creating a new node. BST and
 *  all nodes are freed.
 *
 * SUCCESS        - BST rebalanced.
 */
BST_ERROR bst_st_rebalance(bst_st_t *bst);

/**
 * Frees a BST.
 *
 * @param bst the bst to free.
 * @return
 * BST_NULL - when provided bst pointer is null.
 *
 * SUCCESS  - bst and all nodes freed.
 */
BST_ERROR bst_st_free(bst_st_t *bst);
#endif // BST_ST_H_
