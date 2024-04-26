#include <stdio.h>
#include <time.h>

#include "bst_st.h"
#include "string.h"

#define INSERTS 10
#define REBALANCE 100

char *usage() {
    char *msg = "\
Usage:\n\
Test BST ST:\n\
    $ test_bst st <inserts> <rebalance_every>\n\
    \n";

    return msg;
}

void bst_st_test(int inserts, int rebalance) {
    srand(time(NULL));
    clock_t t = clock();

    bst_st *bst = bst_st_new(rebalance);

    for (int i = 0, r = 0; i < inserts; i++, r++) {
        bst_st_add(bst, i + rand() % inserts);
    }

    double time = ((double)clock() - (double)t) / CLOCKS_PER_SEC;

    printf("Nodes,Min,Max,Height,Width,Time\n");
    bst_st_print_details(bst);

    printf("%f\n", time);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        PANIC(usage());
    }

    if (strncmp(argv[1], "st", 2) == 0) {
        if (argc < 4) {
            PANIC(usage());
        }

        int inserts, rebalance;

        if (str2int(&inserts, argv[2], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        if (str2int(&rebalance, argv[3], 10) != STR2INT_SUCCESS) {
            PANIC(usage());
        }

        bst_st_test(inserts, rebalance);
    } else {
        PANIC(usage());
    }

    return 0;
}
