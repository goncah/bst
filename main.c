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

#include "bst_mt_grwl.h"
#include "bst_mt_lrwl.h"
#include "bst_st.h"

char *usage() {
    char *msg = "\
Usage:\n\
    $ bst <options>\n\
\n\
Options:\n\
\t-o Set the number of operations\n\
\t-t Set the number of threads, operations are evenly distributed. Ignored for BST ST type.\n\
\t-r Set the number of test repetitions, applies to each strat and each BST type\n\
\t-s <strategy> Set the test strategy. Multiple strategies can be set, example -s 1 -s 2 -s 3. Available strategies are:\n\
\t\tinsert     - Inserts only with random generated numbers\n\
\t\twrite      - Random inserts, deletes with random generated numbers\n\
\t\twriteb     - Random inserts, deletes and rebalance with random generated numbers\n\
\t\tread       - Random search, min, max, height and width. -o sets the number of elements in the read.\n\
\t\tread_write - Random inserts, deletes, search, min, max, height and width with random generated numbers.\n\
\t-c Set the BST type to ST, can be set with -g and -l to test multiple BST types\n\
\t-g Set the BST type to MT Global RwLock, can be set with -c and -l to test multiple BST types\n\
\t-l Set the BST type to MT Local RwLock, can be set with -c and -g to test multiple BST types\n\
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

// Swap two signed long integers
void swap(int64_t *a, int64_t *b) {
    int64_t t = *b;

    *b = *a;
    *a = t;
}

// https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
// shuffle an array of signed long integers
void fisher_yates_shuffle(size_t n, int64_t *a) {
    srand(mix(clock(), time(NULL), getpid()));

    for (size_t i = n - 1; i > 0; i--) {
        size_t j = rand() % i;

        swap(&a[j], &a[i]);
    }
}

typedef enum {
    STR2LLINT_SUCCESS,
    STR2LLINT_OVERFLOW,
    STR2LLINT_UNDERFLOW,
    STR2LLINT_INCONVERTIBLE
} str2int_errno;

// Safely convert string to signed long
str2int_errno str2int(int64_t *out, char *s) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2LLINT_INCONVERTIBLE;
    errno = 0;
    int64_t l = strtol(s, &end, 10);

    if (errno == ERANGE && l == LONG_MAX)
        return STR2LLINT_OVERFLOW;
    if (errno == ERANGE && l == LONG_MIN)
        return STR2LLINT_UNDERFLOW;
    if (*end != '\0')
        return STR2LLINT_INCONVERTIBLE;
    *out = l;
    return STR2LLINT_SUCCESS;
}

enum bst_type {
    ST = (1u << 1),
    GRWL = (1u << 2),
    LRWL = (1u << 3),
};

enum test_strat {
    // Insert only
    INSERT = (1u << 1),

    // Random inserts, deletes
    WRITE = (1u << 2),

    // Random inserts, deletes and rebalance
    WRITEB = (1u << 3),

    // Random search, min, max, height and width
    READ = (1u << 4),

    // Random inserts, deletes, search, min, max, height and width
    READ_WRITE = (1u << 6),
};

typedef struct test_bst_metrics {
    size_t inserts;
    size_t searches;
    size_t mins;
    size_t maxs;
    size_t heights;
    size_t widths;
    size_t deletes;
    size_t rebalances;
} test_bst_metrics;

test_bst_metrics *bst_metrics_new() {
    test_bst_metrics *m = malloc(sizeof *m);

    m->inserts = 0;
    m->searches = 0;
    m->mins = 0;
    m->maxs = 0;
    m->heights = 0;
    m->widths = 0;
    m->deletes = 0;
    m->rebalances = 0;

    return m;
}

typedef struct test_bst_s {
    size_t operations;
    size_t start;
    int64_t *values;
    test_bst_metrics *metrics;
    void *bst;
    BST_ERROR (*add)(void *, int64_t);
    BST_ERROR (*search)(void *, int64_t);
    BST_ERROR (*min)(void *, int64_t *);
    BST_ERROR (*max)(void *, int64_t *);
    BST_ERROR (*height)(void *, int64_t *);
    BST_ERROR (*width)(void *, int64_t *);
    BST_ERROR (*delete)(void *, int64_t);
    BST_ERROR (*rebalance)(void *);
} test_bst_s;

