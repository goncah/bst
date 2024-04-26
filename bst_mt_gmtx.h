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
    long count;
    int rebalance;
    int cr;
    bst_node *root;
} bst_mt_gmtx;

void bst_mt_gmtx_check_rebalance(bst_mt_gmtx *bst);

bst_mt_gmtx *bst_mt_gmtx_new() {
    bst_mt_gmtx *bst = (bst_mt_gmtx *)malloc(sizeof *bst);

    if (bst == NULL) {
        PANIC("malloc() failure\n");
    }

    if (pthread_mutex_init(&bst->mtx, NULL)) {
        PANIC("pthread_mutex_init() failure\n");
    }

    bst->count = 0;
    bst->root = NULL;

    return bst;
}

int bst_mt_gmtx_add(bst_mt_gmtx *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    pthread_mutex_lock(&bst->mtx);
    if (bst->root == NULL) {
        bst->count++;
        bst->root = bst_node_new(value);
        pthread_mutex_unlock(&bst->mtx);
        bst_mt_gmtx_check_rebalance(bst);
        return 0;
    }

    bst_node *root = bst->root;
    bst->count++;
    while (root != NULL) {
        if (value - root->value < 0) {
            if (root->left == NULL) {
                root->left = bst_node_new(value);
                pthread_mutex_unlock(&bst->mtx);
                bst_mt_gmtx_check_rebalance(bst);
                return 0;
            }

            root = root->left;
        } else if (value - root->value > 0) {
            if (root->right == NULL) {
                root->right = bst_node_new(value);
                pthread_mutex_unlock(&bst->mtx);
                bst_mt_gmtx_check_rebalance(bst);
                return 0;
            }

            root = root->right;
        } else {
            pthread_mutex_unlock(&bst->mtx);
            // Value already exists
            return 0;
        }
    }
    pthread_mutex_unlock(&bst->mtx);
    return 1;
}

int bst_mt_gmtx_search(bst_mt_gmtx *bst, int value) {
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

int bst_mt_gmtx_min(bst_mt_gmtx *bst) {
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

int bst_mt_gmtx_max(bst_mt_gmtx *bst) {
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

int bst_mt_gmtx_node_find_height(bst_node *root) {
    if (root == NULL) {
        return -1;
    }

    return 1 + max(bst_mt_gmtx_node_find_height(root->left),
                   bst_mt_gmtx_node_find_height(root->right));
}

int bst_mt_gmtx_height(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return -1;
    }

    return bst_mt_gmtx_node_find_height(bst->root);
}

int bst_mt_gmtx_width(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return -1;
    }

    int w = 0;
    bst_node *q[bst->count];
    int f = 0, r = 0;

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

    return w;
}

void bst_mt_gmtx_node_traverse_preorder(bst_node *node) {
    if (node != NULL) {
        printf("%d ", node->value);
        bst_mt_gmtx_node_traverse_preorder(node->left);
        bst_mt_gmtx_node_traverse_preorder(node->right);
    }
}

int bst_mt_gmtx_traverse_preorder(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_gmtx_node_traverse_preorder(bst->root);
    printf("\n");

    return 0;
}

void bst_mt_gmtx_node_traverse_inorder(bst_node *node) {
    if (node != NULL) {
        bst_mt_gmtx_node_traverse_inorder(node->left);
        printf("%d ", node->value);
        bst_mt_gmtx_node_traverse_inorder(node->right);
    }
}

int bst_mt_gmtx_traverse_inorder(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_gmtx_node_traverse_inorder(bst->root);
    printf("\n");

    return 0;
}

void bst_mt_gmtx_node_traverse_postorder(bst_node *node) {
    if (node != NULL) {
        bst_mt_gmtx_node_traverse_postorder(node->left);
        bst_mt_gmtx_node_traverse_postorder(node->right);
        printf("%d ", node->value);
    }
}

int bst_mt_gmtx_traverse_postorder(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_gmtx_node_traverse_postorder(bst->root);
    printf("\n");

    return 0;
}

bst_node *min_node(bst_node *node) {
    bst_node *current = node;

    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

bst_node *delete_node(bst_node *root, int value) {
    if (root == NULL) {
        return root;
    }

    if (value < root->value) {
        root->left = delete_node(root->left, value);
    } else if (value > root->value) {
        root->right = delete_node(root->right, value);
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

        bst_node *temp = min_node(root->right);

        root->value = temp->value;

        root->right = delete_node(root->right, temp->value);
    }

    return root;
}

int bst_mt_gmtx_delete(bst_mt_gmtx *bst, int value) {
    if (bst == NULL) {
        return 1;
    }
    pthread_mutex_lock(&bst->mtx);
    delete_node(bst->root, value);
    pthread_mutex_unlock(&bst->mtx);
    bst->count--;

    return 0;
}

void save_inorder(bst_node *node, int *inorder, int *index) {
    if (node == NULL)
        return;
    save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    save_inorder(node->right, inorder, index);
}

bst_node *array_to_bst(int arr[], int start, int end) {
    if (start > end)
        return NULL;

    int mid = (start + end) / 2;
    bst_node *node = bst_node_new(arr[mid]);

    node->left = array_to_bst(arr, start, mid - 1);
    node->right = array_to_bst(arr, mid + 1, end);

    return node;
}

int node_count(bst_node *root) {
    if (root == NULL) {
        return 0;
    } else {
        return node_count(root->left) + node_count(root->right) + 1;
    }
}

int bst_mt_gmtx_node_count(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return 0;
    }

    return node_count(bst->root);
}

bst_mt_gmtx *bst_mt_gmtx_rebalance(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return NULL;
    }

    if (bst->root == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&bst->mtx);
    int *inorder = malloc(sizeof(int) * bst->count);
    int index = 0;
    save_inorder(bst->root, inorder, &index);

    bst_node_free(bst->root);

    bst->root = array_to_bst(inorder, 0, index - 1);

    free(inorder);
    pthread_mutex_unlock(&bst->mtx);
    return bst;
}

void bst_mt_gmtx_check_rebalance(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return;
    }

    if (bst->cr >= bst->rebalance) {
        bst_mt_gmtx_rebalance(bst);
        bst->cr = 0;
    }

    if (bst->count >= bst->rebalance * 10) {
        bst->rebalance = bst->rebalance * 10;
    }
}

void bst_mt_gmtx_print_details(bst_mt_gmtx *bst) {
    if (bst == NULL) {
        return;
    }

    printf("\nNodes: %ld\n", bst->count);
    printf("Min: %d\n", bst_mt_gmtx_min(bst));
    printf("Max: %d\n", bst_mt_gmtx_max(bst));
    printf("Height: %d\n", bst_mt_gmtx_height(bst));
    printf("Width: %d\n", bst_mt_gmtx_width(bst));
}

#endif