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

#include "bst_common.h"
#include "bst_st.h"

bst_st_node_t *bst_st_node_new(int64_t value, BST_ERROR *err) {
    bst_st_node_t *node = (bst_st_node_t *)malloc(sizeof *node);

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
    bst_st_t *bst = (bst_st_t *)malloc(sizeof *bst);

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

BST_ERROR bst_st_add(bst_st_t *bst, int64_t value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        BST_ERROR err;
        bst_st_node_t *node = bst_st_node_new(value, &err);

        if (IS_SUCCESS(err)) {
            bst->root = node;
        } else {
            return err;
        }

        bst->count++;
        return SUCCESS;
    }

    bst_st_node_t *root = bst->root;

    while (root != NULL) {
        if (value - root->value < 0) {
            if (root->left == NULL) {
                BST_ERROR err;
                bst_st_node_t *node = bst_st_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->left = node;
                } else {
                    return err;
                }

                bst->count++;
                return SUCCESS;
            }

            root = root->left;
        } else if (value - root->value > 0) {
            if (root->right == NULL) {
                BST_ERROR err;
                bst_st_node_t *node = bst_st_node_new(value, &err);

                if (IS_SUCCESS(err)) {
                    root->right = node;
                } else {
                    return err;
                }

                bst->count++;
                return SUCCESS;
            }

            root = root->right;
        } else {
            return VALUE_EXISTS;
        }
    }

    return UNKNOWN; // Should never get here
}

