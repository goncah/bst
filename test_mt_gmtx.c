#include <stdio.h>
#include <time.h>

#include "bst_mt_gmtx.h"

int main() {
    time_t tnow, tthen;
    srand(time(NULL));

    time(&tnow);
    bst_mt_gmtx *bst = bst_mt_gmtx_new();

    for (int i = 0; i < 10; i++) {
        bst_mt_gmtx_add(bst, i);
    }

    bst_mt_gmtx_delete(bst, 1);
    bst_mt_gmtx_delete(bst, 8);

    bst_mt_gmtx_traverse_preorder(bst);
    bst_mt_gmtx_print_details(bst);

    bst->root->right->value = 10;

    bst = bst_mt_gmtx_rebalance(bst);
    bst_mt_gmtx_print_details(bst);

    time(&tthen);

    bst_mt_gmtx_traverse_preorder(bst);

    return 0;
}
