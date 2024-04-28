/*
Universidade Aberta
File: bst_st.h
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
#ifndef __bst_st__
#define __bst_st__
#include <stdio.h>
#include <stdlib.h>

#include "bst_common.h"

typedef struct bst_st {
    long long int count;
    bst_node *root;
} bst_st;

static inline bst_st *bst_st_new() {
    bst_st *bst = (bst_st *)malloc(sizeof *bst);

    if (bst == NULL) {
        PANIC("malloc() failure\n");
    }

    bst->count = 0;
    bst->root = NULL;

    return bst;
}

static inline int bst_st_add(bst_st *bst, long long int value) {
    if (bst == NULL) {
        return 1;
    }

    if (bst->root == NULL) {
        bst->count++;
        bst->root = bst_node_new(value);
        return 0;
    }

    bst_node *root = bst->root;
    bst->count++;
    while (root != NULL) {
        if (value - root->value < 0) {
            if (root->left == NULL) {
                root->left = bst_node_new(value);
                return 0;
            }

            root = root->left;
        } else if (value - root->value > 0) {
            if (root->right == NULL) {
                root->right = bst_node_new(value);
                return 0;
            }

            root = root->right;
        } else {
            // Value already exists
            return 0;
        }
    }

    return 1;
}

static inline int bst_st_search(bst_st *bst, long long int value) {
    if (bst == NULL) {
        return 1;
    }

    bst_node *root = bst->root;

    while (root != NULL) {
        if (root->value == value) {
            return 0;
        } else if (value - root->value < 0) {
            root = root->left;
        } else if (value - root->value > 0) {
            root = root->right;
        }
    }

    return 1;
}

static inline long long int bst_st_min(bst_st *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_node *root = bst->root;

    if (root == NULL) {
        return 0;
    }

    while (root->left != NULL) {
        root = root->left;
    }

    return root->value;
}

static inline long long int bst_st_max(bst_st *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_node *root = bst->root;

    if (root == NULL) {
        return 0;
    }

    while (root->right != NULL) {
        root = root->right;
    }

    return root->value;
}

static inline long long int bst_st_node_find_height(bst_node *root) {
    if (root == NULL) {
        return -1;
    }

    return 1 + max(bst_st_node_find_height(root->left),
                   bst_st_node_find_height(root->right));
}

static inline long long int bst_st_height(bst_st *bst) {
    if (bst == NULL) {
        return -1;
    }

    return bst_st_node_find_height(bst->root);
}

static inline long long int bst_st_width(bst_st *bst) {
    if (bst == NULL) {
        return -1;
    }

    long long int w = 0;
    bst_node **q = malloc(sizeof *q * bst->count);
    long long int f = 0, r = 0;

    q[r++] = bst->root;

    while (f < r) {
        long long int count = r - f;
        w = w > count ? w : count;

        for (int i = 0; i < count; i++) {
            bst_node *n = q[f++];
            if (n->left != NULL) {
                q[r++] = n->left;
            }

            if (n->right != NULL) {
                q[r++] = n->right;
            }
        }
    }

    free(q);
    return w;
}

static inline void bst_st_node_traverse_preorder(bst_node *node) {
    if (node != NULL) {
        printf("%lld ", node->value);
        bst_st_node_traverse_preorder(node->left);
        bst_st_node_traverse_preorder(node->right);
    }
}

static inline int bst_st_traverse_preorder(bst_st *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_st_node_traverse_preorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_st_node_traverse_inorder(bst_node *node) {
    if (node != NULL) {
        bst_st_node_traverse_inorder(node->left);
        printf("%lld ", node->value);
        bst_st_node_traverse_inorder(node->right);
    }
}

static inline int bst_st_traverse_inorder(bst_st *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_st_node_traverse_inorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_st_node_traverse_postorder(bst_node *node) {
    if (node != NULL) {
        bst_st_node_traverse_postorder(node->left);
        bst_st_node_traverse_postorder(node->right);
        printf("%lld ", node->value);
    }
}

static inline int bst_st_traverse_postorder(bst_st *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_st_node_traverse_postorder(bst->root);
    printf("\n");

    return 0;
}

static inline bst_node *st_min_node(bst_node *node) {
    bst_node *current = node;

    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

static inline bst_node *st_delete_node(bst_node *root, long long int value) {
    if (root == NULL) {
        return root;
    }

    if (value < root->value) {
        root->left = st_delete_node(root->left, value);
    } else if (value > root->value) {
        root->right = st_delete_node(root->right, value);
    } else {
        if (root->left == NULL) {
            bst_node *temp = root->right;
            free(root);
            return temp;
        } else if (root->right == NULL) {
            bst_node *temp = root->left;
            free(root);
            return temp;
        }

        bst_node *temp = st_min_node(root->right);

        root->value = temp->value;

        root->right = st_delete_node(root->right, temp->value);
    }
    return root;
}

static inline int bst_st_delete(bst_st *bst, long long int value) {
    if (bst == NULL) {
        return 1;
    }

    st_delete_node(bst->root, value);
    bst->count--;

    return 0;
}

static inline void st_save_inorder(bst_node *node, long long int *inorder,
                                   long long int *index) {
    if (node == NULL)
        return;
    st_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    st_save_inorder(node->right, inorder, index);
}

static inline bst_node *
st_array_to_bst(long long int arr[], long long int start, long long int end) {
    if (start > end)
        return NULL;

    long long int mid = (start + end) / 2;
    bst_node *node = bst_node_new(arr[mid]);

    node->left = st_array_to_bst(arr, start, mid - 1);
    node->right = st_array_to_bst(arr, mid + 1, end);

    return node;
}

static inline long long int st_node_count(bst_node *root) {
    if (root == NULL) {
        return 0;
    } else {
        return st_node_count(root->left) + st_node_count(root->right) + 1;
    }
}

static inline int bst_st_node_count(bst_st *bst) {
    if (bst == NULL) {
        return 0;
    }

    return st_node_count(bst->root);
}

static inline bst_st *bst_st_rebalance(bst_st *bst) {
    if (bst == NULL) {
        return NULL;
    }

    if (bst->root == NULL) {
        return NULL;
    }

    long long int *inorder = malloc(sizeof(long long int) * bst->count);
    long long int index = 0;
    st_save_inorder(bst->root, inorder, &index);

    bst_node_free(bst->root);

    bst->root = st_array_to_bst(inorder, 0, index - 1);

    free(inorder);
    return bst;
}

static inline void bst_st_print_details(bst_st *bst) {
    if (bst == NULL) {
        return;
    }
    printf("%lld,", bst->count);
    printf("%lld,", bst_st_min(bst));
    printf("%lld,", bst_st_max(bst));
    printf("%lld,", bst_st_height(bst));
    printf("%lld,", bst_st_width(bst));
}

#endif