void set_st_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(void *, int64_t))bst_st_add;
    t->search = (BST_ERROR(*)(void *, int64_t))bst_st_search;
    t->min = (BST_ERROR(*)(void *, int64_t *))bst_st_min;
    t->max = (BST_ERROR(*)(void *, int64_t *))bst_st_max;
    t->height = (BST_ERROR(*)(void *, int64_t *))bst_st_height;
    t->width = (BST_ERROR(*)(void *, int64_t *))bst_st_width;
    t->delete = (BST_ERROR(*)(void *, int64_t))bst_st_delete;
    t->rebalance = (BST_ERROR(*)(void *))bst_st_rebalance;
}

void set_mt_gmtx_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(void *, int64_t))bst_mt_grwl_add;
    t->search = (BST_ERROR(*)(void *, int64_t))bst_mt_grwl_search;
    t->min = (BST_ERROR(*)(void *, int64_t *))bst_mt_grwl_min;
    t->max = (BST_ERROR(*)(void *, int64_t *))bst_mt_grwl_max;
    t->height = (BST_ERROR(*)(void *, int64_t *))bst_mt_grwl_height;
    t->width = (BST_ERROR(*)(void *, int64_t *))bst_mt_grwl_width;
    t->delete = (BST_ERROR(*)(void *, int64_t))bst_mt_grwl_delete;
    t->rebalance = (BST_ERROR(*)(void *))bst_mt_grwl_rebalance;
}

void set_mt_lrwl_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(void *, int64_t))bst_mt_lrwl_add;
    t->search = (BST_ERROR(*)(void *, int64_t))bst_mt_lrwl_search;
    t->min = (BST_ERROR(*)(void *, int64_t *))bst_mt_lrwl_min;
    t->max = (BST_ERROR(*)(void *, int64_t *))bst_mt_lrwl_max;
    t->height = (BST_ERROR(*)(void *, int64_t *))bst_mt_lrwl_height;
    t->width = (BST_ERROR(*)(void *, int64_t *))bst_mt_lrwl_width;
    t->delete = (BST_ERROR(*)(void *, int64_t))bst_mt_lrwl_delete;
    t->rebalance = (BST_ERROR(*)(void *))bst_mt_lrwl_rebalance;
}

void *bst_st_test_insert_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    size_t operations = data->operations;
    size_t start = data->start;
    int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    for (int64_t i = 0; i < operations; i++) {
        if ((data->add(data->bst, values[start + i]) & SUCCESS) != SUCCESS) {
            PANIC("Failed to add element");
        }
        metrics->inserts++;
    }

    return NULL;
}

void *bst_st_test_write_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    size_t operations = data->operations;
    size_t start = data->start;
    int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (int64_t i = 0; i < operations; i++) {
        int op = i < 3 ? 0 : rand_r(&seed) % 2;

        if (op == 0) {
            if ((data->add(data->bst, values[start + i]) & SUCCESS) != SUCCESS) {
                PANIC("Failed to add element");
            }
            metrics->inserts++;
        } else {
            BST_ERROR be =
                data->delete (data->bst, values[start + rand_r(&seed) % i]);
            if ((be & SUCCESS) != SUCCESS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT &&
                (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to delete element");
            }
            metrics->deletes++;
        }
    }

    return NULL;
}

void *bst_st_test_writeb_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    size_t operations = data->operations;
    size_t start = data->start;
    int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (int64_t i = 0; i < operations; i++) {
        int op = i < 3 ? 0 : rand_r(&seed) % 3;

        if (op == 0) {
            if ((data->add(data->bst, values[start + i]) & SUCCESS) != SUCCESS) {
                PANIC("Failed to add element");
            }
            metrics->inserts++;
        } else if (op == 1) {
            BST_ERROR be =
                data->delete (data->bst, values[start + rand_r(&seed) % i]);
            if ((be & SUCCESS) != SUCCESS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT &&
                (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to delete element");
            }
            metrics->deletes++;
        } else {
            BST_ERROR be = data->rebalance(data->bst);

            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to rebalance");
            }

            metrics->rebalances++;
        }
    }

    return NULL;
}

