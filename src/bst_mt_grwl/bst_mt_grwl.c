/*
Universidade Aberta
File: bst_mt_grwl_t.c
Author: Hugo Gonçalves, 2100562

MT Safe BST using a global RwLock with write preference.

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
#include "include/bst_mt_grwl.h"

bst_mt_grwl_node_t *bst_mt_grwl_node_new(const int64_t value, BST_ERROR *err) {
    bst_mt_grwl_node_t *node = malloc(sizeof(bst_mt_grwl_node_t));

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

bst_mt_grwl_t *bst_mt_grwl_new(BST_ERROR *err) {
    bst_mt_grwl_t *bst = malloc(sizeof(bst_mt_grwl_t));

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

    pthread_rwlockattr_t rtx_attr;

    if (pthread_rwlockattr_setkind_np(
            &rtx_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP)) {
        free(bst);

        if (err != NULL) {
            *err = PT_RWLOCK_ATTR_INIT_FAILURE;
        }

        return NULL;
    }

    if (pthread_rwlock_init(&bst->rwl, &rtx_attr)) {
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

BST_ERROR bst_mt_grwl_add(bst_mt_grwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        BST_ERROR err;
        bst_mt_grwl_node_t *node = bst_mt_grwl_node_new(value, &err);

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

    bst_mt_grwl_node_t *root = bst_->root;

    while (root != NULL) {
        if (compare(value, root->value) < 0) {
            if (root->left == NULL) {
                BST_ERROR err;
                bst_mt_grwl_node_t *node = bst_mt_grwl_node_new(value, &err);

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
                bst_mt_grwl_node_t *node = bst_mt_grwl_node_new(value, &err);

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

BST_ERROR bst_mt_grwl_search(bst_mt_grwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_grwl_node_t *root = bst_->root;

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

BST_ERROR bst_mt_grwl_min(bst_mt_grwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_grwl_node_t *root = bst_->root;

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

BST_ERROR bst_mt_grwl_max(bst_mt_grwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }
        return BST_EMPTY;
    }

    const bst_mt_grwl_node_t *root = bst_->root;

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

BST_ERROR bst_mt_grwl_node_count(bst_mt_grwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

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

BST_ERROR bst_mt_grwl_height(bst_mt_grwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    // Stack for tree nodes to avoid recursion and allow easier concurrency
    // control
    struct stack {
        bst_mt_grwl_node_t *node;
        size_t depth;
    };

    struct stack *stack = malloc(sizeof *stack * bst_->count);

    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    size_t stack_size = 0;

    // Initial push of the root node with depth 0
    stack[stack_size++] = (struct stack){bst_->root, 0};

    size_t max_depth = 0;

    while (stack_size > 0) {
        // Pop the top element from stack
        const struct stack top = stack[--stack_size];
        const bst_mt_grwl_node_t *current = top.node;
        const size_t current_depth = top.depth;

        // Update maximum depth found
        if (current_depth >= max_depth) {
            max_depth = current_depth;
        }

        // Push children to the stack with incremented depth
        if (current != NULL && current->right != NULL) {
            stack[stack_size++] =
                (struct stack){current->right, current_depth + 1};
        }
        if (current != NULL && current->left != NULL) {
            stack[stack_size++] =
                (struct stack){current->left, current_depth + 1};
        }
    }

    free(stack);

    if (value != NULL) {
        *value = max_depth;
    }

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_grwl_width(bst_mt_grwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    size_t w = 0;
    bst_mt_grwl_node_t **q = malloc(sizeof **q * bst_->count);

    if (q == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    size_t f = 0, r = 0;

    q[r++] = bst_->root;

    while (f < r) {
        // Compute the number of nodes at the current level.
        const size_t count = r - f;

        // Update the maximum width if the current level's width is greater.
        w = w > count ? w : count;

        // Loop through each node on the current level and enqueue children.
        for (int i = 0; i < count; i++) {
            const bst_mt_grwl_node_t *n = q[f++];
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

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_grwl_traverse_preorder(bst_mt_grwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    // Stack to store the nodes
    bst_mt_grwl_node_t **stack = malloc(sizeof **stack * bst_->count);
    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    int top = 0;
    stack[top++] = bst_->root;

    while (top > 0) {
        const bst_mt_grwl_node_t *node = stack[--top];
        printf("%ld ", node->value);

        // Push right child first so that it is processed after the left child
        if (node->right != NULL) {
            stack[top++] = node->right;
        }

        // Push left child
        if (node->left != NULL) {
            stack[top++] = node->left;
        }
    }

    printf("\n");

    free(stack);

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_grwl_traverse_inorder(bst_mt_grwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    // Stack to store the nodes
    bst_mt_grwl_node_t **stack = malloc(sizeof **stack * bst_->count);
    if (stack == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    int top = 0;
    bst_mt_grwl_node_t *current = bst_->root;

    while (current != NULL || top > 0) {
        // Reach the left most Node of the current Node
        while (current != NULL) {
            // Place pointer to a tree node on the stack before traversing the
            // node's left subtree
            stack[top++] = current;
            current = current->left;
        }

        // Current must be NULL at this point
        current = stack[--top];
        printf("%ld ", current->value);

        // We have visited the node and its left subtree. Now, it's right
        // subtree's turn
        current = current->right;
    }

    printf("\n");

    free(stack);

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_grwl_traverse_postorder(bst_mt_grwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_rdlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    // Stack to store the nodes
    bst_mt_grwl_node_t **stack1 = malloc(sizeof **stack1 * bst_->count);
    if (stack1 == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    bst_mt_grwl_node_t **stack2 = malloc(sizeof **stack2 * bst_->count);
    if (stack2 == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }

        return MALLOC_FAILURE;
    }

    int top1 = 0, top2 = 0;

    stack1[top1++] = bst_->root;
    bst_mt_grwl_node_t *node;

    // Run while first stack is not empty
    while (top1 > 0) {
        // Pop an item from stack1 and push it to stack2
        node = stack1[--top1];
        stack2[top2++] = node;

        // Push left and right children of removed item to stack1
        if (node->left) {
            stack1[top1++] = node->left;
        }

        if (node->right) {
            stack1[top1++] = node->right;
        }
    }

    // Print all elements of second stack
    while (top2 > 0) {
        node = stack2[--top2];
        printf("%ld ", node->value);
    }

    printf("\n");

    free(stack1);
    free(stack2);

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_grwl_delete(bst_mt_grwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    bst_mt_grwl_node_t *current = bst_->root, *parent = NULL;

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
        bst_mt_grwl_node_t *successor = current->right;
        bst_mt_grwl_node_t *successor_parent = current;

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
    bst_mt_grwl_node_t *child =
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

void grwl_save_inorder(const bst_mt_grwl_node_t *node, int64_t *inorder,
                       int64_t *index) {
    if (node == NULL)
        return;
    grwl_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    grwl_save_inorder(node->right, inorder, index);
}

void bst_mt_grwl_node_free(bst_mt_grwl_node_t *root) {
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

bst_mt_grwl_node_t *grwl_array_to_bst(int64_t *arr, const int64_t start,
                                      const int64_t end, BST_ERROR *err) {
    if (start > end) {
        if (err != NULL) {
            *err = SUCCESS;
        }
        return NULL;
    }

    const int64_t mid = (start + end) / 2;

    BST_ERROR e;

    bst_mt_grwl_node_t *node = bst_mt_grwl_node_new(arr[mid], &e);

    if (IS_SUCCESS(e)) {
        node->left = grwl_array_to_bst(arr, start, mid - 1, &e);

        if (IS_SUCCESS(e)) {
            node->right = grwl_array_to_bst(arr, mid + 1, end, &e);

            if (IS_SUCCESS(e)) {
                if (err != NULL) {
                    *err = SUCCESS;
                }
                return node;
            }

            bst_mt_grwl_node_free(node);

            if (err != NULL) {
                *err = e;
            }

            return NULL;
        }

        bst_mt_grwl_node_free(node);

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

BST_ERROR bst_mt_grwl_rebalance(bst_mt_grwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

    if (pthread_rwlock_wrlock(&bst_->rwl)) {
        return PT_RWLOCK_LOCK_FAILURE;
    }

    if (bst_->root == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | BST_EMPTY;
        }

        return BST_EMPTY;
    }

    int64_t *inorder = malloc(sizeof(int64_t) * bst_->count);

    if (inorder == NULL) {
        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | MALLOC_FAILURE;
        }
        return MALLOC_FAILURE;
    }

    int64_t index = 0;
    grwl_save_inorder(bst_->root, inorder, &index);

    bst_mt_grwl_node_free(bst_->root);

    BST_ERROR err;
    bst_->root = grwl_array_to_bst(inorder, 0, index - 1, &err);

    if (IS_SUCCESS(err)) {
        bst_->count = index;
        free(inorder);

        if (pthread_rwlock_unlock(&bst_->rwl)) {
            return PT_RWLOCK_UNLOCK_FAILURE | SUCCESS;
        }
        return SUCCESS;
    }

    free(inorder);

    if (pthread_rwlock_unlock(&bst_->rwl)) {
        free(bst_);

        return PT_RWLOCK_UNLOCK_FAILURE | err;
    }

    if (pthread_rwlock_destroy(&bst_->rwl)) {
        free(bst);
        return PT_RWLOCK_DESTROY_FAILURE | err;
    }

    free(bst);
    return err;
}

BST_ERROR bst_mt_grwl_free(bst_mt_grwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_grwl_t *bst_ = *bst;

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