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

bst_mt_fgl_node_t *bst_mt_lrwl_node_new(const int64_t value, BST_ERROR *err) {
    bst_mt_fgl_node_t *node = malloc(sizeof(bst_mt_fgl_node_t));

    if (node == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    pthread_mutex_init(&node->mtx, NULL);

    node->value = value;
    node->left = NULL;
    node->right = NULL;

    if (err != NULL) {
        *err = SUCCESS;
    }

    return node;
}

bst_mt_fgl_t *bst_mt_fgl_new(BST_ERROR *err) {
    bst_mt_fgl_t *bst = malloc(sizeof(bst_mt_fgl_t));

    if (bst == NULL) {
        if (err != NULL) {
            *err = MALLOC_FAILURE;
        }

        return NULL;
    }

    pthread_mutex_init(&bst->mtx, NULL);
    pthread_mutex_init(&bst->cmtx, NULL);

    bst->count = 0;
    bst->root = NULL;

    return bst;
}

BST_ERROR bst_mt_fgl_add(bst_mt_fgl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;
    pthread_mutex_lock(&bst_->mtx);
    if (bst_->root == NULL) {
        bst_->root = malloc(sizeof(bst_mt_fgl_node_t));
        bst_->root->value = value;
        bst_->root->left = bst_->root->right = NULL;
        pthread_mutex_init(&bst_->root->mtx, NULL);
        pthread_mutex_lock(&bst_->cmtx);
        bst_->count++;
        pthread_mutex_unlock(&bst_->cmtx);
        pthread_mutex_unlock(&bst_->mtx);
        return SUCCESS;
    }

    bst_mt_fgl_node_t *current = bst_->root;

    pthread_mutex_lock(&current->mtx);
    pthread_mutex_unlock(&bst_->mtx);

    while (current != NULL) {
        if (compare(value, current->value) < 0) {
            if (current->left == NULL) {
                current->left = malloc(sizeof(bst_mt_fgl_node_t));
                current->left->value = value;
                current->left->left = NULL;
                current->left->right = NULL;
                pthread_mutex_init(&current->left->mtx, NULL);
                pthread_mutex_lock(&bst_->cmtx);
                bst_->count++;
                pthread_mutex_unlock(&bst_->cmtx);
                pthread_mutex_unlock(&current->mtx);
                return SUCCESS;
            }

            if (current->left) {
                pthread_mutex_lock(&current->left->mtx);
            }
            bst_mt_fgl_node_t *t = current;
            current = current->left;
            pthread_mutex_unlock(&t->mtx);
        } else if (compare(value, current->value) > 0) {
            if (current->right == NULL) {
                current->right = malloc(sizeof(bst_mt_fgl_node_t));
                current->right->value = value;
                current->right->left = NULL;
                current->right->right = NULL;
                pthread_mutex_init(&current->right->mtx, NULL);
                pthread_mutex_lock(&bst_->cmtx);
                bst_->count++;
                pthread_mutex_unlock(&bst_->cmtx);
                pthread_mutex_unlock(&current->mtx);
                return SUCCESS;
            }

            if (current->right) {
                pthread_mutex_lock(&current->right->mtx);
            }

            bst_mt_fgl_node_t *t = current;
            current = current->right;
            pthread_mutex_unlock(&t->mtx);
        } else {
            pthread_mutex_unlock(&current->mtx);
            return VALUE_EXISTS; // Value already exists in the tree
        }
    }

    return SUCCESS;
}

static int bst_mt_lrwl_find(bst_mt_fgl_node_t *root, const int64_t value) {
    if (compare(value, root->value) < 0) {
        if (root->left == NULL) {
            pthread_mutex_unlock(&root->mtx);
            return 1;
        }
        pthread_mutex_lock(&root->left->mtx);
        pthread_mutex_unlock(&root->mtx);
        return bst_mt_lrwl_find(root->left, value);
    } else if (compare(value, root->value) > 0) {
        if (root->right == NULL) {
            pthread_mutex_unlock(&root->mtx);
            return 1;
        }
        pthread_mutex_lock(&root->right->mtx);
        pthread_mutex_unlock(&root->mtx);
        return bst_mt_lrwl_find(root->right, value);
    }

    pthread_mutex_unlock(&root->mtx);
    return 0;
}

BST_ERROR bst_mt_fgl_search(bst_mt_fgl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->mtx);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->mtx);
        return BST_EMPTY;
    }

    pthread_mutex_lock(&bst_->root->mtx);
    pthread_mutex_unlock(&bst_->mtx);

    if (bst_mt_lrwl_find(bst_->root, value)) {
        return VALUE_NONEXISTENT;
    }

    return SUCCESS;
}

