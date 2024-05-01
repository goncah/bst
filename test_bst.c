#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "bst_mt_cas.h"
#include "bst_mt_gmtx.h"
#include "bst_mt_lmtx.h"
#include "bst_st.h"
#include "pthread.h"
#include "string.h"

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

enum bst_type { ST, GMTX, LMTX, CAS };

typedef struct test_bst_s {
    long long int inserts;
    long long int start;
    long long int *values;
    void *bst;
} test_bst_s;

void *bst_st_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_st *bst = (bst_st *)data->bst;
    long long int inserts = data->inserts;
    long long int start = data->start;
    long long int *values = data->values;

    for (long long int i = 0; i < inserts; i++) {
        bst_st_add(bst, values[start + i]);
    }

    return NULL;
}

void *bst_mt_gmtx_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_mt_gmtx *bst = (bst_mt_gmtx *)data->bst;
    long long int inserts = data->inserts;
    long long int start = data->start;
    long long int *values = data->values;

    for (long long int i = 0; i < inserts; i++) {
        bst_mt_gmtx_add(bst, values[start + i]);
    }

    return NULL;
}

void *bst_mt_lmtx_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_mt_lmtx *bst = (bst_mt_lmtx *)data->bst;
    long long int inserts = data->inserts;
    long long int start = data->start;
    long long int *values = data->values;

    for (long long int i = 0; i < inserts; i++) {
        bst_mt_lmtx_add(bst, values[start + i]);
    }

    return NULL;
}

void *bst_mt_cas_test_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_mt_cas *bst = (bst_mt_cas *)data->bst;
    long long int inserts = data->inserts;
    long long int start = data->start;
    long long int *values = data->values;

    for (long long int i = 0, r = 0; i < inserts; i++, r++) {
        bst_mt_cas_add(bst, values[start + i]);
    }

    return NULL;
}

void bst_test(long long int inserts, int threads, enum bst_type bt,
              int repeat) {
    long long int *values = malloc(sizeof *values * inserts);
    for (long long int i = 0; i < inserts; i++) {
        values[i] = i;
    }

    fisher_yates_shuffle(inserts, values);

    for (long long int r = 0; r < repeat; r++) {
        struct timeval start, end;

        void *bst = NULL;

        void *(*function)(void *) = NULL;

        switch (bt) {
        case ST:
            function = bst_st_test_thread;
            bst = bst_st_new();
            break;
        case GMTX:
            function = bst_mt_gmtx_test_thread;
            bst = bst_mt_gmtx_new();
            break;
        case LMTX:
            function = bst_mt_lmtx_test_thread;
            bst = bst_mt_lmtx_new();
            break;
        case CAS:
            function = bst_mt_cas_test_thread;
            bst = bst_mt_cas_new();
            break;
        default:
            return;
        }

        long long int ti = inserts / threads;
        long long int tr = inserts % threads;
        long long int tc = tr != 0 ? threads + 1 : threads;

        test_bst_s t_data[tc];

        for (int i = 0; i < threads; i++) {
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

        switch (bt) {
        case ST:
            bst_st_print_details(bst);
            bst_st_free(bst);
            break;
        case GMTX:
            bst_mt_gmtx_print_details(bst);
            bst_mt_gmtx_free(bst);
            break;
        case LMTX:
            bst_mt_lmtx_print_details(bst);
            bst_mt_lmtx_free(bst);
            break;
        case CAS:
            bst_mt_cas_print_details(bst);
            bst_mt_cas_free(bst);
            break;
        default:
            return;
        }

        printf("%f\n", time_taken);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        PANIC(usage());
    }

    if (strncmp(argv[1], "st", 2) == 0) {
        if (argc < 3) {
            PANIC(usage());
        }

        long long int inserts, repeat = 1;

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

        long long int inserts, threads, repeat = 1;

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

        long long int inserts, threads, repeat = 1;

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
    } else if (strncmp(argv[1], "cas", 3) == 0) {
        if (argc < 4) {
            PANIC(usage());
        }

        long long int inserts, threads, repeat = 1;

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

        bst_test(inserts, threads, CAS, repeat);
    } else {
        PANIC(usage());
    }

    return 0;
}
