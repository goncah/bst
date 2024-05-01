/*
Universidade Aberta
File: bst_mt_gmtx.h
Author: Hugo Gonçalves, 2100562

Multi-thread BST using global mutex

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
#ifndef __bst_mt_gmtx__
#define __bst_mt_gmtx__
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "bst_common.h"

typedef struct bst_mt_gmtx {
    pthread_mutex_t mtx;
    bst_node *root;
} bst_mt_gmtx;

static inline bst_mt_gmtx *bst_mt_gmtx_new() {
    bst_mt_gmtx *bst = (bst_mt_gmtx *)malloc(sizeof *bst);

    if (bst == NULL) {
        PANIC("malloc() failure\n");
    }

    if (pthread_mutex_init(&bst->mtx, NULL)) {
        PANIC("pthread_mutex_init() failure");
    }

    bst->root = NULL;
    return bst;
}

static inline long long int bst_mt_gmtx_add(bst_mt_gmtx *bst,
                                            long long int value) {
    if (bst == NULL) {
        return 1;
    }

    pthread_mutex_lock(&bst->mtx);
    if (bst->root == NULL) {
        bst->root = bst_node_new(value);
        pthread_mutex_unlock(&bst->mtx);
        return 0;
    }

    bst_node *root = bst->root;

    while (root != NULL) {
        if (value - root->value < 0) {
            if (root->left == NULL) {
                root->left = bst_node_new(value);
                pthread_mutex_unlock(&bst->mtx);
                return 0;
            }

            root = root->left;
        } else if (value - root->value > 0) {
            if (root->right == NULL) {
                root->right = bst_node_new(value);
                pthread_mutex_unlock(&bst->mtx);
                return 0;
            }

            root = root->right;
        } else {
            // Value already exists
            pthread_mutex_unlock(&bst->mtx);
            return 0;
        }
    }

    pthread_mutex_unlock(&bst->mtx);
    return 1;
}

static inline int bst_mt_gmtx_search(bst_mt_gmtx *bst, long long int value) {
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

static inline long long int bst_mt_gmtx_min(bst_mt_gmtx *bst) {
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

static inline long long int bst_mt_gmtx_max(bst_mt_gmtx *bst) {
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

static inline long long int bst_mt_gmtx_node_find_height(bst_node *root) {
    if (root == NULL) {
        return -1;
    }

    return 1 + max(bst_mt_gmtx_node_find_height(root->left),
                   bst_mt_gmtx_node_find_height(root->right));
}

static inline long long int bst_mt_gmtx_height(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return -1;
    }

    return bst_mt_gmtx_node_find_height(bst->root);
}

static inline void bst_mt_gmtx_node_traverse_preorder(bst_node *node) {
    if (node != NULL) {
        printf("%lld ", node->value);
        bst_mt_gmtx_node_traverse_preorder(node->left);
        bst_mt_gmtx_node_traverse_preorder(node->right);
    }
}

static inline int bst_mt_gmtx_traverse_preorder(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_gmtx_node_traverse_preorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_mt_gmtx_node_traverse_inorder(bst_node *node) {
    if (node != NULL) {
        bst_mt_gmtx_node_traverse_inorder(node->left);
        printf("%lld ", node->value);
        bst_mt_gmtx_node_traverse_inorder(node->right);
    }
}

static inline int bst_mt_gmtx_traverse_inorder(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_gmtx_node_traverse_inorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_mt_gmtx_node_traverse_postorder(bst_node *node) {
    if (node != NULL) {
        bst_mt_gmtx_node_traverse_postorder(node->left);
        bst_mt_gmtx_node_traverse_postorder(node->right);
        printf("%lld ", node->value);
    }
}

static inline int bst_mt_gmtx_traverse_postorder(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_gmtx_node_traverse_postorder(bst->root);
    printf("\n");

    return 0;
}

static inline bst_node *mt_min_node(bst_node *node) {
    bst_node *current = node;

    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

static inline bst_node *mt_delete_node(bst_node *root, long long int value) {
    if (root == NULL) {
        return root;
    }

    if (value < root->value) {
        root->left = mt_delete_node(root->left, value);
    } else if (value > root->value) {
        root->right = mt_delete_node(root->right, value);
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

        bst_node *temp = mt_min_node(root->right);

        root->value = temp->value;

        root->right = mt_delete_node(root->right, temp->value);
    }
    return root;
}

static inline int bst_mt_gmtx_delete(bst_mt_gmtx *bst, long long int value) {
    if (bst == NULL) {
        return 1;
    }

    pthread_mutex_lock(&bst->mtx);
    mt_delete_node(bst->root, value);
    pthread_mutex_unlock(&bst->mtx);

    return 0;
}

static inline void mt_save_inorder(bst_node *node, long long int *inorder,
                                   long long int *index) {
    if (node == NULL)
        return;
    mt_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    mt_save_inorder(node->right, inorder, index);
}

static inline bst_node *
mt_array_to_bst(long long int arr[], long long int start, long long int end) {
    if (start > end)
        return NULL;

    long long mid = (start + end) / 2;
    bst_node *node = bst_node_new(arr[mid]);

    node->left = mt_array_to_bst(arr, start, mid - 1);
    node->right = mt_array_to_bst(arr, mid + 1, end);

    return node;
}

static inline long long int mt_node_count(bst_node *root) {
    if (root == NULL) {
        return 0;
    } else {
        return mt_node_count(root->left) + mt_node_count(root->right) + 1;
    }
}

static inline long long int bst_mt_gmtx_node_count(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 0;
    }

    return mt_node_count(bst->root);
}

static inline long long int bst_mt_gmtx_width(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return -1;
    }

    long long int w = 0;
    bst_node **q = malloc(sizeof *q * mt_node_count(bst->root));
    long long int f = 0, r = 0;

    q[r++] = bst->root;

    while (f < r) {
        int count = r - f;
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

static inline bst_mt_gmtx *bst_mt_gmtx_rebalance(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return NULL;
    }

    if (bst->root == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&bst->mtx);

    long long int *inorder =
        malloc(sizeof(long long int) * mt_node_count(bst->root));
    long long int index = 0;
    mt_save_inorder(bst->root, inorder, &index);

    bst_node_free(bst->root);

    bst->root = mt_array_to_bst(inorder, 0, index - 1);

    free(inorder);

    pthread_mutex_unlock(&bst->mtx);
    return bst;
}

static inline void bst_mt_gmtx_print_details(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return;
    }
    printf("%lld,", mt_node_count(bst->root));
    printf("%lld,", bst_mt_gmtx_min(bst));
    printf("%lld,", bst_mt_gmtx_max(bst));
    printf("%lld,", bst_mt_gmtx_height(bst));
    printf("%lld,", bst_mt_gmtx_width(bst));
}

static inline void bst_mt_gmtx_free(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return;
    }

    bst_node_free(bst->root);
    free(bst);
}

#endif