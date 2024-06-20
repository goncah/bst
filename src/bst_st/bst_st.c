/*
Universidade Aberta
File: bst_st_t.c
Author: Hugo Gonçalves, 2100562

Single-thread MT Unsafe BST

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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/bst_common.h"
#include "include/bst_st.h"

bst_st_node_t *bst_st_node_new(const int64_t value, BST_ERROR *err) {
    bst_st_node_t *node = malloc(sizeof(bst_st_node_t));

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

bst_st_t *bst_st_new(BST_ERROR *err) {
    bst_st_t *bst = malloc(sizeof(bst_st_t));

    if (bst == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
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

BST_ERROR bst_st_add(bst_st_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_st_t *bst_ = *bst;

    if (bst_->root == NULL) {
        BST_ERROR err;
        bst_st_node_t *node = bst_st_node_new(value, &err);

        if (IS_SUCCESS(err)) {
            bst_->root = node;
        } else {
            return err;
        }

        bst_->count++;
        return SUCCESS;
    }

    bst_st_node_t *root = bst_->root;

    while (root != NULL) {
        if (compare(value, root->value) < 0) {
            if (root->left == NULL) {
                BST_ERROR err;
                bst_st_node_t *node = bst_st_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->left = node;
                } else {
                    return err;
                }

                bst_->count++;
                return SUCCESS;
            }

            root = root->left;
        } else if (compare(value, root->value) > 0) {
            if (root->right == NULL) {
                BST_ERROR err;
                bst_st_node_t *node = bst_st_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->right = node;
                } else {
                    return err;
                }

                bst_->count++;
                return SUCCESS;
            }

            root = root->right;
        } else {
            return VALUE_EXISTS;
        }
    }

    return UNKNOWN; // Should never get here
}

BST_ERROR bst_st_search(bst_st_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    const bst_st_t *bst_ = *bst;

    const bst_st_node_t *root = bst_->root;

    while (root != NULL) {
        if (root->value == value) {
            return VALUE_EXISTS;
        } else if (compare(value, root->value) < 0) {
            root = root->left;
        } else if (compare(value, root->value) > 0) {
            root = root->right;
        }
    }

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_st_min(bst_st_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    const bst_st_t *bst_ = *bst;

    if (bst_->root == NULL) {
        return BST_EMPTY;
    }

    const bst_st_node_t *root = bst_->root;

    while (root->left != NULL) {
        root = root->left;
    }

    if (value != NULL) {
        *value = root->value;
    }

    return SUCCESS;
}

BST_ERROR bst_st_max(bst_st_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    const bst_st_t *bst_ = *bst;

    if (bst_->root == NULL) {
        return BST_EMPTY;
    }

    const bst_st_node_t *root = bst_->root;

    while (root->right != NULL) {
        root = root->right;
    }

    if (value != NULL) {
        *value = root->value;
    }

    return SUCCESS;
}

BST_ERROR bst_st_delete(bst_st_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_st_t *bst_ = *bst;

    if (bst_->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_t *current = bst_->root, *parent = NULL;

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
        return VALUE_NONEXISTENT;
    }

    // Node with two children
    if (current->left != NULL && current->right != NULL) {
        bst_st_node_t *successor = current->right;
        bst_st_node_t *successor_parent = current;

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
    bst_st_node_t *child =
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
    return SUCCESS;
}

static void bst_node_free(bst_st_node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_node_free(root->right);
    }

    free(root);
}

BST_ERROR bst_st_free(bst_st_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_st_t *bst_ = *bst;

    *bst = NULL;

    bst_node_free(bst_->root);

    free(bst_);

    return SUCCESS;
}