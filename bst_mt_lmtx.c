/*
Universidade Aberta
File: bst_mt_lmtx_t.c
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
#define _XOPEN_SOURCE 700 // for PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bst_common.h"
#include "bst_mt_lmtx.h"

bst_mt_lmtx_node_t *bst_mt_lmtx_node_new(int64_t value,
                                         bst_mt_lmtx_node_t *parent,
                                         BST_ERROR *err) {
    bst_mt_lmtx_node_t *node = (bst_mt_lmtx_node_t *)malloc(sizeof *node);

    if (node == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    if (pthread_mutex_init(&node->mtx, NULL)) {
        free(node);
        if (err != NULL) {
            *err = PT_MUTEX_INIT_FAILURE;
        }
        return NULL;
    }

    node->value = value;
    node->parent = parent;
    node->left = NULL;
    node->right = NULL;

    if (err != NULL) {
        *err = SUCCESS;
    }

    return node;
}

bst_mt_lmtx_t *bst_mt_lmtx_new(BST_ERROR *err) {
    bst_mt_lmtx_t *bst = (bst_mt_lmtx_t *)malloc(sizeof *bst);

    if (bst == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    if (pthread_mutex_init(&bst->mtx, NULL)) {
        free(bst);
        if (err != NULL) {
            *err = PT_MUTEX_INIT_FAILURE;
        }
        return NULL;
    }

    // Inverted usage of the RwLock, this allows to have fine-grained
    // locking on insert with local mutex and a read lock on the RwLock
    // while all other operations that require no changes on the BST
    // during execution will request a write lock. RwLock is configured
    // to prefer write, otherwise, under heavy contention, only inserts
    // are executed.
    pthread_rwlockattr_t rtx_attr;

    if (pthread_rwlockattr_setkind_np(
            &rtx_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)) {
        if (pthread_mutex_destroy(&bst->mtx)) {
            free(bst);
            if (err != NULL) {
                *err = PT_MUTEX_DESTROY_FAILURE | PT_RWLOCK_ATTR_INIT_FAILURE;
            }
            return NULL;
        }

        free(bst);
        if (err != NULL) {
            *err = PT_RWLOCK_ATTR_INIT_FAILURE;
        }
        return NULL;
    }

    if (pthread_rwlock_init(&bst->rwl, &rtx_attr)) {
        if (pthread_mutex_destroy(&bst->mtx)) {
            free(bst);

            if (err != NULL) {
                *err = PT_MUTEX_DESTROY_FAILURE | PT_RWLOCK_INIT_FAILURE;
            }
            return NULL;
        }

        free(bst);

        if (err != NULL) {
            *err = PT_RWLOCK_INIT_FAILURE;
        }
        return NULL;
    }

    bst->root = NULL;

    return bst;
}

BST_ERROR bst_mt_lmtx_read_unlock(bst_mt_lmtx_t *bst) {
    if (pthread_mutex_unlock(&bst->mtx)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_READ_UNLOCK_FAILURE | PT_MUTEX_UNLOCK_FAILURE;
        }
        return PT_MUTEX_UNLOCK_FAILURE;
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_READ_UNLOCK_FAILURE;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_read_unlock_node(bst_mt_lmtx_t *bst,
                                       bst_mt_lmtx_node_t *root) {
    if (pthread_mutex_unlock(&root->mtx)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_MUTEX_UNLOCK_FAILURE | PT_RWLOCK_READ_UNLOCK_FAILURE;
        }
        return PT_MUTEX_UNLOCK_FAILURE;
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_READ_UNLOCK_FAILURE;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_add(bst_mt_lmtx_t *bst, int64_t value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_rdlock(&bst->rwl)) {
        return PT_RWLOCK_READ_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_mutex_lock(&bst->mtx)) {
            return PT_MUTEX_LOCK_FAILURE;
        }

        if (bst->root == NULL) {
            BST_ERROR err;
            bst_mt_lmtx_node_t *node = bst_mt_lmtx_node_new(value, NULL, &err);

            if (IS_SUCCESS(err)) {
                bst->root = node;
                return bst_mt_lmtx_read_unlock(bst) | VALUE_ADDED;
            }

            return bst_mt_lmtx_read_unlock(bst) | err | VALUE_NOT_ADDED;
        }

        if (pthread_mutex_unlock(&bst->mtx)) {
            return PT_MUTEX_UNLOCK_FAILURE | VALUE_NOT_ADDED;
        }
    }

    bst_mt_lmtx_node_t *root = bst->root, *tmp;
    while (root != NULL) {
        if (value - root->value < 0) {
            if (pthread_mutex_lock(&root->mtx)) {
                return PT_MUTEX_LOCK_FAILURE | VALUE_NOT_ADDED;
            }

            if (value - root->value < 0) {
                if (root->left == NULL) {
                    BST_ERROR err;
                    bst_mt_lmtx_node_t *node =
                        bst_mt_lmtx_node_new(value, root, &err);

                    if (IS_SUCCESS(err)) {
                        root->left = node;
                        return bst_mt_lmtx_read_unlock_node(bst, root) |
                               VALUE_ADDED;
                    }

                    return bst_mt_lmtx_read_unlock_node(bst, root) | err |
                           VALUE_NOT_ADDED;
                }

                tmp = root;
                root = root->left;
                if (pthread_mutex_unlock(&tmp->mtx)) {
                    return PT_MUTEX_UNLOCK_FAILURE | VALUE_NOT_ADDED;
                }
                continue;
            } else if (value - root->value > 0) {
                if (root->right == NULL) {
                    BST_ERROR err;
                    bst_mt_lmtx_node_t *node =
                        bst_mt_lmtx_node_new(value, root, &err);

                    if (IS_SUCCESS(err)) {
                        root->right = node;
                        return bst_mt_lmtx_read_unlock_node(bst, root);
                    }

                    return bst_mt_lmtx_read_unlock_node(bst, root) | err |
                           VALUE_NOT_ADDED;
                }

                tmp = root;
                root = root->right;
                if (pthread_mutex_unlock(&tmp->mtx)) {
                    return PT_MUTEX_UNLOCK_FAILURE | VALUE_NOT_ADDED;
                }
                continue;
            } else {
                BST_ERROR err = bst_mt_lmtx_read_unlock_node(bst, root);
                return err == SUCCESS ? VALUE_EXISTS : VALUE_EXISTS | err;
            }
        } else if (value - root->value > 0) {
            if (pthread_mutex_lock(&root->mtx)) {
                return PT_MUTEX_LOCK_FAILURE | VALUE_NOT_ADDED;
            }

            if (value - root->value < 0) {
                if (root->left == NULL) {
                    BST_ERROR err;
                    bst_mt_lmtx_node_t *node =
                        bst_mt_lmtx_node_new(value, root, &err);

                    if (IS_SUCCESS(err)) {
                        root->left = node;
                        return bst_mt_lmtx_read_unlock_node(bst, root);
                    }

                    return bst_mt_lmtx_read_unlock_node(bst, root) | err |
                           VALUE_NOT_ADDED;
                }

                tmp = root;
                root = root->left;
                if (pthread_mutex_unlock(&tmp->mtx)) {
                    return PT_MUTEX_UNLOCK_FAILURE | VALUE_NOT_ADDED;
                }
                continue;
            } else if (value - root->value > 0) {
                if (root->right == NULL) {
                    BST_ERROR err;
                    bst_mt_lmtx_node_t *node =
                        bst_mt_lmtx_node_new(value, root, &err);

                    if (IS_SUCCESS(err)) {
                        root->right = node;
                        return bst_mt_lmtx_read_unlock_node(bst, root);
                    }

                    return bst_mt_lmtx_read_unlock_node(bst, root) | err |
                           VALUE_NOT_ADDED;
                }

                tmp = root;
                root = root->right;
                if (pthread_mutex_unlock(&tmp->mtx)) {
                    return PT_MUTEX_UNLOCK_FAILURE;
                }
                continue;
            } else {
                BST_ERROR err = bst_mt_lmtx_read_unlock_node(bst, root);
                return err == SUCCESS ? VALUE_EXISTS : VALUE_EXISTS | err;
            }
        } else {
            return VALUE_EXISTS;
        }
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_READ_UNLOCK_FAILURE | UNKNOWN;
    }

    // Should never get here
    return UNKNOWN;
}

BST_ERROR bst_mt_lmtx_search(bst_mt_lmtx_t *bst, int64_t value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    bst_mt_lmtx_node_t *root = bst->root;

    while (root != NULL) {
        if (root->value == value) {
            if (pthread_rwlock_unlock(&bst->rwl)) {
                return PT_RWLOCK_WRITE_UNLOCK_FAILURE | VALUE_EXISTS;
            }

            return VALUE_EXISTS;
        } else if (value - root->value < 0) {
            root = root->left;
        } else if (value - root->value > 0) {
            root = root->right;
        }
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | VALUE_NONEXISTENT;
    }

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_mt_lmtx_min(bst_mt_lmtx_t *bst, int64_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    bst_mt_lmtx_node_t *root = bst->root;

    while (root->left != NULL) {
        root = root->left;
    }

    if (value != NULL) {
        *value = root->value;
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_max(bst_mt_lmtx_t *bst, int64_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    bst_mt_lmtx_node_t *root = bst->root;

    while (root->right != NULL) {
        root = root->right;
    }

    if (value != NULL) {
        *value = root->value;
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_node_count(bst_mt_lmtx_t *bst, size_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    size_t count = 0;
    if (bst->root == NULL) {
        if (value != NULL) {
            *value = count;
        }
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t size = 10;
    int64_t top = -1;
    bst_mt_lmtx_node_t **stack =
        (bst_mt_lmtx_node_t **)malloc(sizeof **stack * size);

    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    stack[++top] = bst->root;

    while (top != -1) {
        bst_mt_lmtx_node_t *node = stack[top--];
        count++;

        if (top + 2 >= size) {
            bst_mt_lmtx_node_t **tmp = stack;
            stack = realloc(stack, sizeof **stack * size * 2);

            if (stack != NULL) {
                size *= 2;
            } else {
                free(tmp);
                if (pthread_rwlock_unlock(&bst->rwl)) {
                    return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
                }
                return MALLOC_FAILURE;
            }
        }

        // Push children to stack
        if (node != NULL) {
            if (node->right != NULL) {
                stack[++top] = node->right;
            }
            if (node->left != NULL) {
                stack[++top] = node->left;
            }
        }
    }

    if (value != NULL) {
        *value = count;
    }

    free(stack);

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_node_count_no_lock(bst_mt_lmtx_t *bst, size_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    size_t count = 0;
    if (bst->root == NULL) {
        if (value != NULL) {
            *value = count;
        }
        return BST_EMPTY;
    }

    size_t size = 10;
    int64_t top = -1;
    bst_mt_lmtx_node_t **stack =
        (bst_mt_lmtx_node_t **)malloc(sizeof **stack * size);

    if (stack == NULL) {
        return MALLOC_FAILURE;
    }

    stack[++top] = bst->root;

    while (top != -1) {
        bst_mt_lmtx_node_t *node = stack[top--];
        count++;

        if (top + 2 >= size) {
            bst_mt_lmtx_node_t **tmp = stack;
            stack = realloc(stack, sizeof **stack * size * 2);

            if (stack != NULL) {
                size *= 2;
            } else {
                free(tmp);
                return MALLOC_FAILURE;
            }
        }

        // Push children to stack
        if (node != NULL) {
            if (node->right != NULL) {
                stack[++top] = node->right;
            }
            if (node->left != NULL) {
                stack[++top] = node->left;
            }
        }
    }

    if (value != NULL) {
        *value = count;
    }

    free(stack);

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_height(bst_mt_lmtx_t *bst, size_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (value != NULL) {
            *value = 0;
        }
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    // Stack for tree nodes to avoid recursion and allow easier concurrency
    // control
    struct Stack {
        bst_mt_lmtx_node_t *node;
        size_t depth;
    };
    size_t bst_size = 0;
    BST_ERROR be = bst_mt_lmtx_node_count_no_lock(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | be;
        }
        return be;
    }

    struct Stack *stack = malloc(sizeof *stack * bst_size);

    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    size_t stack_size = 0;

    // Initial push of the root node with depth 0
    stack[stack_size++] = (struct Stack){bst->root, 0};

    size_t max_depth = 0;

    while (stack_size > 0) {
        // Pop the top element from stack
        struct Stack top = stack[--stack_size];
        bst_mt_lmtx_node_t *current = top.node;
        size_t current_depth = top.depth;

        // Update maximum depth found
        if (current_depth >= max_depth) {
            max_depth = current_depth;
        }

        // Push children to the stack with incremented depth
        if (current->right != NULL) {
            stack[stack_size++] =
                (struct Stack){current->right, current_depth + 1};
        }
        if (current->left != NULL) {
            stack[stack_size++] =
                (struct Stack){current->left, current_depth + 1};
        }
    }

    free(stack);

    if (value != NULL) {
        *value = max_depth;
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_width(bst_mt_lmtx_t *bst, size_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (value != NULL) {
            *value = 0;
        }
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    int64_t w = 0;
    size_t bst_size = 0;
    BST_ERROR be = bst_mt_lmtx_node_count_no_lock(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | be;
        }
        return be;
    }

    bst_mt_lmtx_node_t **q = malloc(sizeof **q * bst_size);

    if (q == NULL) {
        if (pthread_mutex_unlock(&bst->mtx)) {
            return PT_MUTEX_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    int64_t f = 0, r = 0;

    q[r++] = bst->root;

    while (f < r) {
        // Compute the number of nodes at the current level.
        int64_t count = r - f;

        // Update the maximum width if the current level's width is greater.
        w = w > count ? w : count;

        // Loop through each node on the current level and enqueue children.
        for (int i = 0; i < count; i++) {
            bst_mt_lmtx_node_t *n = q[f++];
            if (n->left != NULL) {
                q[r++] = n->left;
            }

            if (n->right != NULL) {
                q[r++] = n->right;
            }
        }
    }

    free(q);

    if (value != NULL) {
        *value = w;
    }

    if (pthread_mutex_unlock(&bst->mtx)) {
        return PT_MUTEX_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_traverse_preorder(bst_mt_lmtx_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    BST_ERROR be = bst_mt_lmtx_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | be;
        }
        return be;
    }

    // Stack to store the nodes
    bst_mt_lmtx_node_t **stack = malloc(sizeof **stack * bst_size);

    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t top = 0;
    stack[top++] = bst->root;

    while (top > 0) {
        bst_mt_lmtx_node_t *node = stack[--top];
        printf("%ld ", node->value);

        // Push right child first so that it is processed after the left child
        if (node->right != NULL) {
            stack[top++] = node->right;
        }

        // Push left child
        if (node->left != NULL) {
            stack[top++] = node->left;
        }
    }

    printf("\n");

    free(stack);

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_traverse_inorder(bst_mt_lmtx_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    BST_ERROR be = bst_mt_lmtx_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | be;
        }
        return be;
    }

    // Stack to store the nodes
    bst_mt_lmtx_node_t **stack = malloc(sizeof **stack * bst_size);
    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t top = 0;
    bst_mt_lmtx_node_t *current = bst->root;

    while (current != NULL || top > 0) {
        // Reach the left most Node of the current Node
        while (current != NULL) {
            // Place pointer to a tree node on the stack before traversing the
            // node's left subtree
            stack[top++] = current;
            current = current->left;
        }

        // Current must be NULL at this point
        current = stack[--top];
        printf("%ld ", current->value);

        // We have visited the node and its left subtree. Now, it's right
        // subtree's turn
        current = current->right;
    }

    printf("\n");

    free(stack);

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_traverse_postorder(bst_mt_lmtx_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    BST_ERROR be = bst_mt_lmtx_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | be;
        }
        return be;
    }

    // Stack to store the nodes
    bst_mt_lmtx_node_t **stack1 = malloc(sizeof **stack1 * bst_size);
    if (stack1 == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    bst_mt_lmtx_node_t **stack2 = malloc(sizeof **stack2 * bst_size);
    if (stack2 == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t top1 = 0, top2 = 0;

    stack1[top1++] = bst->root;
    bst_mt_lmtx_node_t *node;

    // Run while first stack is not empty
    while (top1 > 0) {
        // Pop an item from stack1 and push it to stack2
        node = stack1[--top1];
        stack2[top2++] = node;

        // Push left and right children of removed item to stack1
        if (node->left) {
            stack1[top1++] = node->left;
        }

        if (node->right) {
            stack1[top1++] = node->right;
        }
    }

    // Print all elements of second stack
    while (top2 > 0) {
        node = stack2[--top2];
        printf("%ld ", node->value);
    }

    printf("\n");

    free(stack1);
    free(stack2);

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lmtx_delete(bst_mt_lmtx_t *bst, int64_t value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    bst_mt_lmtx_node_t *current = bst->root, *parent = NULL;

    // Find the node
    while (current != NULL && current->value != value) {
        if (value < current->value) {
            current = current->left;
        } else {
            current = current->right;
        }
    }

    if (current == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | VALUE_NONEXISTENT;
        }

        return VALUE_NONEXISTENT;
    }

    parent = current->parent;

    // Node with two children
    if (current->left != NULL && current->right != NULL) {
        bst_mt_lmtx_node_t *successor = current->right;
        bst_mt_lmtx_node_t *successor_parent = current;

        // Find in-order successor and its parent
        while (successor->left != NULL) {
            successor_parent = successor;
            successor = successor->left;
        }

        // Replace current node's data with successor's data
        current->value = successor->value;

        // Move pointers to delete successor
        current = successor;
        parent = successor_parent;
    }

    // Node with one or zero children
    bst_mt_lmtx_node_t *child =
        (current->left != NULL) ? current->left : current->right;
    if (parent == NULL) {
        bst->root = child; // Delete the root node
    } else if (parent->left == current) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    free(current);

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

void save_inorder(bst_mt_lmtx_node_t *node, int64_t *inorder, size_t *index) {
    if (node == NULL)
        return;
    save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    save_inorder(node->right, inorder, index);
}

void bst_node_free(bst_mt_lmtx_node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_node_free(root->right);
    }

    pthread_mutex_destroy(&root->mtx);
    free(root);
}

bst_mt_lmtx_node_t *array_to_bst(int64_t *arr, size_t start, size_t end,
                                 bst_mt_lmtx_node_t *parent, BST_ERROR *err) {
    if (start > end) {
        if (err != NULL) {
            *err = SUCCESS;
        }
        return NULL;
    }

    size_t mid = (start + end) / 2;

    BST_ERROR e;

    bst_mt_lmtx_node_t *node = bst_mt_lmtx_node_new(arr[mid], parent, &e);

    if (IS_SUCCESS(e)) {
        node->left = array_to_bst(arr, start, mid - 1, node, &e);

        if (IS_SUCCESS(e)) {
            node->right = array_to_bst(arr, mid + 1, end, node, &e);

            if (IS_SUCCESS(e)) {
                if (err != NULL) {
                    *err = SUCCESS;
                }
                return node;
            }

            bst_node_free(node);

            if (err != NULL) {
                *err = e;
            }

            return NULL;
        }

        bst_node_free(node);

        if (err != NULL) {
            *err = e;
        }

        return NULL;
    }

    if (err != NULL) {
        *err = e;
    }

    return NULL;
}

BST_ERROR bst_mt_lmtx_rebalance(bst_mt_lmtx_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    if (bst->root == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    BST_ERROR be = bst_mt_lmtx_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | be;
        }
        return be;
    }

    int64_t *inorder = malloc(sizeof(int64_t) * bst_size);

    if (inorder == NULL) {
        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t index = 0;
    save_inorder(bst->root, inorder, &index);

    bst_node_free(bst->root);

    BST_ERROR err;
    bst_mt_lmtx_node_t *node = array_to_bst(inorder, 0, index - 1, NULL, &err);

    if (IS_SUCCESS(err)) {
        free(inorder);
        bst->root = node;

        if (pthread_rwlock_unlock(&bst->rwl)) {
            return PT_RWLOCK_WRITE_UNLOCK_FAILURE | SUCCESS;
        }
        return SUCCESS;
    }

    free(inorder);

    if (pthread_rwlock_unlock(&bst->rwl)) {
        if (pthread_mutex_destroy(&bst->mtx)) {
            if (pthread_rwlock_destroy(&bst->rwl)) {
                free(bst);
                return PT_RWLOCK_DESTROY_FAILURE | PT_MUTEX_DESTROY_FAILURE |
                       PT_MUTEX_UNLOCK_FAILURE | err;
            }
            free(bst);
            return PT_MUTEX_DESTROY_FAILURE | PT_MUTEX_UNLOCK_FAILURE | err;
        }

        free(bst);

        return PT_MUTEX_UNLOCK_FAILURE | err;
    }

    if (pthread_mutex_destroy(&bst->mtx)) {
        if (pthread_rwlock_destroy(&bst->rwl)) {
            free(bst);
            return PT_RWLOCK_DESTROY_FAILURE | PT_MUTEX_DESTROY_FAILURE | err;
        }
        free(bst);
        return PT_MUTEX_DESTROY_FAILURE | err;
    }

    free(bst);
    return err;
}

BST_ERROR bst_mt_lmtx_free(bst_mt_lmtx_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (pthread_mutex_lock(&bst->mtx)) {
        return PT_MUTEX_LOCK_FAILURE;
    }

    if (pthread_rwlock_wrlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_LOCK_FAILURE;
    }

    bst_node_free(bst->root);
    bst->root = NULL;

    if (pthread_mutex_unlock(&bst->mtx)) {
        return PT_MUTEX_UNLOCK_FAILURE;
    }

    if (pthread_mutex_destroy(&bst->mtx)) {
        return PT_MUTEX_DESTROY_FAILURE;
    }

    if (pthread_rwlock_unlock(&bst->rwl)) {
        return PT_RWLOCK_WRITE_UNLOCK_FAILURE;
    }

    if (pthread_rwlock_destroy(&bst->rwl)) {
        return PT_RWLOCK_DESTROY_FAILURE;
    }

    free(bst);

    bst = NULL;

    return SUCCESS;
}