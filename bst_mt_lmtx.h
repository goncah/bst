/*
Universidade Aberta
File: bst_mt_lmtx_t.h
Author: Hugo Gonçalves, 2100562

Multi-thread BST using a local mutex

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
#ifndef BST_MT_LMTX_H_
#define BST_MT_LMTX_H_
#include <pthread.h>
#include <stdint.h>

#include "bst_common.h"

/**
 * Holds a tree node with pointer to both children as well as the parent node
 */
typedef struct bst_mt_lmtx_node {
    int64_t value;
    struct bst_mt_lmtx_node *parent;
    struct bst_mt_lmtx_node *left;
    struct bst_mt_lmtx_node *right;
    pthread_mutex_t mtx;
} bst_mt_lmtx_node_t;

/**
 * The BST
 */
typedef struct bst_mt_lmtx {
    bst_mt_lmtx_node_t *root;
    pthread_mutex_t mtx; // Still need a global mutex to control concurrency
                         // when adding the root node
    pthread_rwlock_t
        rwl; // Inverted usage of the RwLock, this allows to have fine-grained
             // locking on insert with local mutex and a read lock on the RwLock
             // while all other operations that require no changes on the BST
             // during execution will request a write lock. RwLock is configured
             // to prefer write, otherwise, under heavy contention, only inserts
             // are executed.
} bst_mt_lmtx_t;

// Prototypes
/**
 * Allocates memory for a new BST MT Local Mutex returning the pointer to it.
 * Any errors will trigger the attempt of destruction of mutex, rwlock and free
 * the allocated BST memory.
 *
 * Check the bitmask of err for possible error combinations:
 * SUCCESS - pointer to BST is returned
 * MALLOC_FAILURE - malloc() failed to allocate memory for the BST
 * PT_MUTEX_INIT_FAILURE - failed to init BST Global Mutex, BST is freed
 * PT_MUTEX_DESTROY_FAILURE | PT_RWLOCK_ATTR_INIT_FAILURE:
 *      Failed to init the RwLock Attributes, also failed to destroy the global
 *      Mutex. BST is freed.
 *
 * PT_RWLOCK_ATTR_INIT_FAILURE - Failed to init the RwLock Attributes.
 *      BST is freed.
 *
 * PT_MUTEX_DESTROY_FAILURE | PT_RWLOCK_INIT_FAILURE:
 *      Failed to init the RwLock, also failed to destroy the global
 *      Mutex. BST is freed.
 *
 * PT_RWLOCK_INIT_FAILURE - Failed to init the RwLock, BST is freed.
 * @param err NULL (no effect) or allocated pointer to store any errors
 * @return NULL or BST
 */
bst_mt_lmtx_t *bst_mt_lmtx_new(BST_ERROR *err);

/**
 * Adds a new value to the BST - Thread safe.
 *
 * @param bst the BST ST to add the value to
 * @param value the value to add
 * @return  SUCCESS                       - Value added.
 *          BST_NULL                      - when provided bst pointer is null.
 *          PT_RWLOCK_READ_LOCK_FAILURE   - when failed to lock global RwLock.
 *                                          No changes to bst are performed.
 *          PT_MUTEX_LOCK_FAILURE         - when failed to lock a local mutex,
 *                                          no changes to the BST.
 *          PT_RWLOCK_READ_LOCK_FAILURE   - when failed to lock a global RwLock,
 *                                           no changes to the BST.
 *          PT_MUTEX_UNLOCK_FAILURE       - when failed to unlock a local
 *                                          mutex. Always paired with either
 *                                          VALUE_ADDED or VALUE_NOT_ADDED.
 *                                          Additionally paired with
 *                                          MALLOC_FAILURE, in this case, no
 *                                          changes to the BST.
 *          PT_RWLOCK_READ_UNLOCK_FAILURE - when failed to unlock a global
 *                                          RwLock. Always paired with either
 *                                          VALUE_ADDED or VALUE_NOT_ADDED.
 *                                          Additionally paired with
 *                                          MALLOC_FAILURE, in this case, no
 *                                          changes to the BST.
 *          MALLOC_FAILURE                - when malloc fails to allocate memory
 *                                          for a new tree node.
 *          VALUE_EXISTS                  - when the value already exists.
 *          UNKNOWN                       - should never get here.
 */
BST_ERROR bst_mt_lmtx_add(bst_mt_lmtx_t *bst, int64_t value);

/**
 * Searches the BST for the given value- Thread safe,
 * no write operations are permitted during execution.
 *
 * @param bst the BST to search the value
 * @param value the value to search
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           RwLock.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, can be paired with
 *                                           BST_EMPTY VALUE_EXISTS or
 *                                           VALUE_NONEXISTENT.
 *          VALUE_EXISTS                   - value exists in the BST
 *          VALUE_NONEXISTENT              - value does not exist in the BST
 */
BST_ERROR bst_mt_lmtx_search(bst_mt_lmtx_t *bst, int64_t value);

