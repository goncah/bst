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
#include <stdbool.h>
#include <stdlib.h>

#include "../include/bst_common.h"
#include "include/bst_at.h"

#include <unistd.h>

static bst_at_node_t *bst_at_node_new(const int64_t value) {
    bst_at_node_t *node = malloc(sizeof(bst_at_node_t));

    if (node) {
        node->value = value;
        atomic_init(&node->ref_count, 0);
        atomic_init(&node->waiting_free, false);
        atomic_init(&node->left, NULL);
        atomic_init(&node->right, NULL);
    }

    return node;
}

// Helper function to load & increment node's rc
static bst_at_node_t *load_node(bst_at_node_t *n) {
    bst_at_node_t *node = atomic_load_explicit(&n, memory_order_acquire);

    if (node &&
        !atomic_load_explicit(&node->waiting_free, memory_order_acquire)) {
        atomic_fetch_add_explicit(&node->ref_count, 1, memory_order_acquire);
    } else {
        return NULL;
    }

    return node;
}

// Helper function to decrease a node's rc, if zero and free is TRUE, free the
// node, else loop until no one else holds a ref.
static void release_node(bst_at_node_t *n, bool f) {
    bst_at_node_t *node = atomic_load_explicit(&n, memory_order_acquire);
    if (node) {
        atomic_fetch_sub_explicit(&node->ref_count, 1, memory_order_release);
        atomic_bool c;
        atomic_init(&c, false);
        if (f && atomic_compare_exchange_strong_explicit(
                     &node->waiting_free, &c, true, memory_order_acq_rel,
                     memory_order_acquire)) {
            for (;;) {
                if (!atomic_load_explicit(&node->ref_count,
                                          memory_order_acquire)) {
                    free(node);
                    return;
                }
                //sleep(1);
            }
        }
    }
}

