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
    STR2LLINT_SUCCESS,
    STR2LLINT_OVERFLOW,
    STR2LLINT_UNDERFLOW,
    STR2LLINT_INCONVERTIBLE
} str2int_errno;

str2int_errno str2int(long long int *out, char *s) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2LLINT_INCONVERTIBLE;
    errno = 0;
    long long int l = strtoll(s, &end, 10);

    if (errno == ERANGE && l == LLONG_MAX)
        return STR2LLINT_OVERFLOW;
    if (errno == ERANGE && l == LLONG_MIN)
        return STR2LLINT_UNDERFLOW;
    if (*end != '\0')
        return STR2LLINT_INCONVERTIBLE;
    *out = l;
    return STR2LLINT_SUCCESS;
}

typedef struct bst_node {
    long long int value;
    struct bst_node *left;
    struct bst_node *right;
} bst_node;

bst_node *bst_node_new(long long int value) {
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

int max(long long int a, long long int b) { return (a > b) ? a : b; }

// Robert Jenkins' 96 bit Mix Function
// https://web.archive.org/web/20070111091013/http://www.concentric.net/~Ttwang/tech/inthash.htm
unsigned long mix(unsigned long long int a, unsigned long long int b,
                  unsigned long long int c) {
    a = a - b;
    a = a - c;
    a = a ^ (c >> 13);
    b = b - c;
    b = b - a;
    b = b ^ (a << 8);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 13);
    a = a - b;
    a = a - c;
    a = a ^ (c >> 12);
    b = b - c;
    b = b - a;
    b = b ^ (a << 16);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 5);
    a = a - b;
    a = a - c;
    a = a ^ (c >> 3);
    b = b - c;
    b = b - a;
    b = b ^ (a << 10);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 15);
    return c;
}

void swap(long long int *a, long long int *b) {
    long long int t = *b;

    *b = *a;
    *a = t;
}

// https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
void fisher_yates_shuffle(long long int n, long long int *a) {
    srand(mix(clock(), time(NULL), getpid()));

    for (long long int i = n - 1; i > 0; i--) {
        long long int j = rand() % i;

        swap(&a[j], &a[i]);
    }
}

#endif