void *bst_st_test_read_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_st_t *bst = (bst_st_t *)data->bst;
    size_t operations = data->operations;
    size_t start = data->start;
    int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (int64_t i = 0; i < operations; i++) {
        int op = rand_r(&seed) % 5;

        if (op == 0) {
            BST_ERROR be = data->search(
                bst, values[start + rand_r(&seed) % (i != 0 ? i : 1)]);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY &&
                (be & VALUE_EXISTS) != VALUE_EXISTS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT) {
                PANIC("Failed to search element");
            }
            metrics->searches++;
        } else if (op == 1) {
            BST_ERROR be = data->min(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST min");
            }
            metrics->mins++;
        } else if (op == 2) {
            BST_ERROR be = data->max(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST max");
            }
            metrics->maxs++;
        } else if (op == 3) {
            BST_ERROR be = data->height(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST height");
            }
            metrics->heights++;
        } else {
            BST_ERROR be = data->width(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST height");
            }
            metrics->widths++;
        }
    }

    return NULL;
}

void *bst_st_test_read_write_thread(void *vargp) {
    test_bst_s *data = (test_bst_s *)vargp;
    bst_st_t *bst = (bst_st_t *)data->bst;
    size_t operations = data->operations;
    size_t start = data->start;
    int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (int64_t i = 0; i < operations; i++) {
        int op = i < 3 ? 0 : rand_r(&seed) % 8;

        if (op == 0) {
            if ((data->add(bst, values[start + i]) & SUCCESS) != SUCCESS) {
                PANIC("Failed to add element");
            }
            metrics->inserts++;
        } else if (op == 1) {
            BST_ERROR be =
                data->delete (bst, values[start + rand_r(&seed) % i]);
            if ((be & SUCCESS) != SUCCESS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT &&
                (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to delete element");
            }
            metrics->deletes++;
        } else if (op == 2) {
            BST_ERROR be = data->rebalance(bst);

            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to rebalance");
            }
            metrics->rebalances++;
        } else if (op == 3) {
            BST_ERROR be = data->search(bst, values[start + rand_r(&seed) % i]);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY &&
                (be & VALUE_EXISTS) != VALUE_EXISTS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT) {
                PANIC("Failed to search element");
            }
            metrics->searches++;
        } else if (op == 4) {
            BST_ERROR be = data->min(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY &&
                (be & VALUE_EXISTS) != VALUE_EXISTS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT) {
                PANIC("Failed to find BST min");
            }
            metrics->mins++;
        } else if (op == 5) {
            BST_ERROR be = data->max(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST max");
            }
            metrics->maxs++;
        } else if (op == 6) {
            BST_ERROR be = data->height(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST height");
            }
            metrics->heights++;
        } else {
            BST_ERROR be = data->width(bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST height");
            }
            metrics->widths++;
        }
    }

    return NULL;
}

