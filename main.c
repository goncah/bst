#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "bst_mt_gmtx.h"
#include "bst_mt_lmtx.h"
#include "bst_st.h"

char *usage() {
    char *msg = "\
Usage:\n\
Test BST ST:\n\
    $ test_bst st <inserts> <repeat>\n\
Test BST MT Global Mutex:\n\
    $ test_bst gmtx <threads> <inserts> <repeat>\n\
Test BST MT Local Mutex:\n\
    $ test_bst lmtx <threads> <inserts> <repeat>\n\
Test BST MT CAS:\n\
    $ test_bst cas <threads> <inserts> <repeat>\n\
    \n";

    return msg;
}

// Robert Jenkins' 96 bit Mix Function
// https://web.archive.org/web/20070111091013/http://www.concentric.net/~Ttwang/tech/inthash.htm
uint64_t mix(uint64_t a, uint64_t b, uint64_t c) {
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

// Swap two ll integers
void swap(int64_t *a, int64_t *b) {
    int64_t t = *b;

    *b = *a;
    *a = t;
}

// https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
void fisher_yates_shuffle(size_t n, int64_t *a) {
    srand(mix(clock(), time(NULL), getpid()));

    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % i;

        swap(&a[j], &a[i]);
    }
}

enum bst_type { ST, GMTX, LMTX };

typedef struct test_bst_s {
    int64_t inserts;
    int64_t start;
    int64_t *values;
    void *bst;
} test_bst_s;

void *bst_st_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_st_t *bst = (bst_st_t *)data->bst;
    int64_t inserts = data->inserts;
    int64_t start = data->start;
    int64_t *values = data->values;

    for (int64_t i = 0; i < inserts; i++) {
        bst_st_add(bst, values[start + i]);
    }

    return NULL;
}

void *bst_mt_gmtx_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_mt_gmtx_t *bst = (bst_mt_gmtx_t *)data->bst;
    int64_t inserts = data->inserts;
    int64_t start = data->start;
    int64_t *values = data->values;

    for (int64_t i = 0; i < inserts; i++) {
        bst_mt_gmtx_add(bst, values[start + i]);
    }

    return NULL;
}

void *bst_mt_lmtx_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_mt_lmtx_t *bst = (bst_mt_lmtx_t *)data->bst;
    int64_t inserts = data->inserts;
    int64_t start = data->start;
    int64_t *values = data->values;

    for (int64_t i = 0; i < inserts; i++) {
        bst_mt_lmtx_add(bst, values[start + i]);
    }

    return NULL;
}

void bst_test(int64_t inserts, size_t threads, enum bst_type bt,
              size_t repeat) {
    int64_t *values = malloc(sizeof *values * inserts);
    for (int64_t i = 0; i < inserts; i++) {
        values[i] = i;
    }

    fisher_yates_shuffle(inserts, values);

    for (int64_t r = 0; r < repeat; r++) {
        struct timeval start, end;

        void *bst = NULL;

        void *(*function)(void *) = NULL;

        switch (bt) {
        case ST:
            function = bst_st_test_thread;
            bst = bst_st_new(NULL);
            break;
        case GMTX:
            function = bst_mt_gmtx_test_thread;
            bst = bst_mt_gmtx_new(NULL);
            break;
        case LMTX:
            function = bst_mt_lmtx_test_thread;
            bst = bst_mt_lmtx_new(NULL);
            break;
        default:
            return;
        }

        int64_t ti = inserts / threads;
        int64_t tr = inserts % threads;
        int64_t tc = tr != 0 ? threads + 1 : threads;

        test_bst_s t_data[tc];

        for (size_t i = 0; i < threads; i++) {
            test_bst_s t;
            t.inserts = ti;
            t.start = i * ti;
            t.bst = bst;
            t.values = values;
            t_data[i] = t;
        }

        if (tr != 0) {
            test_bst_s t;
            t.inserts = tr;
            t.start = threads * ti;
            t.bst = bst;
            t.values = values;
            t_data[threads + 1] = t;
        }

        gettimeofday(&start, NULL);
        pthread_t thread[tc];
        for (int i = 0; i < tc; i++) {
            if (pthread_create(&thread[i], NULL, function, &t_data[i])) {
                PANIC("pthread_create() failure");
            };
        }

        for (int i = 0; i < tc; i++) {
            if (pthread_join(thread[i], NULL)) {
                PANIC("pthread_join() failure");
            }
        }

        gettimeofday(&end, NULL);
        double time_taken =
            end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;

        size_t nc = 0, height = 0, width = 0;
        int64_t min = 0, max = 0;

        switch (bt) {
        case ST:
            nc = ((bst_st_t *)bst)->count;
            bst_st_min(bst, &min);
            bst_st_max(bst, &max);
            bst_st_height(bst, &height);
            bst_st_width(bst, &width);
            bst_st_free(bst);
            break;
        case GMTX:
            bst_mt_gmtx_node_count(bst, &nc);
            bst_mt_gmtx_min(bst, &min);
            bst_mt_gmtx_max(bst, &max);
            bst_mt_gmtx_height(bst, &height);
            bst_mt_gmtx_width(bst, &width);
            bst_mt_gmtx_free(bst);
            break;
        case LMTX:
            bst_mt_lmtx_node_count(bst, &nc);
            bst_mt_lmtx_min(bst, &min);
            bst_mt_lmtx_max(bst, &max);
            bst_mt_lmtx_height(bst, &height);
            bst_mt_lmtx_width(bst, &width);
            bst_mt_lmtx_free(bst);
            break;
        default:
            return;
        }

        printf("%ld,", nc);
        printf("%ld,", min);
        printf("%ld,", max);
        printf("%ld,", height);
        printf("%ld,", width);
        printf("%f\n", time_taken);
    }
}

