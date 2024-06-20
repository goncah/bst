/*
Universidade Aberta
File: bst_mt_cgl.c
Author: Hugo Gonçalves, 2100562

MT Safe Coarse-Grained Lock BST using pthreads RwLock.

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
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/bst_common.h"
#include "include/bst_mt_cgl.h"

bst_mt_cgl_node_t *bst_mt_grwl_node_new(const int64_t value, BST_ERROR *err) {
    bst_mt_cgl_node_t *node = malloc(sizeof(bst_mt_cgl_node_t));

    if (node == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
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

bst_mt_cgl_t *bst_mt_cgl_new(BST_ERROR *err) {
    bst_mt_cgl_t *bst = malloc(sizeof(bst_mt_cgl_t));

    if (bst == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }
        return NULL;
    }

    if (pthread_rwlock_init(&bst->rwl, NULL)) {
        if (err != NULL) {
            *err = PT_RWLOCK_INIT_FAILURE;
        }

        free(bst);
        return NULL;
    }

    if (pthread_rwlock_init(&bst->rwl, NULL)) {
        free(bst);

        if (err != NULL) {
            *err = PT_RWLOCK_INIT_FAILURE;
        }

        return NULL;
    }

    bst->count = 0;
    bst->root = NULL;

    if (err != NULL) {
        *err = SUCCESS;
    }

    return bst;
}

BST_ERROR bst_mt_cgl_add(bst_mt_cgl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        BST_ERROR err;
        bst_mt_cgl_node_t *node = bst_mt_grwl_node_new(value, &err);

        if ((err & SUCCESS) == SUCCESS) {
            bst_->root = node;
            bst_->count++;

            if (pthread_rwlock_unlock(&bst_->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
            }

            return SUCCESS;
        }

        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | err;
        }

        return err;
    }

    bst_mt_cgl_node_t *root = bst_->root;

    while (root != NULL) {
        if (compare(value, root->value) < 0) {
            if (root->left == NULL) {
                BST_ERROR err;
                bst_mt_cgl_node_t *node = bst_mt_grwl_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->left = node;
                    bst_->count++;

                    if (pthread_rwlock_unlock(&bst_->rwl)) {
                        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
                    }

                    return SUCCESS;
                }

                if (pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_UNLOCK_FAILURE | err;
                }

                return err;
            }

            root = root->left;
        } else if (compare(value, root->value) > 0) {
            if (root->right == NULL) {
                BST_ERROR err;
                bst_mt_cgl_node_t *node = bst_mt_grwl_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->right = node;
                    bst_->count++;

                    if (pthread_rwlock_unlock(&bst_->rwl)) {
                        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
                    }

                    return SUCCESS;
                }

                if (pthread_rwlock_unlock(&bst_->rwl)) {
                    return PT_RWLOCK_UNLOCK_FAILURE | err;
                }

                return err;
            }

            root = root->right;
        } else {
            // Value already exists
            if (pthread_rwlock_unlock(&bst_->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | VALUE_EXISTS;
            }

            return VALUE_EXISTS;
        }
    }

    // Should never get here
    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | UNKNOWN;
    }

    return UNKNOWN;
}

BST_ERROR bst_mt_cgl_search(bst_mt_cgl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_cgl_node_t *root = bst_->root;

    while (root != NULL) {
        if (root->value == value) {
            if (pthread_rwlock_unlock(&bst_->rwl)) {
                return PT_RWLOCK_UNLOCK_FAILURE | VALUE_EXISTS;
            }

            return VALUE_EXISTS;
        } else if (compare(value, root->value) < 0) {
            root = root->left;
        } else if (compare(value, root->value) > 0) {
            root = root->right;
        }
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | VALUE_NONEXISTENT;
    }

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_mt_cgl_min(bst_mt_cgl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_cgl_node_t *root = bst_->root;

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

BST_ERROR bst_mt_cgl_max(bst_mt_cgl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_cgl_node_t *root = bst_->root;

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

BST_ERROR bst_mt_cgl_node_count(bst_mt_cgl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (value != NULL) {
        *value = bst_->count;
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_cgl_delete(bst_mt_cgl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    bst_mt_cgl_node_t *current = bst_->root, *parent = NULL;

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
        bst_mt_cgl_node_t *successor = current->right;
        bst_mt_cgl_node_t *successor_parent = current;

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
    bst_mt_cgl_node_t *child =
        current->left != NULL ? current->left : current->right;
    if (parent == NULL) {
        bst_->root = child; // Delete the root node
    } else if (parent->left == current) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    free(current);

    bst_->count--;

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

void bst_mt_grwl_node_free(bst_mt_cgl_node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_mt_grwl_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_mt_grwl_node_free(root->right);
    }

    free(root);
}

BST_ERROR bst_mt_cgl_free(bst_mt_cgl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_cgl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    *bst = NULL; // No other operations will start

    bst_mt_grwl_node_free(bst_->root);
    bst_->root = NULL;
    bst_->count = 0;

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE;
    }

    if (pthread_rwlock_destroy(&bst_->rwl)) {
        return PT_RWLOCK_DESTROY_FAILURE;
    }

    free(bst_);

    return SUCCESS;
}