/**
 * Finds and places in value the min value in the BST - Thread safe,
 * no write operations are permitted during execution.
 *
 * @param bst   the BST to search the min value
 * @param value pointer to store the min value, memory must be pre-allocated
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           RwLock, value is not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, can be paired with
 *                                           BST_EMPTY or SUCCESS. If paired
 *                                           with success, min is stored in
 *                                           value, if value is not NULL.
 *          SUCCESS                        - min is stored in value, if value is
 *                                           not NULL
 */
BST_ERROR bst_mt_lmtx_min(bst_mt_lmtx_t *bst, int64_t *value);

/**
 * Finds and places in value the min value in the BST - Thread safe,
 * no write operations are permitted during execution.
 *
 * @param bst   the BST to search the min value
 * @param value pointer to store the min value, memory must be pre-allocated
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           RwLock, value is not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, can be paired with BST_EMPTY
 *                                           or SUCCESS. If paired with success,
 *                                           max is stored in value, if value is
 *                                           not NULL
 *          SUCCESS                        - max is stored in value, if value is
 *                                           not NULL
 */
BST_ERROR bst_mt_lmtx_max(bst_mt_lmtx_t *bst, int64_t *value);

/**
 * Finds and places in value the total number of tree nodes in the BST - Thread
 * safe, no write operations are permitted during execution.
 *
 * @param bst   the BST to search the total number of tree nodes.
 * @param value pointer to store the total number of tree nodes, memory must be
 * pre-allocated.
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          MALLOC_FAILURE                 - when the stack used to traverse the
 *                                           tree fails to allocate, value is
 *                                           not written.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           RwLock, value is not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, can be paired with SUCCESS,
 *                                           MALLOC_FAILURE or BST_EMPTY.
 *                                           If paired with SUCCESS, node count
 *                                           is stored in value, if value is not
 *                                           NULL.
 *          SUCCESS                        - node count is stored in value, if
 *                                           value is not NULL.
 */
BST_ERROR bst_mt_lmtx_node_count(bst_mt_lmtx_t *bst, size_t *value);

/**
 * Calculates and places in value the BST height - Thread safe,
 * no write operations are permitted during search.
 *
 * @param bst   the BST to calculate the height.
 * @param value pointer to store the BST height, memory must be pre-allocated.
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           mutex, value is not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, can be paired with
 *                                           BST_EMPTY, MALLOC_FAILURE or
 *                                           SUCCESS. If paired with SUCCESS,
 *                                           value is still written. If paired
 *                                           with MALLOC_FAILURE or BST_EMPTY
 *                                           value is not written. When paired
 *                                           with MALLOC_FAILURE, it means that
 *                                           a queue used to traverse the tree
 *                                           failed to be allocated.
 *                                           If paired with success, height is
 *                                           stored in value, if value not NULL.
 *          SUCCESS                        - height is stored in value, if value
 *                                           not NULL.
 */
BST_ERROR bst_mt_lmtx_height(bst_mt_lmtx_t *bst, size_t *value);

/**
 * Calculates and places in value the BST width - Thread safe,
 * no write operations are permitted during search.
 *
 * @param bst   the BST to calculate the width.
 * @param value pointer to store the BST width, memory must be pre-allocated.
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           mutex, value is not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, can be paired with
 *                                           BST_EMPTY, MALLOC_FAILURE or
 *                                           SUCCESS. If paired with SUCCESS,
 *                                           value is still written. If paired
 *                                           with MALLOC_FAILURE or BST_EMPTY
 *                                           value is not written. When paired
 *                                           with MALLOC_FAILURE, it means that
 *                                           a queue used to traverse the tree
 *                                           failed to be allocated.
 *                                           If paired with success, width is
 *                                           stored in value, if value not NULL.
 *          SUCCESS                        - width is stored in value, if value
 *                                           not NULL.
 */
BST_ERROR bst_mt_lmtx_width(bst_mt_lmtx_t *bst, size_t *value);

/**
 * Traverse and print the BST nodes preorder - Thread safe,
 * no write operations are permitted during traverse.
 *
 * @param bst the BST to traverse
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           Rwlock, elements are not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           Rwlock, can be paired with
 *                                           BST_EMPTY, MALLOC_FAILURE or
 *                                           SUCCESS.
 *                                           If paired with MALLOC_FAILURE or
 *                                           BST_EMPTY elements are not written.
 *                                           When paired with MALLOC_FAILURE, it
 *                                           means that the queue used to
 *                                           traverse the tree failed to be
 *                                           allocated.
 *                                           If paired with success, elements
 *                                           are written to stdout in preorder.
 *          SUCCESS                        - elements written to stdout in
 *                                           preorder.
 */
BST_ERROR bst_mt_lmtx_traverse_preorder(bst_mt_lmtx_t *bst);