typedef enum {
    STR2LLINT_SUCCESS,
    STR2LLINT_OVERFLOW,
    STR2LLINT_UNDERFLOW,
    STR2LLINT_INCONVERTIBLE
} str2int_errno;

str2int_errno str2int(int64_t *out, char *s) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2LLINT_INCONVERTIBLE;
    errno = 0;
    int64_t l = strtol(s, &end, 10);

    if (errno == ERANGE && l == LLONG_MAX)
        return STR2LLINT_OVERFLOW;
    if (errno == ERANGE && l == LLONG_MIN)
        return STR2LLINT_UNDERFLOW;
    if (*end != '\0')
        return STR2LLINT_INCONVERTIBLE;
    *out = l;
    return STR2LLINT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        PANIC(usage());
    }

    if (strncmp(argv[1], "st", 2) == 0) {
        if (argc < 3) {
            PANIC(usage());
        }

        int64_t inserts, repeat = 1;

        if (str2int(&inserts, argv[2]) != STR2LLINT_SUCCESS) {
            PANIC(usage());
        }

        if (argc > 3) {
            if (str2int(&repeat, argv[3]) != STR2LLINT_SUCCESS) {
                PANIC(usage());
            }
        }

        bst_test(inserts, 1, ST, repeat);
    } else if (strncmp(argv[1], "gmtx", 4) == 0) {
        if (argc < 4) {
            PANIC(usage());
        }

        int64_t inserts, threads, repeat = 1;

        if (str2int(&threads, argv[2]) != STR2LLINT_SUCCESS) {
            PANIC(usage());
        }

        if (str2int(&inserts, argv[3]) != STR2LLINT_SUCCESS) {
            PANIC(usage());
        }

        if (argc > 4) {
            if (str2int(&repeat, argv[4]) != STR2LLINT_SUCCESS) {
                PANIC(usage());
            }
        }

        bst_test(inserts, threads, GMTX, repeat);
    } else if (strncmp(argv[1], "lmtx", 4) == 0) {
        if (argc < 4) {
            PANIC(usage());
        }

        int64_t inserts, threads, repeat = 1;

        if (str2int(&threads, argv[2]) != STR2LLINT_SUCCESS) {
            PANIC(usage());
        }

        if (str2int(&inserts, argv[3]) != STR2LLINT_SUCCESS) {
            PANIC(usage());
        }

        if (argc > 4) {
            if (str2int(&repeat, argv[4]) != STR2LLINT_SUCCESS) {
                PANIC(usage());
            }
        }

        bst_test(inserts, threads, LMTX, repeat);
    } else {
        PANIC(usage());
    }

    return 0;
}
