/*
Universidade Aberta
File: bst_at.c
Author: Hugo Gonçalves, 2100562

MT Safe BST using atomic operations.

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
#include <stdlib.h>

#include "../include/bst_common.h"
#include "include/bst_at.h"

static bst_at_node_t *bst_at_node_new(const int64_t value) {
    bst_at_node_t *node = malloc(sizeof(bst_at_node_t));

    if (node) {
        node->value = value;
        atomic_init(&node->left, NULL);
        atomic_init(&node->right, NULL);
    }

    return node;
}

bst_at_t *bst_at_new(memory_order mo, BST_ERROR *err) {
    bst_at_t *bst = malloc(sizeof(bst_at_t));

    if (bst) {
        bst->mo = mo;
        atomic_init(&bst->count, 0);
        atomic_init(&bst->root, NULL);

        if (err) {
            *err = SUCCESS;
        }
    } else if (err) {
        *err = MALLOC_FAILURE;
    }

    return bst;
}

BST_ERROR bst_at_add(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *node = bst_at_node_new(value);
    size_t retries = 0;

    if (node == NULL)
        return MALLOC_FAILURE;

    do {
        bst_at_node_t *expected = NULL;
        if (atomic_compare_exchange_strong_explicit(&bst_->root, &expected,
                                                    node, bst_->mo, bst_->mo)) {
            atomic_fetch_add(&bst_->count, 1);
            return SUCCESS;
        }

        bst_at_node_t *parent = NULL;
        bst_at_node_t *current = atomic_load_explicit(&bst_->root, bst_->mo);
        while (current != NULL) {
            parent = current;
            if (value < current->value) {
                current = atomic_load_explicit(&current->left, bst_->mo);
            } else if (value > current->value) {
                current = atomic_load_explicit(&current->right, bst_->mo);
            } else {
                free(node);
                return VALUE_EXISTS;
            }
        }

        expected = NULL;
        if (compare(value, parent->value) < 0) {
            if (atomic_compare_exchange_strong_explicit(
                    &parent->left, &expected, node, bst_->mo, bst_->mo)) {
                atomic_fetch_add_explicit(&bst_->count, 1, bst_->mo);
                return SUCCESS;
            }
        } else {
            if (atomic_compare_exchange_strong_explicit(
                    &parent->right, &expected, node, bst_->mo, bst_->mo)) {
                atomic_fetch_add_explicit(&bst_->count, 1, bst_->mo);
                return SUCCESS;
            }
        }
        retries++;
    } while (retries < CAS_FAILED_RETRY_MAX);

    free(node);
    return CAS_FAILED;
}

BST_ERROR bst_at_search(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *current = atomic_load_explicit(&bst_->root, bst_->mo);
    while (current != NULL) {
        if (compare(value, current->value) < 0) {
            current = atomic_load_explicit(&current->left, bst_->mo);
        } else if (value > current->value) {
            current = atomic_load_explicit(&current->right, bst_->mo);
        } else {
            return SUCCESS | VALUE_EXISTS;
        }
    }

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_at_min(bst_at_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *current = atomic_load_explicit(&bst_->root, bst_->mo);

    if (current) {
        while (current->left != NULL) {
            current = atomic_load_explicit(&current->left, bst_->mo);
        }

        if (value) {
            *value = current->value;
        }

        return SUCCESS;
    }

    return BST_EMPTY;
}

BST_ERROR bst_at_max(bst_at_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *current = atomic_load_explicit(&bst_->root, bst_->mo);

    if (current) {
        while (current->right != NULL) {
            current = atomic_load_explicit(&current->right, bst_->mo);
        }

        if (value) {
            *value = current->value;
        }

        return SUCCESS;
    }

    return BST_EMPTY;
}

BST_ERROR bst_at_node_count(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    size_t s = atomic_load_explicit(&bst_->count, bst_->mo);

    if (value) {
        *value = s;
    }

    return SUCCESS;
}

static __thread memory_order mo = memory_order_seq_cst;

static size_t bst_at_find_height(bst_at_node_t *r) {
    const bst_at_node_t *root = atomic_load_explicit(&r, mo);
    if (root == NULL) {
        return 0;
    } else {
        const size_t left_height = bst_at_find_height(root->left);
        const size_t right_height = bst_at_find_height(root->right);
        return 1 + (left_height > right_height ? left_height : right_height);
    }
}

BST_ERROR bst_at_height(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    const bst_at_t *bst_ = *bst;
    mo = bst_->mo;
    const size_t h = bst_at_find_height(bst_->root);

    if (value) {
        *value = h;
    }

    return SUCCESS;
}

static void count_nodes_at_level(const bst_at_node_t *r, const size_t level,
                                 size_t *count) {
    const bst_at_node_t *root = atomic_load_explicit(&r, mo);

    if (root == NULL) {
        return;
    }
    if (level == 0) {
        (*count)++;
    } else {
        count_nodes_at_level(root->left, level - 1, count);
        count_nodes_at_level(root->right, level - 1, count);
    }
}

BST_ERROR bst_at_width(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    const bst_at_t *bst_ = *bst;
    mo = bst_->mo;
    const size_t height = bst_at_find_height(bst_->root);
    const bst_at_node_t *root = atomic_load_explicit(&bst_->root, bst_->mo);
    size_t max_width = 0;

    for (size_t level = 0; level <= height; level++) {
        size_t count = 0;
        count_nodes_at_level(root, level, &count);
        if (count > max_width) {
            max_width = count;
        }
    }

    if (value) {
        *value = max_width;
    }

    return SUCCESS;
}

BST_ERROR bst_at_traverse_preorder(bst_at_t **bst) { return 0; }

BST_ERROR bst_at_traverse_inorder(bst_at_t **bst) { return 0; }

BST_ERROR bst_at_traverse_postorder(bst_at_t **bst) { return 0; }

BST_ERROR bst_at_delete(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *current = atomic_load_explicit(&bst_->root, bst_->mo);
    bst_at_node_t **link = &bst_->root;

    while (current != NULL && current->value != value) {
        if (compare(value, current->value) < 0) {
            link = &current->left;
            current = atomic_load_explicit(&current->left, bst_->mo);
        } else {
            link = &current->right;
            current = atomic_load_explicit(&current->right, bst_->mo);
        }
    }

    if (current == NULL)
        return VALUE_NONEXISTENT;

    if (current->left == NULL || current->right == NULL) {
        bst_at_node_t *child = current->left ? current->left : current->right;
        if (atomic_compare_exchange_strong_explicit(link, &current, child,
                                                    bst_->mo, bst_->mo)) {
            free(current);
            atomic_fetch_sub_explicit(&bst_->count, 1, bst_->mo);
            return SUCCESS;
        }
    } else {
        bst_at_node_t *successor = current->right;
        bst_at_node_t **successor_link = &current->right;

        while (atomic_load_explicit(&successor->left, bst_->mo) != NULL) {
            successor_link = &successor->left;
            successor = atomic_load_explicit(&successor->left, bst_->mo);
        }

        current->value = successor->value;
        if (atomic_compare_exchange_strong_explicit(successor_link, &successor,
                                                    successor->right, bst_->mo,
                                                    bst_->mo)) {
            atomic_fetch_sub_explicit(&bst_->count, 1, bst_->mo);
            return SUCCESS;
        }

        return CAS_FAILED;
    }

    return UNKNOWN;
}

BST_ERROR bst_at_rebalance(bst_at_t **bst) { return 0; }

static void bst_at_node_free(bst_at_node_t *root) {
    if (root == NULL) {
        return;
    }

    bst_at_node_free(atomic_load_explicit(&root->left, mo));

    bst_at_node_free(atomic_load_explicit(&root->right, mo));

    free(root);
}

BST_ERROR bst_at_free(bst_at_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;
    *bst = NULL;

    atomic_init(&bst_->count, 0);
    mo = bst_->mo;
    bst_at_node_free(bst_->root);

    free(bst_);

    return SUCCESS;
}