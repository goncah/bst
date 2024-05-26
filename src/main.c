#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "bst_at/include/bst_at.h"
#include "bst_mt_cgl/include/bst_mt_cgl.h"
#include "bst_mt_fgl/include/bst_mt_fgl.h"
#include "bst_st/include/bst_st.h"

const char *usage() {
    const char *msg = "\
Usage:\n\
    $ bst <options>\n\
\n\
Options:\n\
\t-n Set the number of operations\n\
\t-o Set write probability over read operations. Ex 60 for 60% inserts\n\
\t\tThis option is ignored for insert, write and read strategies.\n\
\t-t Set the number of threads, operations are evenly distributed. Ignored for BST ST type.\n\
\t-r Set the number of test repetitions, applies to each strat and each BST type\n\
\t-s <strategy> Set the test strategy. Multiple strategies can be set, example -s 1 -s 2 -s 3. Available strategies are:\n\
\t\tinsert     - Inserts only with random generated numbers\n\
\t\twrite      - Random inserts, deletes with random generated numbers\n\
\t\twriteb     - Random inserts, deletes and rebalance with random generated numbers\n\
\t\tread       - Random search, min, max, height and width. -o sets the number of elements in the read.\n\
\t\tread_write - Random inserts, deletes, search, min, max, height and width with random generated numbers.\n\
\t-a Set the BST type to Atomic, can be set with -c, -g and -l to test multiple BST types\n\
\t-c Set the BST type to ST, can be set with -a, -g and -l to test multiple BST types\n\
\t-g Set the BST type to MT Coarse-Grained Lock, can be set with -a, -c and -l to test multiple BST types\n\
\t-l Set the BST type to MT Fine-Grained Lock, can be set with -a, -c and -g to test multiple BST types\n\
\t-m <#> Sets the memory order for atomic operations, default is 5 \"memory_order_seq_cst\", possible values are: \n\
\t\t0 - memory_order_relaxed, \n\
\t\t1 - memory_order_consume, \n\
\t\t2 - memory_order_acquire, \n\
\t\t3 - memory_order_release,\n\
\t\t4 - memory_order_acq_rel,\n\
\t\t5 - memory_order_seq_cst\n\
    \n";

    return msg;
}

memory_order mo = memory_order_seq_cst;

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
    const int64_t t = *b;

    *b = *a;
    *a = t;
}

// https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
// shuffle an array of signed long integers
void fisher_yates_shuffle(const size_t n, int64_t *a) {
    srand(mix(clock(), time(NULL), getpid()));

    for (size_t i = n - 1; i > 0; i--) {
        const size_t j = rand() % i;

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
str2int_errno str2int(int64_t *out, const char *s) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2LLINT_INCONVERTIBLE;
    errno = 0;
    const int64_t l = strtol(s, &end, 10);

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
    CGL = (1u << 2),
    FGL = (1u << 3),
    AT = (1u << 4),
};

enum test_strat {
    // Insert only
    INSERT = (1u << 1),

    // Random inserts, deletes
    WRITE = (1u << 2),

    // Random search, min, max, height and width
    READ = (1u << 3),

    // Random inserts, deletes, search, min, max, height and width
    READ_WRITE = (1u << 4),
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
    float write_prob;
    void *bst;
    BST_ERROR (*add)(const void **, int64_t);
    BST_ERROR (*search)(const void **, int64_t);
    BST_ERROR (*min)(const void **, int64_t *);
    BST_ERROR (*max)(const void **, int64_t *);
    BST_ERROR (*height)(const void **, int64_t *);
    BST_ERROR (*width)(const void **, int64_t *);
    BST_ERROR (*delete)(const void **, int64_t);
    BST_ERROR (*rebalance)(const void **);
} test_bst_s;

void set_st_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(const void **, int64_t))bst_st_add;
    t->search = (BST_ERROR(*)(const void **, int64_t))bst_st_search;
    t->min = (BST_ERROR(*)(const void **, int64_t *))bst_st_min;
    t->max = (BST_ERROR(*)(const void **, int64_t *))bst_st_max;
    t->height = (BST_ERROR(*)(const void **, int64_t *))bst_st_height;
    t->width = (BST_ERROR(*)(const void **, int64_t *))bst_st_width;
    t->delete = (BST_ERROR(*)(const void **, int64_t))bst_st_delete;
    t->rebalance = (BST_ERROR(*)(const void **))bst_st_rebalance;
}

