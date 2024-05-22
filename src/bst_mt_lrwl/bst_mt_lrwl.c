/*
Universidade Aberta
File: bst_mt_lrwl_t.c
Author: Hugo Gonçalves, 2100562

MT Safe BST using a both a global and a local RwLock, the latter with write
preference. This further improves the insert performance due to no global
lock on insert. The global RwLock is used inverted meaning that insert
operation will read lock it and all others will write lock it. The local
RwLock is write locked when inserting and read locked on all
other operations.

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
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/bst_common.h"
#include "include/bst_mt_lrwl.h"

static int64_t compare(const int64_t a, const int64_t b) {
    struct timespec ts = {0, 10};
    int res = 0;
    errno = 0;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return a - b;
}

bst_mt_lrwl_node_t *bst_mt_lrwl_node_new(const int64_t value, BST_ERROR *err) {
    bst_mt_lrwl_node_t *node = malloc(sizeof(bst_mt_lrwl_node_t));

    if (node == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    pthread_rwlockattr_t rtx_attr;

    if (pthread_rwlockattr_setkind_np(
            &rtx_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)) {
        free(node);
        if (err != NULL) {
            *err = PT_RWLOCK_ATTR_INIT_FAILURE;
        }

        return NULL;
    }

    if (pthread_rwlock_init(&node->rwl, &rtx_attr)) {
        free(node);
        if (err != NULL) {
            *err = PT_RWLOCK_INIT_FAILURE;
        }
        return NULL;
    }

    node->value = value;
    node->left = NULL;
    node->right = NULL;

    if (err != NULL) {
        *err = SUCCESS;
    }

    return node;
}

bst_mt_lrwl_t *bst_mt_lrwl_new(BST_ERROR *err) {
    bst_mt_lrwl_t *bst = malloc(sizeof(bst_mt_lrwl_t));

    if (bst == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    if (pthread_rwlock_init(&bst->rwl, NULL)) {
        free(bst);
        if (err != NULL) {
            *err = PT_RWLOCK_INIT_FAILURE;
        }
        return NULL;
    }

    if (pthread_rwlock_init(&bst->crwl, NULL)) {
        free(bst);
        if (err != NULL) {
            *err = PT_RWLOCK_INIT_FAILURE;
        }
        return NULL;
    }

    bst->count = 0;
    bst->root = NULL;

    return bst;
}

BST_ERROR bst_mt_lrwl_add(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (bst_->root == NULL) {
        if (pthread_rwlock_wrlock(&bst_->rwl)) {
            return PT_RWLOCK_LOCK_FAILURE;
        }

        if (bst_->root == NULL) {
            BST_ERROR err;
            bst_mt_lrwl_node_t *node = bst_mt_lrwl_node_new(value, &err);

            if (IS_SUCCESS(err)) {
                bst_->root = node;
                if (pthread_rwlock_wrlock(&bst_->crwl)) {
                    if (pthread_rwlock_unlock(&bst_->rwl)) {
                        return PT_RWLOCK_UNLOCK_FAILURE;
                    }
                    return PT_RWLOCK_LOCK_FAILURE;
                }

                bst_->count++;

                if (pthread_rwlock_unlock(&bst_->crwl) ||
                    pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_LOCK_FAILURE;
                }

                return SUCCESS;
            }
        }

        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NOT_ADDED;
        }
    }

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    bst_mt_lrwl_node_t *root = bst_->root, *tmp;
    while (root != NULL) {
        if (pthread_rwlock_wrlock(&root->rwl)) {
            return PT_RWLOCK_LOCK_FAILURE;
        }

        if (compare(value, root->value) < 0) {
            if (root->left == NULL) {
                BST_ERROR err;
                bst_mt_lrwl_node_t *node = bst_mt_lrwl_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->left = node;

                    if (pthread_rwlock_wrlock(&bst_->crwl)) {
                        if (pthread_rwlock_unlock(&root->rwl) ||
                            pthread_rwlock_unlock(&bst_->rwl)) {
                            return PT_RWLOCK_UNLOCK_FAILURE;
                        }
                        return PT_RWLOCK_LOCK_FAILURE;
                    }

                    bst_->count++;

                    if (pthread_rwlock_unlock(&root->rwl) ||
                        pthread_rwlock_unlock(&bst_->crwl) ||
                        pthread_rwlock_unlock(&bst_->rwl)) {
                        return PT_RWLOCK_UNLOCK_FAILURE;
                    }

                    return SUCCESS;
                }

                if (pthread_rwlock_unlock(&root->rwl) ||
                    pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NOT_ADDED | err;
                }

                return VALUE_NOT_ADDED | err;
            }

            tmp = root;
            root = root->left;
            if (pthread_rwlock_unlock(&tmp->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NOT_ADDED;
            }
        } else if (compare(value, root->value) > 0) {
            if (root->right == NULL) {
                BST_ERROR err;
                bst_mt_lrwl_node_t *node = bst_mt_lrwl_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->right = node;

                    if (pthread_rwlock_wrlock(&bst_->crwl)) {
                        if (pthread_rwlock_unlock(&root->rwl) ||
                            pthread_rwlock_unlock(&bst_->rwl)) {
                            return PT_RWLOCK_UNLOCK_FAILURE;
                        }
                        return PT_RWLOCK_LOCK_FAILURE;
                    }

                    bst_->count++;

                    if (pthread_rwlock_unlock(&root->rwl) ||
                        pthread_rwlock_unlock(&bst_->crwl) ||
                        pthread_rwlock_unlock(&bst_->rwl)) {
                        return PT_RWLOCK_UNLOCK_FAILURE;
                    }

                    return SUCCESS;
                }

                if (pthread_rwlock_unlock(&root->rwl) ||
                    pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NOT_ADDED | err;
                }

                return VALUE_NOT_ADDED | err;
            }

            tmp = root;
            root = root->right;
            if (pthread_rwlock_unlock(&tmp->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NOT_ADDED;
            }
        } else {
            if (pthread_rwlock_unlock(&root->rwl) ||
                pthread_rwlock_unlock(&bst_->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NOT_ADDED;
            }
            return VALUE_EXISTS;
        }
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | UNKNOWN;
    }

    // Should never get here
    return UNKNOWN;
}

BST_ERROR bst_mt_lrwl_search(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    bst_mt_lrwl_node_t *root = bst_->root;

    while (root != NULL) {
        if (pthread_rwlock_rdlock(&root->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE;
        }

        if (root->value == value) {
            if (pthread_rwlock_unlock(&root->rwl) ||
                pthread_rwlock_unlock(&bst_->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | VALUE_EXISTS;
            }

            return VALUE_EXISTS;
        } else if (compare(value, root->value) < 0) {
            bst_mt_lrwl_node_t *tmp = root;
            root = root->left;

            if (pthread_rwlock_unlock(&tmp->rwl)) {
                if (pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_UNLOCK_FAILURE;
                }
                return PT_RWLOCK_UNLOCK_FAILURE;
            }

        } else if (compare(value, root->value) > 0) {
            bst_mt_lrwl_node_t *tmp = root;
            root = root->right;

            if (pthread_rwlock_unlock(&tmp->rwl)) {
                if (pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_UNLOCK_FAILURE;
                }
                return PT_RWLOCK_UNLOCK_FAILURE;
            }
        }
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NONEXISTENT;
    }

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_mt_lrwl_min(bst_mt_lrwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_lrwl_node_t *root = bst_->root;

    while (root->left != NULL) {
        root = root->left;
    }

    if (value != NULL) {
        *value = root->value;
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_max(bst_mt_lrwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_lrwl_node_t *root = bst_->root;

    while (root->right != NULL) {
        root = root->right;
    }

    if (value != NULL) {
        *value = root->value;
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_node_count(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->crwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (value != NULL) {
        *value = bst_->count;
    }

    if (pthread_rwlock_unlock(&bst_->crwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_height(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (value != NULL) {
            *value = 0;
        }

        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    // Stack for tree nodes to avoid recursion allowing easier concurrency
    // control
    struct Stack {
        bst_mt_lrwl_node_t *node;
        size_t depth;
    };
    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | be;
        }
        return be;
    }

    struct Stack *stack = malloc(sizeof *stack * bst_size);

    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    size_t stack_size = 0;

    // Initial push of the root node with depth 0
    stack[stack_size++] = (struct Stack){bst_->root, 0};

    size_t max_depth = 0;

    while (stack_size > 0) {
        // Pop the top element from stack
        const struct Stack top = stack[--stack_size];
        const bst_mt_lrwl_node_t *current = top.node;
        const size_t current_depth = top.depth;

        // Update maximum depth found
        if (current_depth >= max_depth) {
            max_depth = current_depth;
        }

        // Push children to the stack with incremented depth
        if (current != NULL && current->right != NULL) {
            stack[stack_size++] =
                (struct Stack){current->right, current_depth + 1};
        }
        if (current != NULL && current->left != NULL) {
            stack[stack_size++] =
                (struct Stack){current->left, current_depth + 1};
        }
    }

    free(stack);

    if (value != NULL) {
        *value = max_depth;
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_width(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (value != NULL) {
            *value = 0;
        }

        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    int64_t w = 0;
    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | be;
        }
        return be;
    }

    bst_mt_lrwl_node_t **q = malloc(sizeof **q * bst_size);

    if (q == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    int64_t f = 0, r = 0;

    q[r++] = bst_->root;

    while (f < r) {
        // Compute the number of nodes at the current level.
        const int64_t count = r - f;

        // Update the maximum width if the current level's width is greater.
        w = w > count ? w : count;

        // Loop through each node on the current level and enqueue children.
        for (int i = 0; i < count; i++) {
            const bst_mt_lrwl_node_t *n = q[f++];
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

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_preorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | be;
        }
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack = malloc(sizeof **stack * bst_size);

    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t top = 0;
    stack[top++] = bst_->root;

    while (top > 0) {
        const bst_mt_lrwl_node_t *node = stack[--top];
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

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_inorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | be;
        }
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack = malloc(sizeof **stack * bst_size);
    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t top = 0;
    bst_mt_lrwl_node_t *current = bst_->root;

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

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_postorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | be;
        }
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack1 = malloc(sizeof **stack1 * bst_size);
    if (stack1 == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    bst_mt_lrwl_node_t **stack2 = malloc(sizeof **stack2 * bst_size);
    if (stack2 == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    size_t top1 = 0, top2 = 0;

    stack1[top1++] = bst_->root;
    bst_mt_lrwl_node_t *node;

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

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_delete(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    bst_mt_lrwl_node_t *current = bst_->root, *parent = NULL;

    // Find the node
    while (current != NULL && current->value != value) {
        if (compare(value, current->value) < 0) {
            parent = current;
            current = current->left;
        } else {
            parent = current;
            current = current->right;
        }
    }

    if (current == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NONEXISTENT;
        }

        return VALUE_NONEXISTENT;
    }

    // Node with two children
    if (current->left != NULL && current->right != NULL) {
        bst_mt_lrwl_node_t *successor = current->right;
        bst_mt_lrwl_node_t *successor_parent = current;

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
    bst_mt_lrwl_node_t *child =
        current->left != NULL ? current->left : current->right;
    if (parent == NULL) {
        bst_->root = child; // Delete the root node
    } else if (parent->left == current) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    free(current);

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

void lrwl_save_inorder(const bst_mt_lrwl_node_t *node, int64_t *inorder,
                       int64_t *index) {
    if (node == NULL)
        return;
    lrwl_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    lrwl_save_inorder(node->right, inorder, index);
}

void bst_mt_lrwl_node_free(bst_mt_lrwl_node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_mt_lrwl_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_mt_lrwl_node_free(root->right);
    }

    pthread_rwlock_destroy(&root->rwl);
    free(root);
}

bst_mt_lrwl_node_t *lrwl_array_to_bst(int64_t *arr, const int64_t start,
                                      const int64_t end, BST_ERROR *err) {
    if (start > end) {
        if (err != NULL) {
            *err = SUCCESS;
        }
        return NULL;
    }

    const int64_t mid = (start + end) / 2;

    BST_ERROR e;

    bst_mt_lrwl_node_t *node = bst_mt_lrwl_node_new(arr[mid], &e);

    if (IS_SUCCESS(e)) {
        node->left = lrwl_array_to_bst(arr, start, mid - 1, &e);

        if (IS_SUCCESS(e)) {
            node->right = lrwl_array_to_bst(arr, mid + 1, end, &e);

            if (IS_SUCCESS(e)) {
                if (err != NULL) {
                    *err = SUCCESS;
                }
                return node;
            }

            bst_mt_lrwl_node_free(node);

            if (err != NULL) {
                *err = e;
            }

            return NULL;
        }

        bst_mt_lrwl_node_free(node);

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

BST_ERROR bst_mt_lrwl_rebalance(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | be;
        }
        return be;
    }

    int64_t *inorder = malloc(sizeof(int64_t) * bst_size);

    if (inorder == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    int64_t index = 0;
    lrwl_save_inorder(bst_->root, inorder, &index);

    bst_mt_lrwl_node_free(bst_->root);

    BST_ERROR err;
    bst_mt_lrwl_node_t *node = lrwl_array_to_bst(inorder, 0, index - 1, &err);

    if (IS_SUCCESS(err)) {
        free(inorder);
        bst_->root = node;

        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
        }

        return SUCCESS;
    }

    free(inorder);

    bst_->root = NULL;

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | err;
    }

    return err;
}

BST_ERROR bst_mt_lrwl_free(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    *bst = NULL;

    bst_mt_lrwl_node_free(bst_->root);
    bst_->root = NULL;

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE;
    }

    if (pthread_rwlock_destroy(&bst_->rwl)) {
        return PT_RWLOCK_DESTROY_FAILURE;
    }

    free(bst_);

    bst = NULL;

    return SUCCESS;
}