/**
 * Traverse and print the BST nodes inorder - Thread safe,
 * no write operations are permitted during traverse.
 *
 * @param bst the BST to traverse
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           Rwlock, elements are not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           Rwlock, can be paired with
 *                                           BST_EMPTY, MALLOC_FAILURE or
 *                                           SUCCESS.
 *                                           If paired with MALLOC_FAILURE or
 *                                           BST_EMPTY elements are not written.
 *                                           When paired with MALLOC_FAILURE, it
 *                                           means that the queue used to
 *                                           traverse the tree failed to be
 *                                           allocated.
 *                                           If paired with success, elements
 *                                           are written to stdout in preorder.
 *          SUCCESS                        - elements written to stdout in
 *                                           inorder.
 */
BST_ERROR bst_mt_lmtx_traverse_inorder(bst_mt_lmtx_t *bst);

/**
 * Traverse and print the BST nodes postorder - Thread safe,
 * no write operations are permitted during traverse.
 *
 * @param bst the BST to traverse
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          BST_EMPTY                      - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE   - when failed to lock the global
 *                                           Rwlock, elements are not written.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           Rwlock, can be paired with
 *                                           BST_EMPTY, MALLOC_FAILURE or
 *                                           SUCCESS.
 *                                           If paired with MALLOC_FAILURE or
 *                                           BST_EMPTY elements are not written.
 *                                           When paired with MALLOC_FAILURE, it
 *                                           means that the queue used to
 *                                           traverse the tree failed to be
 *                                           allocated.
 *                                           If paired with success, elements
 *                                           are written to stdout in preorder.
 *          SUCCESS                        - elements written to stdout in
 *                                           postorder.
 */
BST_ERROR bst_mt_lmtx_traverse_postorder(bst_mt_lmtx_t *bst);

/**
 * Attempt to find and delete value from bst - Thread safe,
 * no write operations are permitted during deletion.
 *
 * @param bst the BST to find and delete the value from.
 * @param value the value to delete.
 * @return  BST_NULL                     - when provided bst pointer is null.
 *          BST_EMPTY                    - when provided bst is empty.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE - when failed to lock the global
 *                                         RwLock, value is not deleted.
 *          PT_RWLOCK_WRITE_LOCK_FAILURE - when failed to unlock the global
 *                                         RwLock, can be paired with BST_EMPTY,
 *                                         VALUE_NONEXISTENT or SUCCESS. If
 *                                         paired with SUCCESS, value is still
 *                                         removed.
 *          VALUE_NONEXISTENT            - value not found.
 *          SUCCESS                      - value removed.
 */
BST_ERROR bst_mt_lmtx_delete(bst_mt_lmtx_t *bst, int64_t value);

/**
 * Height rebalance BST ST.
 *
 * @param bst the bst to rebalance.
 * @return  BST_NULL                 - when provided bst pointer is null.
 *          BST_EMPTY                - when provided bst is empty.
 *          MALLOC_FAILURE           - when malloc() fails. can happen for when
 *                                     allocating the array to store values
 *                                     during rebalance or when creating a new
 *                                     node. BST and all nodes are freed.
 *          PT_MUTEX_LOCK_FAILURE    - when failed to lock the global mutex,
 *                                     value is not written. No changes to the
 *                                     BST.
 *          PT_MUTEX_UNLOCK_FAILURE  - when failed to unlock the global mutex,
 *                                     can be paired with BST_EMPTY,
 *                                     MALLOC_FAILURE or SUCCESS.
 *          PT_MUTEX_DESTROY_FAILURE - when failed to destroy the bst global
 *                                     mutex. Can be paired with MALLOC_FAILURE
 *                                     or PT_MUTEX_UNLOCK_FAILURE. In both cases
 *                                     BST and all nodes are freed.
 *          SUCCESS                  - BST rebalanced.
 */
BST_ERROR bst_mt_lmtx_rebalance(bst_mt_lmtx_t *bst);

/**
 * Frees a BST.
 *
 * @param bst the bst to free.
 * @return  BST_NULL                       - when provided bst pointer is null.
 *          PT_MUTEX_LOCK_FAILURE          - when failed to lock the global
 *                                           mutex, no changes to the BST.
 *          PT_MUTEX_LOCK_FAILURE          - when failed to lock the global
 *                                           RwLock, no changes to the BST.
 *          PT_MUTEX_UNLOCK_FAILURE        - when failed to unlock the global
 *                                           mutex, all nodes are freed but the
 *                                           BST is not.
 *          PT_MUTEX_DESTROY_FAILURE       - when failed to destroy the global
 *                                           mutex, all nodes are freed but the BST is not.
 *          PT_RWLOCK_WRITE_UNLOCK_FAILURE - when failed to unlock the global
 *                                           RwLock, all nodes are freed but the
 *                                           BST is not.
*           PT_RWLOCK_DESTROY_FAILURE      - when failed to destroy the global
 *                                           RwLock, all nodes are freed but the
 *                                           BST is not.
 *          SUCCESS                        - bst and all nodes freed.
 */
BST_ERROR bst_mt_lmtx_free(bst_mt_lmtx_t *bst);
#endif // BST_MT_LMTX_H_
