#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "bst_mt_gmtx.h"
#include "bst_mt_lmtx.h"
#include "bst_st.h"
#include "string.h"

#define INSERTS 10
#define REBALANCE 100

char *usage() {
    char *msg = "\
Usage:\n\
Test BST ST:\n\
    $ test_bst st <inserts>\n\
Test BST MT Global Mutex:\n\
    $ test_bst gmtx <threads> <inserts>\n\
    \n";

    return msg;
}

void bst_st_test(int inserts) {
    srand(time(NULL));
    struct timeval start, end;

    gettimeofday(&start, NULL);
    bst_st *bst = bst_st_new();

    for (int i = 0, r = 0; i < inserts; i++, r++) {
        bst_st_add(bst, i + rand() % inserts);
    }

    gettimeofday(&end, NULL);
    double time_taken =
        end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;

    printf("Nodes,Min,Max,Height,Width,Time\n");
    bst_st_print_details(bst);

    printf("%f\n", time_taken);
}

typedef struct test_bst_mtg_s {
    int inserts;
    bst_mt_gmtx *bst;
} test_bst_mtg_s;

void *bst_mt_gmtx_test_thread(void *vargp) {
    test_bst_mtg_s *data = (test_bst_mtg_s *)vargp;
    bst_mt_gmtx *bst = data->bst;
    int inserts = data->inserts;
    srand(time(NULL));
    for (int i = 0, r = 0; i < inserts; i++, r++) {
        bst_mt_gmtx_add(bst, i + rand() % inserts);
    }

    return NULL;
}

void bst_mt_gmtx_test(int inserts, int threads) {
    struct timeval start, end;

    gettimeofday(&start, NULL);

    bst_mt_gmtx *bst = bst_mt_gmtx_new();

    int ti = inserts / threads;
    int tr = inserts % threads;
    int tc = tr != 0 ? threads + 1 : threads;

    test_bst_mtg_s t_data[tc];
    pthread_t thread[tc];

    for (int i = 0; i < threads; i++) {
        test_bst_mtg_s t;
        t.inserts = ti;
        t.bst = bst;
        t_data[i] = t;
    }

    if (tr != 0) {
        test_bst_mtg_s t;
        t.inserts = tr;
        t.bst = bst;
        t_data[threads + 1] = t;
    }

    for (int i = 0; i < tc; i++) {
        pthread_create(&thread[i], NULL, bst_mt_gmtx_test_thread, &t_data[i]);
    }

    for (int i = 0; i < tc; i++) {
        pthread_join(thread[i], NULL);
    }

    gettimeofday(&end, NULL);
    double time_taken =
        end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;

    printf("Nodes,Min,Max,Height,Width,Time\n");
    bst_mt_gmtx_print_details(bst);

    printf("%f\n", time_taken);
}

typedef struct test_bst_mtl_s {
    int inserts;
    bst_mt_lmtx *bst;
} test_bst_mtl_s;

void *bst_mt_lmtx_test_thread(void *vargp) {
    test_bst_mtl_s *data = (test_bst_mtl_s *)vargp;
    bst_mt_lmtx *bst = data->bst;
    int inserts = data->inserts;

    srand(time(NULL));
    for (int i = 0, r = 0; i < inserts; i++, r++) {
        bst_mt_lmtx_add(bst, i + rand() % inserts);
    }

    return NULL;
}

void bst_mt_lmtx_test(int inserts, int threads) {
    struct timeval start, end;

    gettimeofday(&start, NULL);

    bst_mt_lmtx *bst = bst_mt_lmtx_new();

    int ti = inserts / threads;
    int tr = inserts % threads;
    int tc = tr != 0 ? threads + 1 : threads;

    test_bst_mtl_s t_data[tc];
    pthread_t thread[tc];

    for (int i = 0; i < threads; i++) {
        test_bst_mtl_s t;
        t.inserts = ti;
        t.bst = bst;
        t_data[i] = t;
    }

    if (tr != 0) {
        test_bst_mtl_s t;
        t.inserts = tr;
        t.bst = bst;
        t_data[threads + 1] = t;
    }

    for (int i = 0; i < tc; i++) {
        pthread_create(&thread[i], NULL, bst_mt_lmtx_test_thread, &t_data[i]);
    }

    for (int i = 0; i < tc; i++) {
        pthread_join(thread[i], NULL);
    }

    gettimeofday(&end, NULL);
    double time_taken =
        end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;

    printf("Nodes,Min,Max,Height,Width,Time\n");
    bst_mt_lmtx_print_details(bst);

    printf("%f\n", time_taken);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        PANIC(usage());
    }

    if (strncmp(argv[1], "st", 2) == 0) {
        if (argc < 3) {
            PANIC(usage());
        }

        int inserts;

        if (str2int(&inserts, argv[2], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        bst_st_test(inserts);
    } else if (strncmp(argv[1], "gmtx", 4) == 0) {
        if (argc < 4) {
            PANIC(usage());
        }

        int inserts, threads;

        if (str2int(&threads, argv[2], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        if (str2int(&inserts, argv[3], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        bst_mt_gmtx_test(inserts, threads);
    } else if (strncmp(argv[1], "lmtx", 4) == 0) {
        if (argc < 4) {
            PANIC(usage());
        }

        int inserts, threads;

        if (str2int(&threads, argv[2], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        if (str2int(&inserts, argv[3], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        bst_mt_lmtx_test(inserts, threads);
    } else {
        PANIC(usage());
    }

    return 0;
}
