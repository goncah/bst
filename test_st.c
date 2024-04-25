#include <stdio.h>
#include <time.h>

#include "bst_st.h"

int main() {
    time_t tnow, tthen;
    srand(time(NULL));

    time(&tnow);
    bst_st *bst = bst_st_new();

    for (int i = 0; i < 10; i++) {
        bst_st_add(bst, i);
    }

    bst_st_delete(bst, 1);
    bst_st_delete(bst, 8);

    bst_st_traverse_preorder(bst);
    bst_st_print_details(bst);

    bst->root->right->value = 10;

    bst = bst_st_rebalance(bst);
    bst_st_print_details(bst);

    time(&tthen);

    bst_st_traverse_preorder(bst);

    return 0;
}
