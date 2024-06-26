/*
Universidade Aberta
File: bst_at.h
Author: Hugo Gonçalves, 2100562

MT Safe BST using atomic operations and hazard pointers.

MIT License

Copyright (c) 2024 Hugo Gonçalves

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

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
#ifndef BST_AT_H_
#define BST_AT_H_
#include <stdatomic.h>
#include <stdint.h>

#include "../../include/bst_common.h"

/**
 * Holds a tree node with pointers to both children nodes
 */
typedef struct bst_at_node {
    int64_t value;
    _Atomic(struct bst_at_node *) left;
    _Atomic(struct bst_at_node *) right;
} bst_at_node_t;

typedef struct hazard_pointer {
    _Atomic(bst_at_node_t *) pointer;
    struct hazard_pointer *next;
} hazard_pointer_t;

/**
 * The BST
 */
typedef struct bst_at {
    atomic_size_t count;
    _Atomic(bst_at_node_t *) root;
    _Atomic(hazard_pointer_t *) hazard_pointers;
} bst_at_t;

// Prototypes
/**
 * Allocates memory for a new BST MT Local Mutex returning the pointer to it.
 * Any errors will trigger the attempt of destruction of mutex, rwlock and free
 * the allocated BST memory.
 *
 * Check the bitmask of err for possible error combinations:
 * SUCCESS - pointer to BST is returned
 *
 * MALLOC_FAILURE - malloc() failed to allocate memory for the BST
 *
 * @param err NULL (no effect) or allocated pointer to store any errors
 * @return NULL or BST
 */
bst_at_t *bst_at_new(BST_ERROR *err);

/**
 * Adds a new value to the BST - Thread safe.
 *
 * @param bst the BST ST to add the value to
 * @param value the value to add
 * @return
 * SUCCESS                       - Value added.
 *
 * BST_NULL                      - when provided bst pointer is null.
 *
 * MALLOC_FAILURE                - when malloc fails to allocate memory for a
 *  new tree node.
 *
 * VALUE_EXISTS                  - when the value already exists.
 *
 * UNKNOWN                       - should never get here.
 */
BST_ERROR bst_at_add(bst_at_t **bst, int64_t value);

/**
 * Searches the BST for the given value- Thread safe,
 * no write operations are permitted during execution.
 *
 * @param bst the BST to search the value
 * @param value the value to search
 * @return
 * BST_NULL                 - when provided bst pointer is null.
 *
 * BST_EMPTY                - when provided bst is empty.
 *
 * VALUE_EXISTS             - value exists in the BST.
 *
 * VALUE_NONEXISTENT        - value does not exist in the BST.
 */
BST_ERROR bst_at_search(bst_at_t **bst, int64_t value);

/**
 * Finds and places in value the min value in the BST - Thread safe,
 * no write operations are permitted during execution.
 *
 * @param bst   the BST to search the min value
 * @param value pointer to store the min value, memory must be pre-allocated
 * @return
 * BST_NULL                 - when provided bst pointer is null.
 *
 * BST_EMPTY                - when provided bst is empty.
 *
 * SUCCESS                  - min is stored in value, if value is not NULL
 */
BST_ERROR bst_at_min(bst_at_t **bst, int64_t *value);

/**
 * Finds and places in value the min value in the BST - Thread safe,
 * no write operations are permitted during execution.
 *
 * @param bst   the BST to search the min value
 * @param value pointer to store the min value, memory must be pre-allocated
 * @return
 * BST_NULL                 - when provided bst pointer is null.
 *
 * BST_EMPTY                - when provided bst is empty.
 *
 * SUCCESS                  - max is stored in value, if value is not NULL
 */
BST_ERROR bst_at_max(bst_at_t **bst, int64_t *value);

/**
 * Finds and places in value the total number of tree nodes in the BST - Thread
 * safe, no write operations are permitted during execution.
 *
 * @param bst   the BST to search the total number of tree nodes.
 * @param value pointer to store the total number of tree nodes, memory must be
 * pre-allocated.
 * @return
 * BST_NULL                 - when provided bst pointer is null.
 *
 * BST_EMPTY                - when provided bst is empty.
 *
 * MALLOC_FAILURE           - when the stack used to traverse the tree fails to
 *  allocate, value is not written.
 *
 * SUCCESS                  - node count is stored in value, if value is not
 *  NULL.
 */
BST_ERROR bst_at_node_count(bst_at_t **bst, size_t *value);

/**
 * Attempt to find and delete value from bst - Thread safe,
 * no write operations are permitted during deletion.
 *
 * @param bst the BST to find and delete the value from.
 * @param value the value to delete.
 * @return
 * BST_NULL               - when provided bst pointer is null.
 *
 * BST_EMPTY              - when provided bst is empty.
 *
 * VALUE_NONEXISTENT       - value not found.
 *
 * SUCCESS                 - value removed.
 */
BST_ERROR bst_at_delete(bst_at_t **bst, int64_t value);

/**
 * Frees a BST.
 *
 * @param bst the bst to free.
 * @return
 * BST_NULL                  - when provided bst pointer is null.
 *
 * SUCCESS                   - bst and all nodes freed.
 */
BST_ERROR bst_at_free(bst_at_t **bst);
#endif // BST_AT_H_