void bst_test(int64_t operations, size_t threads, enum bst_type bt,
              enum test_strat strat, size_t repeat, int64_t *values) {
    char *bst_type = NULL;
    char *strat_type = NULL;

    void *(*function)(void *) = NULL;

    switch (bt) {
    case ST:
        bst_type = "ST";
        break;
    case GRWL:
        bst_type = "GRWL";
        break;
    case LRWL:
        bst_type = "LRWL";
        break;
    }

    switch (strat) {
    case INSERT:
        strat_type = "INSERT";
        function = bst_st_test_insert_thread;
        break;
    case WRITE:
        strat_type = "WRITE";
        function = bst_st_test_write_thread;
        break;
    case WRITEB:
        strat_type = "WRITEB";
        function = bst_st_test_writeb_thread;
        break;
    case READ:
        strat_type = "READ";
        function = bst_st_test_read_thread;
        break;
    case READ_WRITE:
        strat_type = "READ_WRITE";
        function = bst_st_test_read_write_thread;
        break;
    }

    size_t ti = operations / threads;
    size_t tr = operations % threads;

    test_bst_s t_data[threads];

    for (size_t i = 0; i < threads; i++) {
        test_bst_s t;
        t.operations = ti;
        t.start = i * ti;
        t.values = values;
        t.metrics = bst_metrics_new();
        switch (bt) {
        case ST:
            set_st_functions(&t);
            break;
        case GRWL:
            set_mt_gmtx_functions(&t);
            break;
        case LRWL:
            set_mt_lrwl_functions(&t);
            break;
        }

        t_data[i] = t;
    }

    t_data[threads - 1].operations += tr;

    for (int64_t r = 0; r < repeat; r++) {
        struct timeval start, end;

        void *bst = NULL;

        int add_elements = strat == READ ? 1 : 0;

        switch (bt) {
        case ST:
            bst = bst_st_new(NULL);
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_st_add(bst, values[i]);
                }
            }
            break;
        case GRWL:
            bst = bst_mt_grwl_new(NULL);
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_mt_grwl_add(bst, values[i]);
                }
            }
            break;
        case LRWL:
            bst = bst_mt_lrwl_new(NULL);
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_mt_lrwl_add(bst, values[i]);
                }
            }
            break;
        }

        for (size_t i = 0; i < threads; i++) {
            t_data[i].bst = bst;
        }

        gettimeofday(&start, NULL);
        pthread_t thread[threads];
        for (int i = 0; i < threads; i++) {
            if (pthread_create(&thread[i], NULL, function, &t_data[i])) {
                PANIC("pthread_create() failure");
            };
        }

        for (int i = 0; i < threads; i++) {
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
        case GRWL:
            bst_mt_grwl_node_count(bst, &nc);
            bst_mt_grwl_min(bst, &min);
            bst_mt_grwl_max(bst, &max);
            bst_mt_grwl_height(bst, &height);
            bst_mt_grwl_width(bst, &width);
            bst_mt_grwl_free(bst);
            break;
        case LRWL:
            bst_mt_lrwl_node_count(bst, &nc);
            bst_mt_lrwl_min(bst, &min);
            bst_mt_lrwl_max(bst, &max);
            bst_mt_lrwl_height(bst, &height);
            bst_mt_lrwl_width(bst, &width);
            bst_mt_lrwl_free(bst);
            break;
        }

        size_t inserts = 0;
        size_t searches = 0;
        size_t mins = 0;
        size_t maxs = 0;
        size_t heights = 0;
        size_t widths = 0;
        size_t deletes = 0;
        size_t rebalances = 0;

        for (size_t i = 0; i < threads; i++) {
            inserts += t_data[i].metrics->inserts;
            searches += t_data[i].metrics->searches;
            mins += t_data[i].metrics->mins;
            maxs += t_data[i].metrics->maxs;
            heights += t_data[i].metrics->heights;
            widths += t_data[i].metrics->widths;
            deletes += t_data[i].metrics->deletes;
            rebalances += t_data[i].metrics->rebalances;
        }

        printf("%s,", bst_type);
        printf("%s,", strat_type);
        printf("%zu,", threads);
        printf("%ld,", nc);
        printf("%ld,", min);
        printf("%ld,", max);
        printf("%ld,", height);
        printf("%ld,", width);
        printf("%f,", time_taken);
        printf("%ld,", inserts);
        printf("%ld,", searches);
        printf("%ld,", mins);
        printf("%ld,", maxs);
        printf("%ld,", heights);
        printf("%ld,", widths);
        printf("%ld,", deletes);
        printf("%ld\n", rebalances);

        for (size_t i = 0; i < threads; i++) {
            t_data[i].metrics->inserts = 0;
            t_data[i].metrics->searches = 0;
            t_data[i].metrics->mins = 0;
            t_data[i].metrics->maxs = 0;
            t_data[i].metrics->heights = 0;
            t_data[i].metrics->widths = 0;
            t_data[i].metrics->deletes = 0;
            t_data[i].metrics->rebalances = 0;
        }
    }
}

