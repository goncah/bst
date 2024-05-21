/*
Universidade Aberta
File: bst_common.h
Author: Hugo Gonçalves, 2100562

Common utility macros

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

#ifndef BST_COMMON_H_
#define BST_COMMON_H_

#define PANIC(msg)                                                             \
    {                                                                          \
        fprintf(stderr, "%s\n", (msg));                                        \
        exit(1);                                                               \
    }

#define ERROR(msg)                                                             \
    { fprintf(stderr, msg); }

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * Function return errors. Bitmask is used to report multiple errors.
 * For example, failing to both unlock the RwLock in LRWL BST and unlock the
 * Mutex in a node will return:
 * PT_MUTEX_UNLOCK_FAILURE | PT_RWLOCK_READ_UNLOCK_FAILURE
 */
// clang-format off
typedef enum BST_ERROR {
    SUCCESS                        = (1u << 1),
    BST_NULL                       = (1u << 2),
    BST_EMPTY                      = (1u << 3),
    VALUE_EXISTS                   = (1u << 4),
    VALUE_NONEXISTENT              = (1u << 5),
    VALUE_NOT_ADDED                = (1u << 6),
    VALUE_ADDED                    = (1u << 7),
    MALLOC_FAILURE                 = (1u << 8),
    PT_RWLOCK_INIT_FAILURE         = (1u << 9),
    PT_RWLOCK_DESTROY_FAILURE      = (1u << 10),
    PT_RWLOCK_ATTR_INIT_FAILURE    = (1u << 11),
    PT_RWLOCK_LOCK_FAILURE         = (1u << 12),
    PT_RWLOCK_UNLOCK_FAILURE       = (1u << 13),
    UNKNOWN                        = (1u << 14)
} BST_ERROR;
// clang-format on

#define IS_SUCCESS(a) (((a) & SUCCESS) == SUCCESS ? 1 : 0)
#endif // BST_COMMON_H_
