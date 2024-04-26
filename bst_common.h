/*
Universidade Aberta
File: bst_mt_gmtx.h
Author: Hugo Gonçalves, 2100562

Single-thread BST

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
#ifndef __bst_common__
#define __bst_common__

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define PANIC(msg)                                                             \
    {                                                                          \
        fprintf(stderr, msg);                                                  \
        exit(1);                                                               \
    }

#define ERROR(msg)                                                             \
    { fprintf(stderr, msg); }

typedef enum {
    STR2INT_SUCCESS,
    STR2INT_OVERFLOW,
    STR2INT_UNDERFLOW,
    STR2INT_INCONVERTIBLE
} str2int_errno;

str2int_errno str2int(int *out, char *s, int base) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2INT_INCONVERTIBLE;
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return STR2INT_OVERFLOW;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return STR2INT_UNDERFLOW;
    if (*end != '\0')
        return STR2INT_INCONVERTIBLE;
    *out = l;
    return STR2INT_SUCCESS;
}

typedef struct bst_node {
    int value;
    struct bst_node *left;
    struct bst_node *right;
} bst_node;

bst_node *bst_node_new(int value) {
    bst_node *node = (bst_node *)malloc(sizeof *node);

    if (node == NULL) {
        PANIC("malloc() failure\n");
    }

    node->value = value;
    node->left = NULL;
    node->right = NULL;

    return node;
}

void bst_node_free(bst_node *root) {
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

int max(int a, int b) { return (a > b) ? a : b; }

#endif