BST_ERROR bst_mt_fgl_min(bst_mt_fgl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->mtx);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->mtx);
        return BST_EMPTY;
    }

    bst_mt_fgl_node_t *root = bst_->root;

    pthread_mutex_lock(&root->mtx);
    pthread_mutex_unlock(&bst_->mtx);

    while (root->left != NULL) {
        pthread_mutex_lock(&root->left->mtx);
        bst_mt_fgl_node_t *t = root;
        root = root->left;
        pthread_mutex_unlock(&t->mtx);
    }

    if (value != NULL) {
        *value = root->value;
    }

    pthread_mutex_unlock(&root->mtx);

    return SUCCESS;
}

BST_ERROR bst_mt_fgl_max(bst_mt_fgl_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->mtx);

    if (bst_->root == NULL) {
        pthread_mutex_unlock(&bst_->mtx);
        return BST_EMPTY;
    }

    bst_mt_fgl_node_t *root = bst_->root;

    pthread_mutex_lock(&root->mtx);
    pthread_mutex_unlock(&bst_->mtx);

    while (root->right != NULL) {
        pthread_mutex_lock(&root->right->mtx);
        bst_mt_fgl_node_t *t = root;
        root = root->right;
        pthread_mutex_unlock(&t->mtx);
    }

    if (value != NULL) {
        *value = root->value;
    }

    pthread_mutex_unlock(&root->mtx);

    return SUCCESS;
}

BST_ERROR bst_mt_fgl_node_count(bst_mt_fgl_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->cmtx);

    if (value != NULL) {
        *value = bst_->count;
    }

    pthread_mutex_unlock(&bst_->cmtx);

    return SUCCESS;
}

static void bst_mt_lrwl_delete_root(bst_mt_fgl_t *bst) {
    bst_mt_fgl_node_t *root = bst->root;

    // No children
    if (root->left == NULL && root->right == NULL) {
        bst->root = NULL;
        pthread_mutex_unlock(&root->mtx);
        //pthread_mutex_destroy(&root->mtx);
        //free(root);
        pthread_mutex_unlock(&bst->mtx);
        return;
    }

    // Single child
    if (root->left == NULL || root->right == NULL) {
        bst->root = root->left ? root->left : root->right;
        pthread_mutex_unlock(&root->mtx);
        //pthread_mutex_destroy(&root->mtx);
        //free(root);
        pthread_mutex_unlock(&bst->mtx);
        return;
    }

    // Both children
    bst_mt_fgl_node_t *parent = root;
    bst_mt_fgl_node_t *curr = root->right;
    pthread_mutex_lock(&curr->mtx);
    pthread_mutex_unlock(&bst->mtx);

    for (;;) {
        if (curr->left == NULL) {
            root->value = curr->value;
            if (parent == root) {
                parent->right = curr->right == NULL ? NULL : curr->right;
            } else {
                parent->left = curr->right == NULL ? NULL : curr->right;
                pthread_mutex_unlock(&parent->mtx);
            }
            //free(curr);
            pthread_mutex_unlock(&root->mtx);
            return;
        }

        pthread_mutex_lock(&curr->left->mtx);
        if (parent != root) {
            pthread_mutex_unlock(&parent->mtx);
        }
        parent = curr;
        curr = curr->left;
    }
}