int main(int argc, char **argv) {
    int64_t operations = 0, threads = 1, repeat = 1;
    enum bst_type type = 0;
    enum test_strat strat = 0;

    opterr = 0;

    int c;
    while ((c = getopt(argc, argv, "ho:t:r:s:glc")) != -1)
        switch (c) {
        case 'h':
            fprintf(stdout, "%s", usage());
            exit(0);
        case 'o':
            if (str2int(&operations, optarg) != STR2LLINT_SUCCESS) {
                PANIC("Invalid value for option -o");
            }
            break;
        case 't':
            if (str2int(&threads, optarg) != STR2LLINT_SUCCESS) {
                PANIC("Invalid value for option -t");
            }
            break;
        case 'r':
            if (str2int(&repeat, optarg) != STR2LLINT_SUCCESS) {
                PANIC("Invalid value for option -r");
            }
            break;
        case 's':
            if (strncmp(optarg, "insert", 6) == 0) {
                strat = strat | INSERT;
                break;
            }
            if (strncmp(optarg, "writeb", 6) == 0) {
                strat = strat | WRITEB;
                break;
            }
            if (strncmp(optarg, "write", 5) == 0) {
                strat = strat | WRITE;
                break;
            }
            if (strncmp(optarg, "read_write", 10) == 0) {
                strat = strat | READ_WRITE;
                break;
            }

            if (strncmp(optarg, "read", 4) == 0) {
                strat = strat | READ;
                break;
            }

            PANIC("Invalid value for option -s");
        case 'g':
            type = type | GRWL;
            break;
        case 'l':
            type = type | LRWL;
            break;
        case 'c':
            type = type | ST;
            break;
        case '?':
            if (optopt == 'o') {
                PANIC("Option -o requires an argument.");
            } else if (optopt == 't') {
                PANIC("Option -t requires an argument.");
            } else if (optopt == 'r') {
                PANIC("Option -r requires an argument.");
            } else if (optopt == 's') {
                PANIC("Option -s requires an argument.");
            } else if (isprint(optopt)) {
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                exit(1);
            } else {
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                exit(1);
            }
        default:
            abort();
        }

    if (operations == 0) {
        PANIC("Number of operations not set.")
    }

    if (type == 0) {
        PANIC("BST type not set.")
    }

    if (strat == 0) {
        PANIC("Test strategy type not set.")
    }

    int64_t *values = malloc(sizeof *values * operations);

    for (int64_t i = 0; i < operations; i++) {
        values[i] = i;
    }

    fisher_yates_shuffle(operations, values);

    // Execute possible combinations per strat
    if ((type & ST) == ST && (strat & INSERT) == INSERT) {
        bst_test(operations, 1, ST, INSERT, repeat, values);
    }

    if ((type & ST) == ST && (strat & WRITE) == WRITE) {
        bst_test(operations, 1, ST, WRITE, repeat, values);
    }

    if ((type & ST) == ST && (strat & WRITEB) == WRITEB) {
        bst_test(operations, 1, ST, WRITEB, repeat, values);
    }

    if ((type & ST) == ST && (strat & READ) == READ) {
        bst_test(operations, 1, ST, READ, repeat, values);
    }

    if ((type & ST) == ST && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, 1, ST, READ_WRITE, repeat, values);
    }

    if ((type & GRWL) == GRWL && (strat & INSERT) == INSERT) {
        bst_test(operations, threads, GRWL, INSERT, repeat, values);
    }

    if ((type & GRWL) == GRWL && (strat & WRITE) == WRITE) {
        bst_test(operations, threads, GRWL, WRITE, repeat, values);
    }

    if ((type & GRWL) == GRWL && (strat & WRITEB) == WRITEB) {
        bst_test(operations, threads, GRWL, WRITEB, repeat, values);
    }

    if ((type & GRWL) == GRWL && (strat & READ) == READ) {
        bst_test(operations, threads, GRWL, READ, repeat, values);
    }

    if ((type & GRWL) == GRWL && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, 1, GRWL, READ_WRITE, repeat, values);
    }

    if ((type & LRWL) == LRWL && (strat & INSERT) == INSERT) {
        bst_test(operations, threads, LRWL, INSERT, repeat, values);
    }

    if ((type & LRWL) == LRWL && (strat & WRITE) == WRITE) {
        bst_test(operations, threads, LRWL, WRITE, repeat, values);
    }

    if ((type & LRWL) == LRWL && (strat & WRITEB) == WRITEB) {
        bst_test(operations, threads, LRWL, WRITEB, repeat, values);
    }

    if ((type & LRWL) == LRWL && (strat & READ) == READ) {
        bst_test(operations, threads, LRWL, READ, repeat, values);
    }

    if ((type & LRWL) == LRWL && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, 1, LRWL, READ_WRITE, repeat, values);
    }

    return 0;
}