void set_mt_cgl_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(const void **, int64_t))bst_mt_cgl_add;
    t->search = (BST_ERROR(*)(const void **, int64_t))bst_mt_cgl_search;
    t->min = (BST_ERROR(*)(const void **, int64_t *))bst_mt_cgl_min;
    t->max = (BST_ERROR(*)(const void **, int64_t *))bst_mt_cgl_max;
    t->height = (BST_ERROR(*)(const void **, int64_t *))bst_mt_cgl_height;
    t->width = (BST_ERROR(*)(const void **, int64_t *))bst_mt_cgl_width;
    t->delete = (BST_ERROR(*)(const void **, int64_t))bst_mt_cgl_delete;
    t->rebalance = (BST_ERROR(*)(const void **))bst_mt_cgl_rebalance;
}

void set_mt_fgl_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(const void **, int64_t))bst_mt_fgl_add;
    t->search = (BST_ERROR(*)(const void **, int64_t))bst_mt_fgl_search;
    t->min = (BST_ERROR(*)(const void **, int64_t *))bst_mt_fgl_min;
    t->max = (BST_ERROR(*)(const void **, int64_t *))bst_mt_fgl_max;
    t->height = (BST_ERROR(*)(const void **, int64_t *))bst_mt_fgl_height;
    t->width = (BST_ERROR(*)(const void **, int64_t *))bst_mt_fgl_width;
    t->delete = (BST_ERROR(*)(const void **, int64_t))bst_mt_fgl_delete;
    t->rebalance = (BST_ERROR(*)(const void **))bst_mt_fgl_rebalance;
}

void set_at_functions(test_bst_s *t) {
    t->add = (BST_ERROR(*)(const void **, int64_t))bst_at_add;
    t->search = (BST_ERROR(*)(const void **, int64_t))bst_at_search;
    t->min = (BST_ERROR(*)(const void **, int64_t *))bst_at_min;
    t->max = (BST_ERROR(*)(const void **, int64_t *))bst_at_max;
    t->height = (BST_ERROR(*)(const void **, int64_t *))bst_at_height;
    t->width = (BST_ERROR(*)(const void **, int64_t *))bst_at_width;
    t->delete = (BST_ERROR(*)(const void **, int64_t))bst_at_delete;
    t->rebalance = (BST_ERROR(*)(const void **))bst_at_rebalance;
}

void *bst_st_test_insert_thread(void *vargp) {
    const test_bst_s *data = (test_bst_s *)vargp;
    const size_t operations = data->operations;
    const size_t start = data->start;
    const int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    for (size_t i = 0; i < operations; i++) {
        if ((data->add((const void **)&data->bst, values[start + i]) &
             SUCCESS) != SUCCESS) {
            PANIC("Failed to add element");
        }
        metrics->inserts++;
    }

    return NULL;
}