BST_ERROR bst_mt_fgl_delete(bst_mt_fgl_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->mtx);

    bst_mt_fgl_node_t *root = bst_->root;

    if (root == NULL) {
        pthread_mutex_unlock(&bst_->mtx);
        return BST_EMPTY;
    }

    pthread_mutex_lock(&root->mtx);
    if (root->value == value) {
        bst_mt_lrwl_delete_root(bst_);
        return SUCCESS;
    }

    pthread_mutex_unlock(&bst_->mtx);

    bst_mt_fgl_node_t *curr = root;
    bst_mt_fgl_node_t *parent = NULL;
    bst_mt_fgl_node_t *next = NULL;

    for (;;) {
        if (value < curr->value) {
            if (curr->left == NULL) {
                pthread_mutex_unlock(&curr->mtx);
                if (parent) {
                    pthread_mutex_unlock(&parent->mtx);
                }
                return VALUE_NONEXISTENT;
            }
            next = curr->left;
        } else if (value > curr->value) {
            if (curr->right == NULL) { // The value doesn't exist
                pthread_mutex_unlock(&curr->mtx);
                if (parent) {
                    pthread_mutex_unlock(&parent->mtx);
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

                pthread_mutex_unlock(&curr->mtx);
                pthread_mutex_unlock(&parent->mtx);
                free(curr);
                pthread_mutex_lock(&bst_->cmtx);
                bst_->count--;
                pthread_mutex_unlock(&bst_->cmtx);
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
                    pthread_mutex_unlock(&parent->mtx);
                }
                pthread_mutex_lock(&bst_->cmtx);
                bst_->count--;
                pthread_mutex_unlock(&bst_->cmtx);
                return SUCCESS;

            } else {
                bst_mt_fgl_node_t *curr_parent = curr;
                bst_mt_fgl_node_t *curr_min = curr->right;
                pthread_mutex_lock(&curr_min->mtx);

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
                            pthread_mutex_unlock(&curr_parent->mtx);
                        }

                        free(curr_min);
                        pthread_mutex_unlock(&curr->mtx);
                        pthread_mutex_unlock(&parent->mtx);
                        pthread_mutex_lock(&bst_->cmtx);
                        bst_->count--;
                        pthread_mutex_unlock(&bst_->cmtx);

                        return SUCCESS;
                    }

                    // If we haven't found the minimum element, continue
                    // traversing.
                    pthread_mutex_unlock(&parent->mtx);
                    pthread_mutex_unlock(&curr_min->left->mtx);
                    if (curr_parent != curr) {
                        pthread_mutex_unlock(&curr_parent->mtx);
                    }
                    curr_parent = curr_min;
                    curr_min = curr_min->left;
                }
            }
        }

        pthread_mutex_lock(&next->mtx);
        if (parent) {
            pthread_mutex_unlock(&parent->mtx);
        }
        parent = curr;
        curr = next;
    }
}

void bst_mt_lrwl_node_free(bst_mt_fgl_node_t *root) {
    if (root == NULL) {
        return;
    }

    if (root->left != NULL) {
        bst_mt_lrwl_node_free(root->left);
    }

    if (root->right != NULL) {
        bst_mt_lrwl_node_free(root->right);
    }

    pthread_mutex_destroy(&root->mtx);
    free(root);
}

BST_ERROR bst_mt_fgl_free(bst_mt_fgl_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_NULL;
    }

    bst_mt_fgl_t *bst_ = *bst;

    pthread_mutex_lock(&bst_->mtx);

    *bst = NULL;

    bst_mt_lrwl_node_free(bst_->root);
    bst_->root = NULL;

    pthread_mutex_unlock(&bst_->mtx);
    pthread_mutex_destroy(&bst_->mtx);
    pthread_mutex_destroy(&bst_->cmtx);
    free(bst_);

    bst = NULL;

    return SUCCESS;
}