bst_at_t *bst_at_new(BST_ERROR *err) {
    bst_at_t *bst = malloc(sizeof(bst_at_t));

    if (bst) {
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

    if (node == NULL)
        return MALLOC_FAILURE;

    bst_at_node_t *current, *parent = NULL;

    do {
        bst_at_node_t *expected = NULL;
        if (atomic_compare_exchange_strong_explicit(&bst_->root, &expected,
                                                    node, memory_order_acq_rel,
                                                    memory_order_acquire)) {
            atomic_fetch_add_explicit(&bst_->count, 1, memory_order_release);
            return SUCCESS;
        }

        current = load_node(bst_->root);
        while (current != NULL) {
            parent = current;
            if (value < current->value) {
                bst_at_node_t *next = current->left;
                current = load_node(next);
                if (current)
                    release_node(parent, false);
            } else if (value > current->value) {
                bst_at_node_t *next = current->right;
                current = load_node(next);
                if (current)
                    release_node(parent, false);
            } else {
                free(node);
                release_node(parent, false);
                return VALUE_EXISTS;
            }
        }

        expected = NULL;
        if (compare(value, parent->value) < 0) {
            if (atomic_compare_exchange_strong_explicit(
                    &parent->left, &expected, node, memory_order_acq_rel,
                    memory_order_acquire)) {
                atomic_fetch_add_explicit(&bst_->count, 1,
                                          memory_order_release);
                release_node(parent, false);
                release_node(current, false);
                return SUCCESS;
            }
        } else {
            if (atomic_compare_exchange_strong_explicit(
                    &parent->right, &expected, node, memory_order_acq_rel,
                    memory_order_acquire)) {
                atomic_fetch_add_explicit(&bst_->count, 1,
                                          memory_order_release);
                release_node(parent, false);
                release_node(current, false);
                return SUCCESS;
            }
        }
        release_node(parent, false);
        release_node(current, false);
    } while (1);
}

BST_ERROR bst_at_search(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *current = load_node(bst_->root);
    while (current != NULL) {
        if (compare(value, current->value) < 0) {
            bst_at_node_t *next = current->left;
            release_node(current, false);
            current = load_node(next);
        } else if (value > current->value) {
            bst_at_node_t *next = current->right;
            release_node(current, false);
            current = load_node(next);
        } else {
            release_node(current, false);
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

    bst_at_node_t *current = load_node(bst_->root);

    if (current) {
        while (current->left != NULL) {
            bst_at_node_t *next = current->left;
            release_node(current, false);
            current = load_node(next);
        }

        if (value) {
            *value = current->value;
        }

        release_node(current, false);
        return SUCCESS;
    }

    return BST_EMPTY;
}

BST_ERROR bst_at_max(bst_at_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *current = load_node(bst_->root);

    if (current) {
        while (current->right != NULL) {
            bst_at_node_t *next = current->right;
            release_node(current, false);
            current = load_node(next);
        }

        if (value) {
            *value = current->value;
        }

        release_node(current, false);
        return SUCCESS;
    }

    return BST_EMPTY;
}

BST_ERROR bst_at_node_count(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    size_t s = atomic_load_explicit(&bst_->count, memory_order_acquire);

    if (value) {
        *value = s;
    }

    return SUCCESS;
}

static size_t bst_at_find_height(bst_at_node_t *r) {
    bst_at_node_t *root = load_node(r);

    if (root == NULL) {
        return 0;
    }

    const size_t left_height = bst_at_find_height(root->left);
    const size_t right_height = bst_at_find_height(root->right);
    release_node(root, false);
    return 1 + (left_height > right_height ? left_height : right_height);
}

BST_ERROR bst_at_height(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;
    const size_t h = bst_at_find_height(bst_->root);

    if (value) {
        *value = h;
    }

    return SUCCESS;
}

static void count_nodes_at_level(bst_at_node_t *r, const size_t level,
                                 size_t *count) {
    bst_at_node_t *root = load_node(r);

    if (root == NULL) {
        return;
    }

    if (level == 0) {
        (*count)++;
    } else {
        count_nodes_at_level(root->left, level - 1, count);
        count_nodes_at_level(root->right, level - 1, count);
    }

    release_node(root, false);
}

BST_ERROR bst_at_width(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;
    const size_t height = bst_at_find_height(bst_->root);
    bst_at_node_t *root = load_node(bst_->root);
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

    do {
        bst_at_node_t *current = load_node(bst_->root);
        bst_at_node_t **link = &bst_->root;

        while (current != NULL && current->value != value) {
            if (compare(value, current->value) < 0) {
                link = &current->left;
                bst_at_node_t *next = current->left;
                release_node(current, false);
                current = load_node(next);
            } else {
                link = &current->right;
                bst_at_node_t *next = current->right;
                release_node(current, false);
                current = load_node(next);
            }
        }

        if (current == NULL)
            return VALUE_NONEXISTENT;

        if (current->left == NULL || current->right == NULL) {
            bst_at_node_t *child =
                current->left ? current->left : current->right;
            if (atomic_compare_exchange_strong_explicit(link, &current, child,
                                                        memory_order_acq_rel,
                                                        memory_order_acquire)) {
                release_node(current, true);
                atomic_fetch_sub_explicit(&bst_->count, 1,
                                          memory_order_release);
                return SUCCESS;
            }
        } else {
            bst_at_node_t *successor = load_node(current->right);
            bst_at_node_t **successor_link = &current->right;

            while (successor->left != NULL) {
                successor_link = &successor->left;
                bst_at_node_t *next = successor->left;
                release_node(successor, false);
                successor = load_node(next);
            }

            current->value = successor->value;
            if (atomic_compare_exchange_strong_explicit(
                    successor_link, &successor, successor->right,
                    memory_order_acq_rel, memory_order_acquire)) {
                atomic_fetch_sub_explicit(&bst_->count, 1,
                                          memory_order_release);
                release_node(successor, true);
                release_node(current, false);
                return SUCCESS;
            }

            release_node(successor, false);
            release_node(current, false);
        }
    } while (1);
}

BST_ERROR bst_at_rebalance(bst_at_t **bst) { return 0; }

static void bst_at_node_free(bst_at_node_t *root) {
    if (root == NULL) {
        return;
    }

    bst_at_node_free(root->left);

    bst_at_node_free(root->right);

    free(root);
}

BST_ERROR bst_at_free(bst_at_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;
    *bst = NULL;

    atomic_init(&bst_->count, 0);
    bst_at_node_free(bst_->root);

    free(bst_);

    return SUCCESS;
}