void *bst_st_test_write_thread(void *vargp) {
    const test_bst_s *data = (test_bst_s *)vargp;
    const size_t operations = data->operations;
    const size_t start = data->start;
    const int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (size_t i = 0; i < operations; i++) {
        const int op = i < 3 ? 0 : rand_r(&seed) % 2;

        if (op == 0) {
            if ((data->add((const void **)&data->bst, values[start + i]) &
                 SUCCESS) != SUCCESS) {
                PANIC("Failed to add element");
            }
            metrics->inserts++;
        } else {
            const BST_ERROR be = data->delete (
                (const void **)&data->bst, values[start + rand_r(&seed) % i]);
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

void *bst_st_test_read_thread(void *vargp) {
    const test_bst_s *data = (test_bst_s *)vargp;
    const size_t operations = data->operations;
    const size_t start = data->start;
    const int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (size_t i = 0; i < operations; i++) {
        const int op = rand_r(&seed) % 5;

        if (op == 0) {
            const BST_ERROR be =
                data->search((const void **)&data->bst,
                             values[start + rand_r(&seed) % (i != 0 ? i : 1)]);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY &&
                (be & VALUE_EXISTS) != VALUE_EXISTS &&
                (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT) {
                PANIC("Failed to search element");
            }
            metrics->searches++;
        } else if (op == 1) {
            const BST_ERROR be = data->min((const void **)&data->bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST min");
            }
            metrics->mins++;
        } else if (op == 2) {
            const BST_ERROR be = data->max((const void **)&data->bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST max");
            }
            metrics->maxs++;
        } else if (op == 3) {
            const BST_ERROR be = data->height((const void **)&data->bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST height");
            }
            metrics->heights++;
        } else {
            const BST_ERROR be = data->width((const void **)&data->bst, NULL);
            if ((be & SUCCESS) != SUCCESS && (be & BST_EMPTY) != BST_EMPTY) {
                PANIC("Failed to find BST height");
            }
            metrics->widths++;
        }
    }

    return NULL;
}

void *bst_st_test_read_write_thread(void *vargp) {
    const test_bst_s *data = (test_bst_s *)vargp;
    const size_t operations = data->operations;
    const size_t start = data->start;
    const int64_t *values = data->values;
    test_bst_metrics *metrics = data->metrics;

    uint seed = mix(clock(), time(NULL), getpid());

    for (size_t i = 0; i < operations; i++) {

        const int prob = data->write_prob == 0   ? 0
                         : data->write_prob == 1 ? 1
                         : rand_r(&seed) < (int)(data->write_prob * RAND_MAX)
                             ? 1
                             : 0;

        const int op = i < 3 ? 0 : rand_r(&seed) % 7;

        if (prob) {
            if (op == 0) {
                if ((data->add((const void **)&data->bst, values[start + i]) &
                     SUCCESS) != SUCCESS) {
                    PANIC("Failed to add element");
                }
                metrics->inserts++;
            } else {
                const BST_ERROR be =
                    data->delete ((const void **)&data->bst,
                                  values[start + rand_r(&seed) % i]);
                if ((be & SUCCESS) != SUCCESS &&
                    (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT &&
                    (be & BST_EMPTY) != BST_EMPTY) {
                    PANIC("Failed to delete element");
                }
                metrics->deletes++;
            }
        } else {
            if (op == 2) {
                const BST_ERROR be =
                    data->search((const void **)&data->bst,
                                 values[start + rand_r(&seed) % i]);
                if ((be & SUCCESS) != SUCCESS &&
                    (be & BST_EMPTY) != BST_EMPTY &&
                    (be & VALUE_EXISTS) != VALUE_EXISTS &&
                    (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT) {
                    PANIC("Failed to search element");
                }
                metrics->searches++;
            } else if (op == 3) {
                const BST_ERROR be = data->min((const void **)&data->bst, NULL);
                if ((be & SUCCESS) != SUCCESS &&
                    (be & BST_EMPTY) != BST_EMPTY &&
                    (be & VALUE_EXISTS) != VALUE_EXISTS &&
                    (be & VALUE_NONEXISTENT) != VALUE_NONEXISTENT) {
                    PANIC("Failed to find BST min");
                }
                metrics->mins++;
            } else if (op == 4) {
                const BST_ERROR be = data->max((const void **)&data->bst, NULL);
                if ((be & SUCCESS) != SUCCESS &&
                    (be & BST_EMPTY) != BST_EMPTY) {
                    PANIC("Failed to find BST max");
                }
                metrics->maxs++;
            } else if (op == 5) {
                const BST_ERROR be =
                    data->height((const void **)&data->bst, NULL);
                if ((be & SUCCESS) != SUCCESS &&
                    (be & BST_EMPTY) != BST_EMPTY) {
                    PANIC("Failed to find BST height");
                }
                metrics->heights++;
            } else {
                const BST_ERROR be =
                    data->width((const void **)&data->bst, NULL);
                if ((be & SUCCESS) != SUCCESS &&
                    (be & BST_EMPTY) != BST_EMPTY) {
                    PANIC("Failed to find BST height");
                }
                metrics->widths++;
            }
        }
    }

    return NULL;
}

void bst_test(const int64_t operations, const size_t threads,
              const enum bst_type bt, const enum test_strat strat,
              const size_t repeat, int64_t *values, float write_prob) {
    char *bst_type = NULL;
    char *strat_type = NULL;

    void *(*function)(void *) = NULL;

    switch (bt) {
    case ST:
        bst_type = "ST";
        break;
    case CGL:
        bst_type = "CGL";
        break;
    case FGL:
        bst_type = "FGL";
        break;
    case AT:
        bst_type = "AT";
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
    case READ:
        strat_type = "READ";
        function = bst_st_test_read_thread;
        break;
    case READ_WRITE:
        strat_type = "READ_WRITE";
        function = bst_st_test_read_write_thread;
        break;
    }

    const size_t ti = operations / threads;
    const size_t tr = operations % threads;

    test_bst_s t_data[threads];

    for (size_t i = 0; i < threads; i++) {
        test_bst_s *t = &t_data[i];
        t->operations = ti;
        t->start = i * ti;
        t->values = values;
        t->write_prob = write_prob;
        t->metrics = bst_metrics_new();
        switch (bt) {
        case ST:
            set_st_functions(t);
            break;
        case CGL:
            set_mt_cgl_functions(t);
            break;
        case FGL:
            set_mt_fgl_functions(t);
            break;
        case AT:
            set_at_functions(t);
            break;
        }

        if (i + 1 >= threads) {
            t->operations += tr;
        }
    }

    for (int64_t r = 0; r < repeat; r++) {
        struct timeval start, end;

        const void *bst = NULL;
        const void *bst__ = NULL;

        const int add_elements = strat == READ ? 1 : 0;

        switch (bt) {
        case ST:
            bst = bst_st_new(NULL);
            bst__ = &bst;
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_st_add((bst_st_t **)bst__, values[i]);
                }
            }
            break;
        case CGL:
            bst = bst_mt_cgl_new(NULL);
            bst__ = &bst;
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_mt_cgl_add((bst_mt_cgl_t **)bst__, values[i]);
                }
            }
            break;
        case FGL:
            bst = bst_mt_fgl_new(NULL);
            bst__ = &bst;
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_mt_fgl_add((bst_mt_fgl_t **)bst__, values[i]);
                }
            }
            break;
        case AT:
            bst = bst_at_new(mo, NULL);
            bst__ = &bst;
            if (add_elements) {
                for (int i = 0; i < operations; i++) {
                    bst_at_add((bst_at_t **)bst__, values[i]);
                }
            }
            break;
        }

        for (size_t i = 0; i < threads; i++) {
            t_data[i].bst = (void *)bst;
        }

        gettimeofday(&start, NULL);
        pthread_t thread[threads];
        for (int i = 0; i < threads; i++) {
            if (pthread_create(&thread[i], NULL, function, &t_data[i])) {
                PANIC("pthread_create() failure");
            }
        }

        for (int i = 0; i < threads; i++) {
            if (pthread_join(thread[i], NULL)) {
                PANIC("pthread_join() failure");
            }
        }

        gettimeofday(&end, NULL);
        const double time_taken =
            end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6;

        size_t nc = 0, height = 0, width = 0;
        int64_t min = 0, max = 0;

        switch (bt) {
        case ST:
            nc = ((bst_st_t *)bst)->count;
            bst_st_min((bst_st_t **)bst__, &min);
            bst_st_max((bst_st_t **)bst__, &max);
            bst_st_height((bst_st_t **)bst__, &height);
            bst_st_width((bst_st_t **)bst__, &width);
            bst_st_free((bst_st_t **)bst__);
            break;
        case CGL:
            bst_mt_cgl_node_count((bst_mt_cgl_t **)bst__, &nc);
            bst_mt_cgl_min((bst_mt_cgl_t **)bst__, &min);
            bst_mt_cgl_max((bst_mt_cgl_t **)bst__, &max);
            bst_mt_cgl_height((bst_mt_cgl_t **)bst__, &height);
            bst_mt_cgl_width((bst_mt_cgl_t **)bst__, &width);
            bst_mt_cgl_free((bst_mt_cgl_t **)bst__);
            break;
        case FGL:
            bst_mt_fgl_node_count((bst_mt_fgl_t **)bst__, &nc);
            bst_mt_fgl_min((bst_mt_fgl_t **)bst__, &min);
            bst_mt_fgl_max((bst_mt_fgl_t **)bst__, &max);
            bst_mt_fgl_height((bst_mt_fgl_t **)bst__, &height);
            bst_mt_fgl_width((bst_mt_fgl_t **)bst__, &width);
            bst_mt_fgl_free((bst_mt_fgl_t **)bst__);
            break;
        case AT:
            bst_at_node_count((bst_at_t **)bst__, &nc);
            bst_at_min((bst_at_t **)bst__, &min);
            bst_at_max((bst_at_t **)bst__, &max);
            bst_at_height((bst_at_t **)bst__, &height);
            bst_at_width((bst_at_t **)bst__, &width);
            bst_at_free((bst_at_t **)bst__);
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
        printf("%zu,", operations);
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
        fflush(stdout);

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

    for (size_t i = 0; i < threads; i++) {
        free(t_data[i].metrics);
    }
}

int main(const int argc, char **argv) {
    int64_t operations = 0, threads = 1, repeat = 1;
    float write_prob = 0.5;
    enum bst_type type = 0;
    enum test_strat strat = 0;

    opterr = 0;

    int c;
    while ((c = getopt(argc, argv, "m:hn:o:t:r:s:glca")) != -1)
        switch (c) {
        case 'h':
            fprintf(stdout, "%s", usage());
            exit(0);
        case 'm':
            size_t len = strnlen(optarg, 2);
            if (len == 2 || len != 1) {
                PANIC("Invalid value for option -m");
            }

            char ws[2] = {0};

            strncpy(ws, optarg, 1);

            errno = 0;
            intmax_t m = strtoimax(ws, NULL, 10);
            if (m == UINTMAX_MAX && errno == ERANGE) {
                PANIC("Invalid value for option -o");
            }

            if (m < 0 || m > 5) {
                PANIC("Invalid value for option -o");
            }

            mo = m;
            break;
        case 'n':
            if (str2int(&operations, optarg) != STR2LLINT_SUCCESS) {
                PANIC("Invalid value for option -n");
            }

            if (operations < 1) {
                PANIC("Invalid value for option -n");
            }

            break;
        case 'o':
            size_t lens = strnlen(optarg, 4);
            if (lens == 4 || lens < 1) {
                PANIC("Invalid value for option -o");
            }

            char wss[5] = {0};

            strncpy(wss, optarg, 3);

            errno = 0;
            const float w = strtof(wss, NULL);
            if (errno != 0) {
                PANIC("Invalid value for option -o");
            }

            write_prob = w / 100;
            break;
        case 't':
            if (str2int(&threads, optarg) != STR2LLINT_SUCCESS) {
                PANIC("Invalid value for option -t");
            }

            if (threads < 1) {
                PANIC("Invalid value for option -t");
            }

            break;
        case 'r':
            if (str2int(&repeat, optarg) != STR2LLINT_SUCCESS) {
                PANIC("Invalid value for option -r");
            }

            if (repeat < 1) {
                PANIC("Invalid value for option -r");
            }
            break;
        case 's':
            if (strncmp(optarg, "insert", 6) == 0) {
                strat = strat | INSERT;
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
            type = type | CGL;
            break;
        case 'l':
            type = type | FGL;
            break;
        case 'c':
            type = type | ST;
            break;
        case 'a':
            type = type | AT;
            break;
        case '?':
            if (optopt == 'o') {
                PANIC("Option -o requires an argument.");
            } else if (optopt == 'n') {
                PANIC("Option -n requires an argument.");
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
        bst_test(operations, 1, ST, INSERT, repeat, values, write_prob);
    }

    if ((type & ST) == ST && (strat & WRITE) == WRITE) {
        bst_test(operations, 1, ST, WRITE, repeat, values, write_prob);
    }

    if ((type & ST) == ST && (strat & READ) == READ) {
        bst_test(operations, 1, ST, READ, repeat, values, write_prob);
    }

    if ((type & ST) == ST && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, 1, ST, READ_WRITE, repeat, values, write_prob);
    }

    if ((type & CGL) == CGL && (strat & INSERT) == INSERT) {
        bst_test(operations, threads, CGL, INSERT, repeat, values, write_prob);
    }

    if ((type & CGL) == CGL && (strat & WRITE) == WRITE) {
        bst_test(operations, threads, CGL, WRITE, repeat, values, write_prob);
    }

    if ((type & CGL) == CGL && (strat & READ) == READ) {
        bst_test(operations, threads, CGL, READ, repeat, values, write_prob);
    }

    if ((type & CGL) == CGL && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, threads, CGL, READ_WRITE, repeat, values,
                 write_prob);
    }

    if ((type & FGL) == FGL && (strat & INSERT) == INSERT) {
        bst_test(operations, threads, FGL, INSERT, repeat, values, write_prob);
    }

    if ((type & FGL) == FGL && (strat & WRITE) == WRITE) {
        bst_test(operations, threads, FGL, WRITE, repeat, values, write_prob);
    }

    if ((type & FGL) == FGL && (strat & READ) == READ) {
        bst_test(operations, threads, FGL, READ, repeat, values, write_prob);
    }

    if ((type & FGL) == FGL && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, threads, FGL, READ_WRITE, repeat, values,
                 write_prob);
    }

    if ((type & AT) == AT && (strat & INSERT) == INSERT) {
        bst_test(operations, threads, AT, INSERT, repeat, values, write_prob);
    }

    if ((type & AT) == AT && (strat & WRITE) == WRITE) {
        bst_test(operations, threads, AT, WRITE, repeat, values, write_prob);
    }

    if ((type & AT) == AT && (strat & READ) == READ) {
        bst_test(operations, threads, AT, READ, repeat, values, write_prob);
    }

    if ((type & AT) == AT && (strat & READ_WRITE) == READ_WRITE) {
        bst_test(operations, threads, AT, READ_WRITE, repeat, values,
                 write_prob);
    }

    free(values);
    return 0;
}
