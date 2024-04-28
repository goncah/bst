/*
Universidade Aberta
File: bst_mt_cas.h
Author: Hugo Gonçalves, 2100562

Multi-thread BST using atomic operations (CAS)

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
#ifndef __bst_mt_cas__
#define __bst_mt_cas__
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "bst_common.h"

typedef struct bst_cas_node {
    int value;
    _Atomic(struct bst_cas_node *) left;
    _Atomic(struct bst_cas_node *) right;
} bst_cas_node;

typedef struct bst_mt_cas {
    atomic_long count;
    _Atomic(bst_cas_node *) root;
} bst_mt_cas;

static inline bst_cas_node *bst_cas_node_new(int value) {
    bst_cas_node *node = (bst_cas_node *)malloc(sizeof *node);

    if (node == NULL) {
        PANIC("malloc() failure\n");
    }

    node->value = value;
    atomic_store(&node->left, NULL);
    atomic_store(&node->right, NULL);

    return node;
}

static inline void bst_cas_node_free(bst_cas_node *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_cas_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_cas_node_free(root->right);
    }

    free(root);
}

static inline bst_mt_cas *bst_mt_cas_new() {
    bst_mt_cas *bst = (bst_mt_cas *)malloc(sizeof *bst);

    if (bst == NULL) {
        PANIC("malloc() failure\n");
    }

    atomic_init(&bst->count, 0);
    atomic_store(&bst->root, NULL);

    return bst;
}

static inline int bst_mt_cas_add(bst_mt_cas *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    _Atomic(bst_cas_node *) *nodeptr;
    bst_cas_node *node;

    while (1) {
        nodeptr = &bst->root;
        node = atomic_load(nodeptr);

        if (node == NULL) {
            bst_cas_node *new_node = bst_cas_node_new(value);
            if (atomic_compare_exchange_weak(nodeptr, &node, new_node)) {
                atomic_fetch_add(&bst->count, 1);
                return 0; // Successfully added root
            }
            free(new_node); // CAS failed, another thread added a node, so retry
        } else {
            // Start traversing the tree
            while (1) {
                if (value < node->value) {
                    nodeptr = &node->left;
                } else if (value > node->value) {
                    nodeptr = &node->right;
                } else {
                    return 0; // Value already exists, no need to insert
                }

                bst_cas_node *next = atomic_load(nodeptr);
                if (next == NULL) {
                    bst_cas_node *new_node = bst_cas_node_new(value);
                    if (atomic_compare_exchange_weak(nodeptr, &next,
                                                     new_node)) {
                        atomic_fetch_add(&bst->count, 1);
                        return 0; // Success
                    }
                    free(new_node); // CAS failed, need to retry at this level
                } else {
                    node = next; // Move to next node
                }
            }
        }
    }
}

static inline int bst_mt_cas_search(bst_mt_cas *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    bst_cas_node *root = bst->root;

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

static inline int bst_mt_cas_min(bst_mt_cas *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_cas_node *root = bst->root;

    if (root == NULL) {
        return 0;
    }

    while (root->left != NULL) {
        root = root->left;
    }

    return root->value;
}

static inline int bst_mt_cas_max(bst_mt_cas *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_cas_node *root = bst->root;

    if (root == NULL) {
        return 0;
    }

    while (root->right != NULL) {
        root = root->right;
    }

    return root->value;
}

static inline int bst_mt_cas_node_find_height(bst_cas_node *root) {
    if (root == NULL) {
        return -1;
    }

    return 1 + max(bst_mt_cas_node_find_height(root->left),
                   bst_mt_cas_node_find_height(root->right));
}

static inline int bst_mt_cas_height(bst_mt_cas *bst) {
    if (bst == NULL) {
        return -1;
    }

    return bst_mt_cas_node_find_height(bst->root);
}

static inline int bst_mt_cas_width(bst_mt_cas *bst) {
    if (bst == NULL) {
        return -1;
    }

    int w = 0;
    bst_cas_node **q = malloc(sizeof *q * bst->count);
    int f = 0, r = 0;

    q[r++] = bst->root;

    while (f < r) {
        int count = r - f;
        w = w > count ? w : count;

        for (int i = 0; i < count; i++) {
            bst_cas_node *n = q[f++];
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

static inline void bst_mt_cas_node_traverse_preorder(bst_cas_node *node) {
    if (node != NULL) {
        printf("%d ", node->value);
        bst_mt_cas_node_traverse_preorder(node->left);
        bst_mt_cas_node_traverse_preorder(node->right);
    }
}

static inline int bst_mt_cas_traverse_preorder(bst_mt_cas *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_cas_node_traverse_preorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_mt_cas_node_traverse_inorder(bst_cas_node *node) {
    if (node != NULL) {
        bst_mt_cas_node_traverse_inorder(node->left);
        printf("%d ", node->value);
        bst_mt_cas_node_traverse_inorder(node->right);
    }
}

static inline int bst_mt_cas_traverse_inorder(bst_mt_cas *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_cas_node_traverse_inorder(bst->root);
    printf("\n");

    return 0;
}

static inline void bst_mt_cas_node_traverse_postorder(bst_cas_node *node) {
    if (node != NULL) {
        bst_mt_cas_node_traverse_postorder(node->left);
        bst_mt_cas_node_traverse_postorder(node->right);
        printf("%d ", node->value);
    }
}

static inline int bst_mt_cas_traverse_postorder(bst_mt_cas *bst) {
    if (bst == NULL) {
        return 1;
    }

    bst_mt_cas_node_traverse_postorder(bst->root);
    printf("\n");

    return 0;
}

static inline bst_cas_node *cas_min_node(bst_cas_node *node) {
    bst_cas_node *current = node;

    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

static inline bst_cas_node *cas_delete_node(bst_cas_node *root, int value) {
    if (root == NULL) {
        return root;
    }

    if (value < root->value) {
        root->left = cas_delete_node(root->left, value);
    } else if (value > root->value) {
        root->right = cas_delete_node(root->right, value);
    } else {
        if (root->left == NULL) {
            bst_cas_node *temp = root->right;
            free(root);
            return temp;
        } else if (root->right == NULL) {
            bst_cas_node *temp = root->left;
            free(root);
            return temp;
        }

        bst_cas_node *temp = cas_min_node(root->right);

        root->value = temp->value;

        root->right = cas_delete_node(root->right, temp->value);
    }
    return root;
}

static inline int bst_mt_cas_delete(bst_mt_cas *bst, int value) {
    if (bst == NULL) {
        return 1;
    }

    cas_delete_node(bst->root, value);
    bst->count--;

    return 0;
}

static inline void cas_save_inorder(bst_cas_node *node, int *inorder,
                                    int *index) {
    if (node == NULL)
        return;
    cas_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    cas_save_inorder(node->right, inorder, index);
}

static inline bst_cas_node *cas_array_to_bst(int arr[], int start, int end) {
    if (start > end)
        return NULL;

    int mid = (start + end) / 2;
    bst_cas_node *node = bst_cas_node_new(arr[mid]);

    node->left = cas_array_to_bst(arr, start, mid - 1);
    node->right = cas_array_to_bst(arr, mid + 1, end);

    return node;
}

static inline int cas_node_count(bst_cas_node *root) {
    if (root == NULL) {
        return 0;
    } else {
        return cas_node_count(root->left) + cas_node_count(root->right) + 1;
    }
}

static inline int bst_mt_cas_node_count(bst_mt_cas *bst) {
    if (bst == NULL) {
        return 0;
    }

    return cas_node_count(bst->root);
}

static inline bst_mt_cas *bst_mt_cas_rebalance(bst_mt_cas *bst) {
    if (bst == NULL) {
        return NULL;
    }

    if (bst->root == NULL) {
        return NULL;
    }

    int *inorder = malloc(sizeof(int) * bst->count);
    int index = 0;
    cas_save_inorder(bst->root, inorder, &index);

    bst_cas_node_free(bst->root);

    bst->root = cas_array_to_bst(inorder, 0, index - 1);

    free(inorder);
    return bst;
}

static inline void bst_mt_cas_print_details(bst_mt_cas *bst) {
    if (bst == NULL) {
        return;
    }
    printf("%ld,", bst->count);
    printf("%d,", bst_mt_cas_min(bst));
    printf("%d,", bst_mt_cas_max(bst));
    printf("%d,", bst_mt_cas_height(bst));
    printf("%d,", bst_mt_cas_width(bst));
}

#endif