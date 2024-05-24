/*
Universidade Aberta
File: bst_mt_fgl.c
Author: Hugo Gonçalves, 2100562

MT Safe Fine-Grained Lock BST using pthreads Mutex.

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
#include "include/bst_mt_fgl.h"

bst_mt_lrwl_node_t *bst_mt_lrwl_node_new(const int64_t value, BST_ERROR *err) {
    bst_mt_lrwl_node_t *node = malloc(sizeof(bst_mt_lrwl_node_t));

    if (node == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    pthread_mutex_init(&node->rwl, NULL);

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

    pthread_mutex_init(&bst->rwl, NULL);
    pthread_mutex_init(&bst->crwl, NULL);

    bst->count = 0;
    bst->root = NULL;

    return bst;
}

BST_ERROR bst_mt_lrwl_add(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;
    pthread_mutex_lock(&bst_->rwl);
    if (bst_->root == NULL) {
        bst_->root = malloc(sizeof(bst_mt_lrwl_node_t));
        bst_->root->value = value;
        bst_->root->left = bst_->root->right = NULL;
        pthread_mutex_init(&bst_->root->rwl, NULL);
        pthread_mutex_lock(&bst_->crwl);
        bst_->count++;
        pthread_mutex_unlock(&bst_->crwl);
        pthread_mutex_unlock(&bst_->rwl);
        return SUCCESS;
    }

    bst_mt_lrwl_node_t *current = bst_->root;

    pthread_mutex_lock(&current->rwl);
    pthread_mutex_unlock(&bst_->rwl);

    while (current != NULL) {
        if (value < current->value) {
            if (current->left == NULL) {
                current->left = malloc(sizeof(bst_mt_lrwl_node_t));
                current->left->value = value;
                current->left->left = NULL;
                current->left->right = NULL;
                pthread_mutex_init(&current->left->rwl, NULL);
                pthread_mutex_lock(&bst_->crwl);
                bst_->count++;
                pthread_mutex_unlock(&bst_->crwl);
                pthread_mutex_unlock(&current->rwl);
                return SUCCESS;
            }

            if (current->left) {
                pthread_mutex_lock(&current->left->rwl);
            }
            bst_mt_lrwl_node_t *t = current;
            current = current->left;
            pthread_mutex_unlock(&t->rwl);
        } else if (value > current->value) {
            if (current->right == NULL) {
                current->right = malloc(sizeof(bst_mt_lrwl_node_t));
                current->right->value = value;
                current->right->left = NULL;
                current->right->right = NULL;
                pthread_mutex_init(&current->right->rwl, NULL);
                pthread_mutex_lock(&bst_->crwl);
                bst_->count++;
                pthread_mutex_unlock(&bst_->crwl);
                pthread_mutex_unlock(&current->rwl);
                return SUCCESS;
            }

            if (current->right) {
                pthread_mutex_lock(&current->right->rwl);
            }

            bst_mt_lrwl_node_t *t = current;
            current = current->right;
            pthread_mutex_unlock(&t->rwl);
        } else {
            pthread_mutex_unlock(&current->rwl);
            return VALUE_EXISTS; // Value already exists in the tree
        }
    }

    return SUCCESS;
}

static int bst_mt_lrwl_find(bst_mt_lrwl_node_t *root, const int64_t value) {
    if (compare(value, root->value) < 0) {
        if (root->left == NULL) {
            pthread_mutex_unlock(&root->rwl);
            return 1;
        }
        pthread_mutex_lock(&root->left->rwl);
        pthread_mutex_unlock(&root->rwl);
        return bst_mt_lrwl_find(root->left, value);
    } else if (compare(value, root->value) > 0) {
        if (root->right == NULL) {
            pthread_mutex_unlock(&root->rwl);
            return 1;
        }
        pthread_mutex_lock(&root->right->rwl);
        pthread_mutex_unlock(&root->rwl);
        return bst_mt_lrwl_find(root->right, value);
    }

    pthread_mutex_unlock(&root->rwl);
    return 0;
}

BST_ERROR bst_mt_lrwl_search(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    pthread_mutex_lock(&bst_->root->rwl);
    pthread_mutex_unlock(&bst_->rwl);

    if (bst_mt_lrwl_find(bst_->root, value)) {
        return VALUE_NONEXISTENT;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_min(bst_mt_lrwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    bst_mt_lrwl_node_t *root = bst_->root;

    pthread_mutex_lock(&root->rwl);
    pthread_mutex_unlock(&bst_->rwl);

    while (root->left != NULL) {
        pthread_mutex_lock(&root->left->rwl);
        bst_mt_lrwl_node_t *t = root;
        root = root->left;
        pthread_mutex_unlock(&t->rwl);
    }

    if (value != NULL) {
        *value = root->value;
    }

    pthread_mutex_unlock(&root->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_max(bst_mt_lrwl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    bst_mt_lrwl_node_t *root = bst_->root;

    pthread_mutex_lock(&root->rwl);
    pthread_mutex_unlock(&bst_->rwl);

    while (root->right != NULL) {
        pthread_mutex_lock(&root->right->rwl);
        bst_mt_lrwl_node_t *t = root;
        root = root->right;
        pthread_mutex_unlock(&t->rwl);
    }

    if (value != NULL) {
        *value = root->value;
    }

    pthread_mutex_unlock(&root->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_node_count(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->crwl);

    if (value != NULL) {
        *value = bst_->count;
    }

    pthread_mutex_unlock(&bst_->crwl);

    return SUCCESS;
}

typedef struct queue_node {
    bst_mt_lrwl_node_t *tree_node;
    struct queue_node *next;
} queue_node_t;

typedef struct queue {
    queue_node_t *front;
    queue_node_t *rear;
} queue_t;

static void enqueue(queue_t *q, bst_mt_lrwl_node_t *node) {
    queue_node_t *new_node = malloc(sizeof(queue_node_t));
    new_node->tree_node = node;
    new_node->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = new_node;
    } else {
        q->rear->next = new_node;
        q->rear = new_node;
    }
}

static bst_mt_lrwl_node_t *dequeue(queue_t *q) {
    if (q->front == NULL) {
        return NULL;
    }
    queue_node_t *temp = q->front;
    bst_mt_lrwl_node_t *node = temp->tree_node;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    return node;
}

BST_ERROR bst_mt_lrwl_height(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL || (*bst)->root == NULL) {
        return BST_NULL | BST_EMPTY;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);
    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        if (value)
            *value = 0;
        return BST_EMPTY;
    }

    if (bst_->root->left == NULL && bst_->root->right == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        if (value)
            *value = 1;
        return SUCCESS;
    }

    pthread_mutex_lock(&bst_->root->rwl);
    pthread_mutex_unlock(&bst_->rwl);

    queue_t q;
    q.front = q.rear = NULL;
    enqueue(&q, bst_->root);

    size_t max_width = 0;

    while (1) {
        int level_node_count = 0;
        int nodes_at_current_level = 0;

        const queue_node_t *current = q.front;
        while (current != NULL) {
            nodes_at_current_level++;
            current = current->next;
        }

        if (nodes_at_current_level == 0) {
            break;
        }

        while (nodes_at_current_level > 0) {
            bst_mt_lrwl_node_t *current_node = dequeue(&q);
            if (current_node->left != NULL) {
                pthread_mutex_lock(&current_node->left->rwl);
                enqueue(&q, current_node->left);
            }
            if (current_node->right != NULL) {
                pthread_mutex_lock(&current_node->right->rwl);
                enqueue(&q, current_node->right);
            }
            pthread_mutex_unlock(&current_node->rwl);
            nodes_at_current_level--;
            level_node_count++;
        }

        if (level_node_count > max_width) {
            max_width = level_node_count;
        }
    }

    if (value)
        *value = max_width;

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_width(bst_mt_lrwl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL || (*bst)->root == NULL) {
        return BST_NULL | BST_EMPTY;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);
    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        if (value)
            *value = 0;
        return BST_EMPTY;
    }

    if (bst_->root->left == NULL && bst_->root->right == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        if (value)
            *value = 1;
        return SUCCESS;
    }

    pthread_mutex_lock(&bst_->root->rwl);
    pthread_mutex_unlock(&bst_->rwl);

    queue_t q;
    q.front = q.rear = NULL;
    enqueue(&q, bst_->root);

    int max_width = 0;

    while (1) {
        int level_node_count = 0;

        queue_node_t *current = q.front;
        while (current != NULL) {
            level_node_count++;
            current = current->next;
        }

        if (level_node_count == 0) {
            break;
        }

        if (level_node_count > max_width) {
            max_width = level_node_count;
        }

        while (level_node_count > 0) {
            bst_mt_lrwl_node_t *current_node = dequeue(&q);

            if (current_node->left != NULL) {
                pthread_mutex_lock(&current_node->left->rwl);
                enqueue(&q, current_node->left);
            }
            if (current_node->right != NULL) {
                pthread_mutex_lock(&current_node->right->rwl);
                enqueue(&q, current_node->right);
            }
            pthread_mutex_unlock(&current_node->rwl);
            level_node_count--;
        }
    }

    if (value) {
        *value = max_width;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_preorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_mutex_unlock(&bst_->rwl);
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack = malloc(sizeof **stack * bst_size);

    if (stack == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
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

    pthread_mutex_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_inorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_mutex_unlock(&bst_->rwl);
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack = malloc(sizeof **stack * bst_size);
    if (stack == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
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

    pthread_mutex_unlock(&bst_->rwl);

    return SUCCESS;
}

BST_ERROR bst_mt_lrwl_traverse_postorder(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_mutex_unlock(&bst_->rwl);
        return be;
    }

    // Stack to store the nodes
    bst_mt_lrwl_node_t **stack1 = malloc(sizeof **stack1 * bst_size);
    if (stack1 == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return MALLOC_FAILURE;
    }

    bst_mt_lrwl_node_t **stack2 = malloc(sizeof **stack2 * bst_size);
    if (stack2 == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
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

    pthread_mutex_unlock(&bst_->rwl);

    return SUCCESS;
}

static void bst_mt_lrwl_delete_root(bst_mt_lrwl_t *bst) {
    bst_mt_lrwl_node_t *root = bst->root;

    // No children
    if (root->left == NULL && root->right == NULL) {
        bst->root = NULL;
        pthread_mutex_unlock(&root->rwl);
        pthread_mutex_destroy(&root->rwl);
        free(root);
        pthread_mutex_unlock(&bst->rwl);
        return;
    }

    // Single child
    if (root->left == NULL || root->right == NULL) {
        bst->root = root->left ? root->left : root->right;
        pthread_mutex_unlock(&root->rwl);
        pthread_mutex_destroy(&root->rwl);
        free(root);
        pthread_mutex_unlock(&bst->rwl);
        return;
    }

    // Both children
    bst_mt_lrwl_node_t *parent = root;
    bst_mt_lrwl_node_t *curr = root->right;
    pthread_mutex_lock(&curr->rwl);
    pthread_mutex_unlock(&bst->rwl);

    for (;;) {
        if (curr->left == NULL) {
            root->value = curr->value;
            if (parent == root) {
                parent->right = curr->right == NULL ? NULL : curr->right;
            } else {
                parent->left = curr->right == NULL ? NULL : curr->right;
                pthread_mutex_unlock(&parent->rwl);
            }
            free(curr);
            pthread_mutex_unlock(&root->rwl);
            return;
        }

        pthread_mutex_lock(&curr->left->rwl);
        if (parent != root) {
            pthread_mutex_unlock(&parent->rwl);
        }
        parent = curr;
        curr = curr->left;
    }
}

BST_ERROR bst_mt_lrwl_delete(bst_mt_lrwl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    bst_mt_lrwl_node_t *root = bst_->root;

    if (root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    pthread_mutex_lock(&root->rwl);
    if (root->value == value) {
        bst_mt_lrwl_delete_root(bst_);
        return SUCCESS;
    }

    pthread_mutex_unlock(&bst_->rwl);

    bst_mt_lrwl_node_t *curr = root;
    bst_mt_lrwl_node_t *parent = NULL;
    bst_mt_lrwl_node_t *next = NULL;

    for (;;) {
        if (value < curr->value) {
            if (curr->left == NULL) {
                pthread_mutex_unlock(&curr->rwl);
                if (parent) {
                    pthread_mutex_unlock(&parent->rwl);
                }
                return VALUE_NONEXISTENT;
            }
            next = curr->left;
        } else if (value > curr->value) {
            if (curr->right == NULL) { // The value doesn't exist
                pthread_mutex_unlock(&curr->rwl);
                if (parent) {
                    pthread_mutex_unlock(&parent->rwl);
                }
                return VALUE_NONEXISTENT;
            }
            next = curr->right;
        } else {
            if (curr->left == NULL && curr->right == NULL) {
                if (curr == parent->left) {
                    parent->left = NULL;
                } else {
                    parent->right = NULL;
                }

                if (parent) {
                    pthread_mutex_unlock(&parent->rwl);
                }
                free(curr);
                pthread_mutex_lock(&bst_->crwl);
                bst_->count--;
                pthread_mutex_unlock(&bst_->crwl);
                return SUCCESS;

            } else if (curr->left == NULL || curr->right == NULL) {
                // One child
                if (curr == parent->left) {
                    parent->left =
                        curr->left == NULL ? curr->right : curr->left;
                } else {
                    parent->right =
                        curr->left == NULL ? curr->right : curr->left;
                }

                free(curr);
                if (parent) {
                    pthread_mutex_unlock(&parent->rwl);
                }
                pthread_mutex_lock(&bst_->crwl);
                bst_->count--;
                pthread_mutex_unlock(&bst_->crwl);
                return SUCCESS;

            } else {
                bst_mt_lrwl_node_t *curr_parent = curr;
                bst_mt_lrwl_node_t *curr_min = curr->right;
                pthread_mutex_lock(&curr_min->rwl);
                pthread_mutex_unlock(&parent->rwl);

                for (;;) {
                    if (curr_min->left == NULL) {
                        curr->value = curr_min->value;
                        if (curr_parent == curr) {
                            curr_parent->right = curr_min->right == NULL
                                                     ? NULL
                                                     : curr_min->right;
                        } else {
                            curr_parent->left = curr_min->right == NULL
                                                    ? NULL
                                                    : curr_min->right;
                            pthread_mutex_unlock(&curr_parent->rwl);
                        }

                        free(curr_min);
                        pthread_mutex_unlock(&curr->rwl);
                        pthread_mutex_lock(&bst_->crwl);
                        bst_->count--;
                        pthread_mutex_unlock(&bst_->crwl);

                        return SUCCESS;
                    }

                    // If we haven't found the minimum element, continue
                    // traversing.
                    pthread_mutex_unlock(&curr_min->left->rwl);
                    if (curr_parent != curr) {
                        pthread_mutex_unlock(&curr_parent->rwl);
                    }
                    curr_parent = curr_min;
                    curr_min = curr_min->left;
                }
            }
        }

        pthread_mutex_lock(&next->rwl);
        if (parent) {
            pthread_mutex_unlock(&parent->rwl);
        }
        parent = curr;
        curr = next;
    }
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

    pthread_mutex_destroy(&root->rwl);
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

    pthread_mutex_lock(&bst_->rwl);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
        return BST_EMPTY;
    }

    size_t bst_size = 0;
    const BST_ERROR be = bst_mt_lrwl_node_count(bst, &bst_size);

    if (!IS_SUCCESS(be)) {
        pthread_mutex_unlock(&bst_->rwl);
        return be;
    }

    int64_t *inorder = malloc(sizeof(int64_t) * bst_size);

    if (inorder == NULL) {
        pthread_mutex_unlock(&bst_->rwl);
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

        pthread_mutex_unlock(&bst_->rwl);

        return SUCCESS;
    }

    free(inorder);

    bst_->root = NULL;

    pthread_mutex_unlock(&bst_->rwl);

    return err;
}

BST_ERROR bst_mt_lrwl_free(bst_mt_lrwl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_lrwl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->rwl);

    *bst = NULL;

    bst_mt_lrwl_node_free(bst_->root);
    bst_->root = NULL;

    pthread_mutex_unlock(&bst_->rwl);
    pthread_mutex_destroy(&bst_->rwl);
    pthread_mutex_destroy(&bst_->crwl);
    free(bst_);

    bst = NULL;

    return SUCCESS;
}