BST_ERROR bst_st_search(bst_st_t *bst, int64_t value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    bst_st_node_t *root = bst->root;

    while (root != NULL) {
        if (root->value == value) {
            return VALUE_EXISTS;
        } else if (value - root->value < 0) {
            root = root->left;
        } else if (value - root->value > 0) {
            root = root->right;
        }
    }

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_st_min(bst_st_t *bst, int64_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_t *root = bst->root;

    while (root->left != NULL) {
        root = root->left;
    }

    if (value != NULL) {
        *value = root->value;
    }

    return SUCCESS;
}

BST_ERROR bst_st_max(bst_st_t *bst, int64_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_t *root = bst->root;

    while (root->right != NULL) {
        root = root->right;
    }

    if (value != NULL) {
        *value = root->value;
    }

    return SUCCESS;
}

BST_ERROR bst_st_height(bst_st_t *bst, size_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    // Stack for tree nodes to avoid recursion and allow easier concurrency
    // control
    struct Stack {
        bst_st_node_t *node;
        size_t depth;
    };

    struct Stack *stack = malloc(sizeof *stack * bst->count);

    if (stack == NULL) {

        return MALLOC_FAILURE;
    }

    size_t stack_size = 0;

    // Initial push of the root node with depth 0
    stack[stack_size++] = (struct Stack){bst->root, 0};

    size_t max_depth = 0;

    while (stack_size > 0) {
        // Pop the top element from stack
        struct Stack top = stack[--stack_size];
        bst_st_node_t *current = top.node;
        size_t current_depth = top.depth;

        // Update maximum depth found
        if (current_depth >= max_depth) {
            max_depth = current_depth;
        }

        // Push children to the stack with incremented depth
        if (current->right != NULL) {
            stack[stack_size++] =
                (struct Stack){current->right, current_depth + 1};
        }
        if (current->left != NULL) {
            stack[stack_size++] =
                (struct Stack){current->left, current_depth + 1};
        }
    }

    free(stack);

    if (value != NULL) {
        *value = max_depth;
    }

    return SUCCESS;
}

BST_ERROR bst_st_width(bst_st_t *bst, size_t *value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    size_t w = 0;
    bst_st_node_t **q = malloc(sizeof **q * bst->count);

    if (q == NULL) {
        return MALLOC_FAILURE;
    }

    size_t f = 0, r = 0;

    q[r++] = bst->root;

    while (f < r) {
        size_t count = r - f;
        w = w > count ? w : count;

        for (int i = 0; i < count; i++) {
            bst_st_node_t *n = q[f++];
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

    return SUCCESS;
}

void bst_st_node_traverse_preorder(bst_st_node_t *node) {
    if (node != NULL) {
        printf("%ld ", node->value);
        bst_st_node_traverse_preorder(node->left);
        bst_st_node_traverse_preorder(node->right);
    }
}

BST_ERROR bst_st_traverse_preorder(bst_st_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_traverse_preorder(bst->root);
    printf("\n");

    return SUCCESS;
}

void bst_st_node_traverse_inorder(bst_st_node_t *node) {
    if (node != NULL) {
        bst_st_node_traverse_inorder(node->left);
        printf("%ld ", node->value);
        bst_st_node_traverse_inorder(node->right);
    }
}

BST_ERROR bst_st_traverse_inorder(bst_st_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_traverse_inorder(bst->root);
    printf("\n");

    return SUCCESS;
}

void bst_st_node_traverse_postorder(bst_st_node_t *node) {
    if (node != NULL) {
        bst_st_node_traverse_postorder(node->left);
        bst_st_node_traverse_postorder(node->right);
        printf("%ld ", node->value);
    }
}

BST_ERROR bst_st_traverse_postorder(bst_st_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_traverse_postorder(bst->root);
    printf("\n");

    return SUCCESS;
}

BST_ERROR bst_st_delete(bst_st_t *bst, int64_t value) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    bst_st_node_t *current = bst->root, *parent = NULL;

    // Find the node
    while (current != NULL && current->value != value) {
        if (value < current->value) {
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
        (current->left != NULL) ? current->left : current->right;
    if (parent == NULL) {
        bst->root = child; // Delete the root node
    } else if (parent->left == current) {
        parent->left = child;
    } else {
        parent->right = child;
    }

    free(current);
    bst->count--;
    return SUCCESS;
}

void save_inorder(bst_st_node_t *node, int64_t *inorder, int64_t *index) {
    if (node == NULL) {
        return;
    }

    save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    save_inorder(node->right, inorder, index);
}

void bst_node_free(bst_st_node_t *root) {
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

bst_st_node_t *array_to_bst(int64_t arr[], int64_t start, int64_t end,
                            BST_ERROR *err) {
    if (start > end) {
        if (err != NULL) {
            *err = SUCCESS;
        }
        return NULL;
    }

    int64_t mid = (start + end) / 2;

    BST_ERROR e;

    bst_st_node_t *node = bst_st_node_new(arr[mid], &e);

    if (IS_SUCCESS(e)) {
        node->left = array_to_bst(arr, start, mid - 1, &e);

        if (IS_SUCCESS(e)) {
            node->right = array_to_bst(arr, mid + 1, end, &e);

            if (IS_SUCCESS(e)) {
                if (err != NULL) {
                    *err = SUCCESS;
                }
                return node;
            }

            bst_node_free(node);

            if (err != NULL) {
                *err = e;
            }

            return NULL;
        }

        bst_node_free(node);

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

BST_ERROR bst_st_rebalance(bst_st_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    if (bst->root == NULL) {
        return BST_EMPTY;
    }

    int64_t *inorder = malloc(sizeof *inorder * bst->count);

    if (inorder == NULL) {
        return MALLOC_FAILURE;
    }

    int64_t index = 0;
    save_inorder(bst->root, inorder, &index);

    bst_node_free(bst->root);

    BST_ERROR err;
    bst->root = array_to_bst(inorder, 0, index - 1, &err);

    if (IS_SUCCESS(err)) {
        bst->count = index;
        free(inorder);
        return SUCCESS;
    }

    free(inorder);
    free(bst);
    return err;
}

BST_ERROR bst_st_free(bst_st_t *bst) {
    if (bst == NULL) {
        return BST_NULL;
    }

    bst_node_free(bst->root);

    free(bst);

    return SUCCESS;
}