/*
Universidade Aberta
File: bst_at.c
Author: Hugo Gonçalves, 2100562

MT Safe BST using atomic operations and hazard pointers.

MIT License

Copyright (c) 2024 Hugo Gonçalves

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

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
#include <stdbool.h>
#include <stdlib.h>

#include "../include/bst_common.h"
#include "include/bst_at.h"

#include <unistd.h>

// Hazard pointers handling funtions
static hazard_pointer_t *acquire_hazard_pointer(bst_at_t *bst) {
    hazard_pointer_t *hp = malloc(sizeof(hazard_pointer_t));
    atomic_store(&hp->pointer, NULL);
    hp->next = NULL;

    // Insert into global hazard pointer list
    hazard_pointer_t *old_head = NULL;
    do {
        old_head = atomic_load(&bst->hazard_pointers);
        hp->next = old_head;
    } while (
        !atomic_compare_exchange_weak(&bst->hazard_pointers, &old_head, hp));

    return hp;
}

static void set_hazard_pointer(hazard_pointer_t *hp, bst_at_node_t *node) {
    atomic_store(&hp->pointer, node);
}

static void release_hazard_pointer(hazard_pointer_t *hp) {
    atomic_store(&hp->pointer, NULL);
}

static bool is_node_hazardous(bst_at_t *bst, bst_at_node_t *node) {
    hazard_pointer_t *current = atomic_load(&bst->hazard_pointers);
    while (current != NULL) {
        if (atomic_load(&current->pointer) == node) {
            return true;
        }
        current = current->next;
    }
    return false;
}

static void retire_node(bst_at_t *bst, bst_at_node_t *node) {
    if (!is_node_hazardous(bst, node)) {
        free(node);
    }
}

static bst_at_node_t *bst_at_node_new(const int64_t value) {
    bst_at_node_t *node = malloc(sizeof(bst_at_node_t));

    if (node) {
        node->value = value;
        atomic_store(&node->left, NULL);
        atomic_store(&node->right, NULL);
    }

    return node;
}

bst_at_t *bst_at_new(BST_ERROR *err) {
    bst_at_t *bst = malloc(sizeof(bst_at_t));

    if (bst) {
        atomic_store(&bst->count, 0);
        atomic_store(&bst->root, NULL);
        atomic_store(&bst->hazard_pointers, NULL);

        if (err) {
            *err = SUCCESS;
        }
    } else if (err) {
        *err = MALLOC_FAILURE;
    }

    return bst;
}

BST_ERROR bst_at_add(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    bst_at_node_t *new_node = bst_at_node_new(value);

    while (1) {
        bst_at_node_t *parent = NULL;
        bst_at_node_t *current = atomic_load(&bst_->root);
        hazard_pointer_t *hp_parent = acquire_hazard_pointer(bst_);
        hazard_pointer_t *hp_current = acquire_hazard_pointer(bst_);

        while (current != NULL) {
            set_hazard_pointer(hp_current, current);

            if (!compare(value, current->value)) {
                release_hazard_pointer(hp_parent);
                release_hazard_pointer(hp_current);
                free(new_node);
                return VALUE_EXISTS;
            }

            parent = current;
            set_hazard_pointer(hp_parent, parent);

            if (compare(value, current->value) < 0) {
                current = atomic_load(&current->left);
            } else {
                current = atomic_load(&current->right);
            }
            set_hazard_pointer(hp_current, current);
        }

        if (parent == NULL) {
            if (atomic_compare_exchange_weak(&bst_->root, &current,
                                               new_node)) {
                release_hazard_pointer(hp_parent);
                release_hazard_pointer(hp_current);
                atomic_fetch_add(&bst_->count, 1);
                return SUCCESS;
            }
        } else {
            if (compare(value, parent->value) < 0) {
                if (atomic_compare_exchange_weak(&parent->left, &current,
                                                   new_node)) {
                    release_hazard_pointer(hp_parent);
                    release_hazard_pointer(hp_current);
                    atomic_fetch_add(&bst_->count, 1);
                    return SUCCESS;
                }
            } else {
                if (atomic_compare_exchange_weak(&parent->right, &current,
                                                   new_node)) {
                    release_hazard_pointer(hp_parent);
                    release_hazard_pointer(hp_current);
                    atomic_fetch_add(&bst_->count, 1);
                    return SUCCESS;
                }
            }
        }
        release_hazard_pointer(hp_parent);
        release_hazard_pointer(hp_current);
    }
}

BST_ERROR bst_at_search(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    hazard_pointer_t *hp = acquire_hazard_pointer(bst_);
    bst_at_node_t *current = atomic_load(&bst_->root);

    while (current != NULL) {
        set_hazard_pointer(hp, current);

        int64_t cmp = compare(value, current->value);

        if (cmp < 0) {
            current = atomic_load(&current->left);
        } else if (cmp > 0) {
            current = atomic_load(&current->right);
        } else {
            release_hazard_pointer(hp);
            return SUCCESS;
        }
    }

    release_hazard_pointer(hp);

    return VALUE_NONEXISTENT;
}

BST_ERROR bst_at_min(bst_at_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    hazard_pointer_t *hp = acquire_hazard_pointer(bst_);
    bst_at_node_t *current = atomic_load(&bst_->root);

    if (current == NULL) {
        release_hazard_pointer(hp);
        return BST_EMPTY;
    }

    while (atomic_load(&current->left) != NULL) {
        set_hazard_pointer(hp, current);
        current = atomic_load(&current->left);
    }

    set_hazard_pointer(hp, current);
    if (value) {
        *value = current->value;
    }
    release_hazard_pointer(hp);

    return SUCCESS;
}

BST_ERROR bst_at_max(bst_at_t **bst, int64_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    hazard_pointer_t *hp = acquire_hazard_pointer(bst_);
    bst_at_node_t *current = atomic_load(&bst_->root);

    if (current == NULL) {
        release_hazard_pointer(hp);
        return BST_EMPTY;
    }

    while (atomic_load(&current->right) != NULL) {
        set_hazard_pointer(hp, current);
        current = atomic_load(&current->right);
    }

    set_hazard_pointer(hp, current);
    if (value) {
        *value = current->value;
    }
    release_hazard_pointer(hp);

    return SUCCESS;
}

BST_ERROR bst_at_node_count(bst_at_t **bst, size_t *value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    size_t s = atomic_load(&bst_->count);

    if (value) {
        *value = s;
    }

    return SUCCESS;
}

static bool bst_replace_node(bst_at_t *bst, bst_at_node_t *parent,
                             bst_at_node_t *current, bst_at_node_t *new_node) {
    if (parent == NULL) {
        return atomic_compare_exchange_strong(&bst->root, &current, new_node);
    }

    if (current == atomic_load(&parent->left)) {
        return atomic_compare_exchange_strong(&parent->left, &current,
                                              new_node);
    }

    return atomic_compare_exchange_strong(&parent->right, &current, new_node);
}

BST_ERROR bst_at_delete(bst_at_t **bst, const int64_t value) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;

    hazard_pointer_t *hp_parent = acquire_hazard_pointer(bst_);
    hazard_pointer_t *hp_current = acquire_hazard_pointer(bst_);
    hazard_pointer_t *hp_successor = acquire_hazard_pointer(bst_);

    while (1) {
        bst_at_node_t *parent = NULL;
        bst_at_node_t *current = atomic_load(&bst_->root);

        while (current != NULL) {
            set_hazard_pointer(hp_current, current);

            if (!compare(value, current->value)) {
                break;
            }

            parent = current;
            set_hazard_pointer(hp_parent, parent);

            if (compare(value, current->value) < 0) {
                current = atomic_load(&current->left);
            } else {
                current = atomic_load(&current->right);
            }
            set_hazard_pointer(hp_current, current);
        }

        if (current == NULL) {
            release_hazard_pointer(hp_parent);
            release_hazard_pointer(hp_current);
            release_hazard_pointer(hp_successor);
            return VALUE_NONEXISTENT;
        }

        bst_at_node_t *left_child = atomic_load(&current->left);
        bst_at_node_t *right_child = atomic_load(&current->right);

        if (left_child == NULL || right_child == NULL) {
            bst_at_node_t *child =
                left_child != NULL ? left_child : right_child;

            if (bst_replace_node(bst_, parent, current, child)) {
                release_hazard_pointer(hp_parent);
                release_hazard_pointer(hp_current);
                release_hazard_pointer(hp_successor);
                retire_node(bst_, current);
                atomic_fetch_sub(&bst_->count, 1);
                return SUCCESS;
            }
        } else {
            bst_at_node_t *successor_parent = current;
            bst_at_node_t *successor = right_child;
            set_hazard_pointer(hp_successor, successor);

            while (atomic_load(&successor->left) != NULL) {
                successor_parent = successor;
                successor = atomic_load(&successor->left);
                set_hazard_pointer(hp_successor, successor);
            }

            const int64_t successor_value = successor->value;

            // Replace the current node's key with the successor's key
            current->value = successor_value;

            // Now we need to remove the successor node
            if (successor_parent == current) {
                atomic_store(&successor_parent->right,
                             atomic_load(&successor->right));
            } else {
                atomic_store(&successor_parent->left,
                             atomic_load(&successor->right));
            }

            release_hazard_pointer(hp_parent);
            release_hazard_pointer(hp_current);
            release_hazard_pointer(hp_successor);

            retire_node(bst_, successor);
            atomic_fetch_sub(&bst_->count, 1);
            return SUCCESS;
        }
    }
}

static void bst_at_free_node(bst_at_node_t *root) {
    if (root) {
        bst_at_free_node(root->left);
        bst_at_free_node(root->right);
        free(root);
    }
}

static void bst_at_free_hp(hazard_pointer_t *hp) {
    if (hp) {
        bst_at_free_hp(hp->next);
        free(hp);
    }
}

BST_ERROR bst_at_free(bst_at_t **bst) {
    if (bst == NULL || *bst == NULL) {
        return BST_EMPTY;
    }

    bst_at_t *bst_ = *bst;
    *bst = NULL;

    bst_at_free_node(atomic_load(&bst_->root));
    bst_at_free_hp(atomic_load(&bst_->hazard_pointers));
    free(bst_);

    return 0;
}