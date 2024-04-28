/*
Universidade Aberta
File: bst_mt_lmtx.h
Author: Hugo Gonçalves, 2100562

Multi-thread BST using local mutex

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
#ifndef __bst_mt_lmtx__
#define __bst_mt_lmtx__
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "bst_common.h"

typedef struct bst_mt_node {
    int value;
    struct bst_mt_node *left;
    struct bst_mt_node *right;
    pthread_mutex_t mtx;
} bst_mt_node;

typedef struct bst_mt_lmtx {
    bst_mt_node *root;
    pthread_mutex_t mtx;
} bst_mt_lmtx;

static inline bst_mt_node *bst_mt_node_new(int value) {
    bst_mt_node *node = (bst_mt_node *)malloc(sizeof *node);

    if (node == NULL) {
        PANIC("malloc() failure\n");
    }

    if (pthread_mutex_init(&node->mtx, NULL)) {
        PANIC("pthread_mutex_init() failure");
    }

    node->value = value;
    node->left = NULL;
    node->right = NULL;

    return node;
}

static inline void bst_mt_node_free(bst_mt_node *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_mt_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_mt_node_free(root->right);
    }

    free(root);
}

static inline bst_mt_lmtx *bst_mt_lmtx_new() {
    bst_mt_lmtx *bst = (bst_mt_lmtx *)malloc(sizeof *bst);

    if (bst == NULL) {
        PANIC("malloc() failure\n");
    }

    if (pthread_mutex_init(&bst->mtx, NULL)) {
        PANIC("pthread_mutex_init() failure");
    }

    bst->root = NULL;

    return bst;
}

static inline int bst_mt_lmtx_add(bst_mt_lmtx *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    if (bst->root == NULL) {
        pthread_mutex_lock(&bst->mtx);
        if (bst->root == NULL) {
            bst->root = bst_mt_node_new(value);
            pthread_mutex_unlock(&bst->mtx);
            return 0;
        }
        pthread_mutex_unlock(&bst->mtx);
    }

    bst_mt_node *root = bst->root, *tmp;
    while (root != NULL) {
        if (value - root->value < 0) {
            pthread_mutex_lock(&root->mtx);
            if (value - root->value < 0) {
                if (root->left == NULL) {
                    root->left = bst_mt_node_new(value);
                    pthread_mutex_unlock(&root->mtx);
                    return 0;
                }

                tmp = root;
                root = root->left;
                pthread_mutex_unlock(&tmp->mtx);
                continue;
            } else if (value - root->value > 0) {
                if (root->right == NULL) {
                    root->right = bst_mt_node_new(value);
                    pthread_mutex_unlock(&root->mtx);
                    return 0;
                }

                tmp = root;
                root = root->right;
                pthread_mutex_unlock(&tmp->mtx);
                continue;
            } else {
                // Value already exists
                pthread_mutex_unlock(&root->mtx);
                return 0;
            }
        } else if (value - root->value > 0) {
            pthread_mutex_lock(&root->mtx);
            if (value - root->value > 0) {
                if (root->right == NULL) {
                    root->right = bst_mt_node_new(value);
                    pthread_mutex_unlock(&root->mtx);
                    return 0;
                }

                tmp = root;
                root = root->right;
                pthread_mutex_unlock(&tmp->mtx);
                continue;
            }
            // Value already exists
            pthread_mutex_unlock(&root->mtx);
            return 0;
        } else {
            // Value already exists
            return 0;
        }
    }

    return 1;
}

static inline int bst_mt_lmtx_search(bst_mt_lmtx *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_node *root = bst->root;

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

static inline int bst_mt_lmtx_min(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_node *root = bst->root;

    if (root == NULL) {
        return 0;
    }

    while (root->left != NULL) {
        root = root->left;
    }

    return root->value;
}

static inline int bst_mt_lmtx_max(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_node *root = bst->root;

    if (root == NULL) {
        return 0;
    }

    while (root->right != NULL) {
        root = root->right;
    }

    return root->value;
}

static inline int bst_mt_lmtx_node_find_height(bst_mt_node *root) {
    if (root == NULL) {
        return -1;
    }

    return 1 + max(bst_mt_lmtx_node_find_height(root->left),
                   bst_mt_lmtx_node_find_height(root->right));
}

static inline int bst_mt_lmtx_height(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return -1;
    }

    return bst_mt_lmtx_node_find_height(bst->root);
}

static inline void bst_mt_lmtx_node_traverse_preorder(bst_mt_node *node) {
    if (node != NULL) {
        printf("%d ", node->value);
        bst_mt_lmtx_node_traverse_preorder(node->left);
        bst_mt_lmtx_node_traverse_preorder(node->right);
    }
}

static inline int bst_mt_lmtx_traverse_preorder(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_lmtx_node_traverse_preorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_mt_lmtx_node_traverse_inorder(bst_mt_node *node) {
    if (node != NULL) {
        bst_mt_lmtx_node_traverse_inorder(node->left);
        printf("%d ", node->value);
        bst_mt_lmtx_node_traverse_inorder(node->right);
    }
}

static inline int bst_mt_lmtx_traverse_inorder(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_lmtx_node_traverse_inorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_mt_lmtx_node_traverse_postorder(bst_mt_node *node) {
    if (node != NULL) {
        bst_mt_lmtx_node_traverse_postorder(node->left);
        bst_mt_lmtx_node_traverse_postorder(node->right);
        printf("%d ", node->value);
    }
}

static inline int bst_mt_lmtx_traverse_postorder(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_lmtx_node_traverse_postorder(bst->root);
    printf("\n");

    return 0;
}

static inline bst_mt_node *lmt_min_node(bst_mt_node *node) {
    bst_mt_node *current = node;

    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

static inline bst_mt_node *lmt_delete_node(bst_mt_node *root, int value) {
    if (root == NULL) {
        return root;
    }

    if (value < root->value) {
        root->left = lmt_delete_node(root->left, value);
    } else if (value > root->value) {
        root->right = lmt_delete_node(root->right, value);
    } else {
        if (root->left == NULL) {
            bst_mt_node *temp = root->right;
            free(root);
            return temp;
        } else if (root->right == NULL) {
            bst_mt_node *temp = root->left;
            free(root);
            return temp;
        }

        bst_mt_node *temp = lmt_min_node(root->right);

        root->value = temp->value;

        root->right = lmt_delete_node(root->right, temp->value);
    }
    return root;
}

static inline int bst_mt_lmtx_delete(bst_mt_lmtx *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    lmt_delete_node(bst->root, value);

    return 0;
}

static inline void lmt_save_inorder(bst_mt_node *node, int *inorder,
                                    int *index) {
    if (node == NULL)
        return;
    lmt_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    lmt_save_inorder(node->right, inorder, index);
}

static inline bst_mt_node *lmt_array_to_bst(int arr[], int start, int end) {
    if (start > end)
        return NULL;

    int mid = (start + end) / 2;
    bst_mt_node *node = bst_mt_node_new(arr[mid]);

    node->left = lmt_array_to_bst(arr, start, mid - 1);
    node->right = lmt_array_to_bst(arr, mid + 1, end);

    return node;
}

static inline int lmt_node_count(bst_mt_node *root) {
    if (root == NULL) {
        return 0;
    } else {
        return lmt_node_count(root->left) + lmt_node_count(root->right) + 1;
    }
}

static inline int bst_mt_lmtx_width(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return -1;
    }

    int w = 0;
    bst_mt_node **q = malloc(sizeof *q * lmt_node_count(bst->root));
    int f = 0, r = 0;

    q[r++] = bst->root;

    while (f < r) {
        int count = r - f;
        w = w > count ? w : count;

        for (int i = 0; i < count; i++) {
            bst_mt_node *n = q[f++];
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

static inline int bst_mt_lmtx_node_count(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return 0;
    }

    return lmt_node_count(bst->root);
}

static inline bst_mt_lmtx *bst_mt_lmtx_rebalance(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return NULL;
    }

    if (bst->root == NULL) {
        return NULL;
    }

    int *inorder = malloc(sizeof(int) * lmt_node_count(bst->root));
    int index = 0;
    lmt_save_inorder(bst->root, inorder, &index);

    bst_mt_node_free(bst->root);

    bst->root = lmt_array_to_bst(inorder, 0, index - 1);

    free(inorder);
    return bst;
}

static inline void bst_mt_lmtx_print_details(bst_mt_lmtx *bst) {
    if (bst == NULL) {
        return;
    }
    printf("%d,", lmt_node_count(bst->root));
    printf("%d,", bst_mt_lmtx_min(bst));
    printf("%d,", bst_mt_lmtx_max(bst));
    printf("%d,", bst_mt_lmtx_height(bst));
    printf("%d,", bst_mt_lmtx_width(bst));
}

#endif