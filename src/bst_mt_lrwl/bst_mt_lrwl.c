/*
Universidade Aberta
File: bst_mt_lrwl_t.c
Author: Hugo Gonçalves, 2100562

MT Safe BST using a both a global and a local RwLock, the latter with write
preference. This further improves the insert performance due to no global
lock on insert. The global RwLock is used inverted meaning that insert
operation will read lock it and all others will write lock it. The local
RwLock is write locked when inserting and read locked on all
other operations.

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
#include "include/bst_mt_lrwl.h"

bst_mt_lrwl_node_t *bst_mt_lrwl_node_new(const int64_t value, BST_ERROR *err) {
    bst_mt_lrwl_node_t *node = malloc(sizeof(bst_mt_lrwl_node_t));

    if (node == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    pthread_rwlock_init(&node->rwl, NULL);

    node->value = value;
    node->left = NULL;
    node->right = NULL;

    if (err != NULL) {
        *err = SUCCESS;
    }

    return node;
}

bst_mt_lrwl_t *bst_mt_lrwl_new(BST_ERROR *err) {
    bst_mt_lrwl_t *bst = malloc(sizeof(bst_mt_lrwl_t));

    if (bst == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    pthread_rwlock_init(&bst->rwl, NULL);
    pthread_rwlock_init(&bst->crwl, NULL);

    bst->count = 0;
    bst->root = NULL;

    return bst;
}

BST_ERROR bst_mt_lrwl_add(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;
    pthread_rwlock_wrlock(&bst_->rwl);
    if (bst_->root == NULL) {
        bst_->root = malloc(sizeof(bst_mt_lrwl_node_t));
        bst_->root->value = value;
        bst_->root->left = bst_->root->right = NULL;
        pthread_rwlock_init(&bst_->root->rwl, NULL);
        pthread_rwlock_wrlock(&bst_->crwl);
        bst_->count++;
        pthread_rwlock_unlock(&bst_->crwl);
        pthread_rwlock_unlock(&bst_->rwl);
        return SUCCESS;
    }

    bst_mt_lrwl_node_t *current = bst_->root;

    pthread_rwlock_wrlock(&current->rwl);
    pthread_rwlock_unlock(&bst_->rwl);

    while (current != NULL) {
        if (value < current->value) {
            if (current->left == NULL) {
                current->left =
                    (bst_mt_lrwl_node_t *)malloc(sizeof(bst_mt_lrwl_node_t));
                current->left->value = value;
                current->left->left = current->left->right = NULL;
                pthread_rwlock_init(&current->left->rwl, NULL);
                pthread_rwlock_wrlock(&bst_->crwl);
                bst_->count++;
                pthread_rwlock_unlock(&bst_->crwl);
                pthread_rwlock_unlock(&current->rwl);
                return SUCCESS;
            }

            if (current->left) {
                pthread_rwlock_wrlock(&current->left->rwl);
            }
            bst_mt_lrwl_node_t *t = current;
            current = current->left;
            pthread_rwlock_unlock(&t->rwl);
        } else if (value > current->value) {
            if (current->right == NULL) {
                current->right =
                    (bst_mt_lrwl_node_t *)malloc(sizeof(bst_mt_lrwl_node_t));
                current->right->value = value;
                current->right->left = current->right->right = NULL;
                pthread_rwlock_init(&current->right->rwl, NULL);
                pthread_rwlock_wrlock(&bst_->crwl);
                bst_->count++;
                pthread_rwlock_unlock(&bst_->crwl);
                pthread_rwlock_unlock(&current->rwl);
                return SUCCESS;
            }

            if (current->right) {
                pthread_rwlock_wrlock(&current->right->rwl);
            }

            bst_mt_lrwl_node_t *t = current;
            current = current->right;
            pthread_rwlock_unlock(&t->rwl);
        } else {
            pthread_rwlock_unlock(&current->rwl);
            return VALUE_EXISTS; // Value already exists in the tree
        }
    }

    return SUCCESS;
}

static int bst_mt_lrwl_find(bst_mt_lrwl_node_t *root, const int64_t value) {
    if (compare(value, root->value) < 0) {
        if (root->left == NULL) {
            return 1;
        }
        pthread_rwlock_rdlock(&root->left->rwl);
        const int r = bst_mt_lrwl_find(root->left, value);
        pthread_rwlock_unlock(&root->left->rwl);
        return r;
    } else if (compare(value, root->value) > 0) {
        if (root->right == NULL) {
            return 1;
        }
        pthread_rwlock_rdlock(&root->right->rwl);
        const int r = bst_mt_lrwl_find(root->right, value);
        pthread_rwlock_unlock(&root->right->rwl);
        return r;
    }

    return 0;
}

BST_ERROR bst_mt_lrwl_search(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    pthread_rwlock_rdlock(&bst_->root->rwl);

    if (bst_mt_lrwl_find(bst_->root, value)) {
        pthread_rwlock_unlock(&bst_->root->rwl);
        pthread_rwlock_unlock(&bst_->rwl);
        return VALUE_NONEXISTENT;
    }

    pthread_rwlock_unlock(&bst_->root->rwl);
    pthread_rwlock_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_min(bst_mt_lrwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    bst_mt_lrwl_node_t *root = bst_->root;

    pthread_rwlock_rdlock(&root->rwl);
    pthread_rwlock_unlock(&bst_->rwl);

    while (root->left != NULL) {
        pthread_rwlock_rdlock(&root->left->rwl);
        bst_mt_lrwl_node_t *t = root;
        root = root->left;
        pthread_rwlock_unlock(&t->rwl);
    }

    if (value != NULL) {
        *value = root->value;
    }

    pthread_rwlock_unlock(&root->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_max(bst_mt_lrwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    bst_mt_lrwl_node_t *root = bst_->root;

    pthread_rwlock_rdlock(&root->rwl);
    pthread_rwlock_unlock(&bst_->rwl);

    while (root->right != NULL) {
        pthread_rwlock_rdlock(&root->right->rwl);
        bst_mt_lrwl_node_t *t = root;
        root = root->right;
        pthread_rwlock_unlock(&t->rwl);
    }

    if (value != NULL) {
        *value = root->value;
    }

    pthread_rwlock_unlock(&root->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_node_count(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->crwl);

    if (value != NULL) {
        *value = bst_->count;
    }

    pthread_rwlock_unlock(&bst_->crwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_height(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        if (value != NULL) {
            *value = 0;
        }

        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    // Stack for tree nodes to avoid recursion allowing easier concurrency
    // control
    struct Stack {
        bst_mt_lrwl_node_t *node;
        size_t depth;
    };
    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_rwlock_unlock(&bst_->rwl);
        return be;
    }

    struct Stack *stack = malloc(sizeof *stack * bst_size);

    size_t stack_size = 0;

    // Initial push of the root node with depth 0
    stack[stack_size++] = (struct Stack){bst_->root, 0};

    size_t max_depth = 0;

    while (stack_size > 0) {
        // Pop the top element from stack
        const struct Stack top = stack[--stack_size];
        const bst_mt_lrwl_node_t *current = top.node;
        const size_t current_depth = top.depth;

        // Update maximum depth found
        if (current_depth >= max_depth) {
            max_depth = current_depth;
        }

        // Push children to the stack with incremented depth
        if (current != NULL && current->right != NULL) {
            stack[stack_size++] =
                (struct Stack){current->right, current_depth + 1};
        }
        if (current != NULL && current->left != NULL) {
            stack[stack_size++] =
                (struct Stack){current->left, current_depth + 1};
        }
    }

    free(stack);

    if (value != NULL) {
        *value = max_depth;
    }

    pthread_rwlock_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_width(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        if (value != NULL) {
            *value = 0;
        }

        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    int64_t w = 0;
    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_rwlock_unlock(&bst_->rwl);
        return be;
    }

    bst_mt_lrwl_node_t **q = malloc(sizeof **q * bst_size);

    if (q == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);

        return MALLOC_FAILURE;
    }

    int64_t f = 0, r = 0;

    q[r++] = bst_->root;

    while (f < r) {
        // Compute the number of nodes at the current level.
        const int64_t count = r - f;

        // Update the maximum width if the current level's width is greater.
        w = w > count ? w : count;

        // Loop through each node on the current level and enqueue children.
        for (int i = 0; i < count; i++) {
            const bst_mt_lrwl_node_t *n = q[f++];
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

    pthread_rwlock_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_preorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_rwlock_unlock(&bst_->rwl);
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack = malloc(sizeof **stack * bst_size);

    if (stack == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return MALLOC_FAILURE;
    }

    size_t top = 0;
    stack[top++] = bst_->root;

    while (top > 0) {
        const bst_mt_lrwl_node_t *node = stack[--top];
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

    pthread_rwlock_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_inorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_rwlock_unlock(&bst_->rwl);
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack = malloc(sizeof **stack * bst_size);
    if (stack == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return MALLOC_FAILURE;
    }

    size_t top = 0;
    bst_mt_lrwl_node_t *current = bst_->root;

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

    pthread_rwlock_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_postorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_rdlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_rwlock_unlock(&bst_->rwl);
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack1 = malloc(sizeof **stack1 * bst_size);
    if (stack1 == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return MALLOC_FAILURE;
    }

    bst_mt_lrwl_node_t **stack2 = malloc(sizeof **stack2 * bst_size);
    if (stack2 == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return MALLOC_FAILURE;
    }

    size_t top1 = 0, top2 = 0;

    stack1[top1++] = bst_->root;
    bst_mt_lrwl_node_t *node;

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

    pthread_rwlock_unlock(&bst_->rwl);

    return SUCCESS;
}

static bst_mt_lrwl_node_t *bst_mt_lrwl_find_min(bst_mt_lrwl_node_t *root) {
    if (root == NULL)
        return NULL;

    pthread_rwlock_rdlock(&root->rwl);

    bst_mt_lrwl_node_t *current = root;
    while (current && current->left != NULL) {
        bst_mt_lrwl_node_t *t = current;
        current = current->left;
        pthread_rwlock_rdlock(&current->rwl);
        pthread_rwlock_unlock(&t->rwl);
    }

    pthread_rwlock_unlock(&current->rwl);
    return current;
}

static bst_mt_lrwl_node_t *bst_mt_lrwl_del(bst_mt_lrwl_node_t *root,
                                           const int64_t value) {
    if (root == NULL)
        return root;

    pthread_rwlock_wrlock(&root->rwl);

    if (compare(value, root->value) < 0) {
        if (root->left != NULL) {
            pthread_rwlock_wrlock(&root->left->rwl);
        }

        root->left = bst_mt_lrwl_del(root->left, value);
        pthread_rwlock_unlock(&root->rwl);
    } else if (value > root->value) {
        if (root->right != NULL) {
            pthread_rwlock_wrlock(&root->right->rwl);
        }

        root->right = bst_mt_lrwl_del(root->right, value);
        pthread_rwlock_unlock(&root->rwl);
    } else {
        // Node with only one child or no child
        if (root->left == NULL || root->right == NULL) {
            bst_mt_lrwl_node_t *temp = root->left ? root->left : root->right;
            pthread_rwlock_unlock(&root->rwl);  // Unlock the root
            pthread_rwlock_destroy(&root->rwl); // Destroy the mutex
            free(root);
            return temp;
        }

        // Node with two children: get the inorder successor (smallest in the
        // right subtree)
        bst_mt_lrwl_node_t *temp = bst_mt_lrwl_find_min(root->right);

        // Copy the inorder successor's content to this node
        root->value = temp->value;

        // Delete the inorder successor
        pthread_rwlock_wrlock(&root->right->rwl);
        root->right = bst_mt_lrwl_del(root->right, temp->value);
        pthread_rwlock_unlock(&root->rwl);
    }

    return root;
}

BST_ERROR bst_mt_lrwl_delete(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_wrlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    if (bst_->root->value == value) {
        bst_->root = bst_mt_lrwl_del(bst_->root, value);

        pthread_rwlock_wrlock(&bst_->crwl);
        bst_->count--;
        pthread_rwlock_unlock(&bst_->crwl);
        pthread_rwlock_unlock(&bst_->rwl);
        return SUCCESS;
    }

    pthread_rwlock_unlock(&bst_->rwl);

    bst_mt_lrwl_del(bst_->root, value);

    pthread_rwlock_wrlock(&bst_->crwl);
    bst_->count--;
    pthread_rwlock_unlock(&bst_->crwl);

    return SUCCESS;
}

void lrwl_save_inorder(const bst_mt_lrwl_node_t *node, int64_t *inorder,
                       int64_t *index) {
    if (node == NULL)
        return;
    lrwl_save_inorder(node->left, inorder, index);
    inorder[(*index)++] = node->value;
    lrwl_save_inorder(node->right, inorder, index);
}

void bst_mt_lrwl_node_free(bst_mt_lrwl_node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_mt_lrwl_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_mt_lrwl_node_free(root->right);
    }

    pthread_rwlock_destroy(&root->rwl);
    free(root);
}

bst_mt_lrwl_node_t *lrwl_array_to_bst(int64_t *arr, const int64_t start,
                                      const int64_t end, BST_ERROR *err) {
    if (start > end) {
        if (err != NULL) {
            *err = SUCCESS;
        }
        return NULL;
    }

    const int64_t mid = (start + end) / 2;

    BST_ERROR e;

    bst_mt_lrwl_node_t *node = bst_mt_lrwl_node_new(arr[mid], &e);

    if (IS_SUCCESS(e)) {
        node->left = lrwl_array_to_bst(arr, start, mid - 1, &e);

        if (IS_SUCCESS(e)) {
            node->right = lrwl_array_to_bst(arr, mid + 1, end, &e);

            if (IS_SUCCESS(e)) {
                if (err != NULL) {
                    *err = SUCCESS;
                }
                return node;
            }

            bst_mt_lrwl_node_free(node);

            if (err != NULL) {
                *err = e;
            }

            return NULL;
        }

        bst_mt_lrwl_node_free(node);

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

BST_ERROR bst_mt_lrwl_rebalance(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_wrlock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_rwlock_unlock(&bst_->rwl);
        return be;
    }

    int64_t *inorder = malloc(sizeof(int64_t) * bst_size);

    if (inorder == NULL) {
        pthread_rwlock_unlock(&bst_->rwl);
        return MALLOC_FAILURE;
    }

    int64_t index = 0;
    lrwl_save_inorder(bst_->root, inorder, &index);

    bst_mt_lrwl_node_free(bst_->root);

    BST_ERROR err;
    bst_mt_lrwl_node_t *node = lrwl_array_to_bst(inorder, 0, index - 1, &err);

    if (IS_SUCCESS(err)) {
        free(inorder);
        bst_->root = node;

        pthread_rwlock_unlock(&bst_->rwl);

        return SUCCESS;
    }

    free(inorder);

    bst_->root = NULL;

    pthread_rwlock_unlock(&bst_->rwl);

    return err;
}

BST_ERROR bst_mt_lrwl_free(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_rwlock_wrlock(&bst_->rwl);

    *bst = NULL;

    bst_mt_lrwl_node_free(bst_->root);
    bst_->root = NULL;

    pthread_rwlock_unlock(&bst_->rwl);
    pthread_rwlock_destroy(&bst_->rwl);
    free(bst_);

    bst = NULL;

    return SUCCESS;
}