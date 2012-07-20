/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// T(A) gets W(L)
// T(B) tries W(L), gets DB_LOCK_NOTGRANTED
// T(A) releases locks
// T(B) gets W(L)
// T(B) releases locks

#include "test.h"
#include <inttypes.h>

static int write_lock(toku_lock_tree *lt, TXNID txnid, const char *k) {
    DBT key; dbt_init(&key, k, strlen(k));
    toku_lock_request lr;
    toku_lock_request_init(&lr, txnid, &key, &key, LOCK_REQUEST_WRITE);
    int r;
    if (0) {
        r = toku_lt_acquire_lock_request_with_timeout(lt, &lr, NULL);
    } else {
        r = toku_lock_request_start(&lr, lt, true);
        if (r == DB_LOCK_NOTGRANTED)
            r = toku_lock_request_wait_with_default_timeout(&lr, lt);
    }
    toku_lock_request_destroy(&lr);
    return r;
}

struct writer_arg {
    TXNID id;
    toku_lock_tree *lt;
    const char *name;
};

static void *writer_thread(void *arg) {
    struct writer_arg *writer_arg = (struct writer_arg *) arg;
    printf("%" PRIu64 " wait\n", writer_arg->id);
    int r = write_lock(writer_arg->lt, writer_arg->id, writer_arg->name); assert(r == 0);
    printf("%" PRIu64 " locked\n", writer_arg->id);
    sleep(1);
    toku_lt_unlock_txn(writer_arg->lt, writer_arg->id);
    printf("%" PRIu64 " unlocked\n", writer_arg->id);
    return arg;
}

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 1;
    uint64_t max_lock_memory = 4096;
    int max_threads = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        if (strcmp(argv[i], "--max_threads") == 0 && i+1 < argc) {
            max_threads = atoi(argv[++i]);
            continue;
        }        
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic);
    assert(r == 0 && ltm);
    toku_ltm_set_lock_wait_time(ltm, UINT64_MAX);

    toku_lock_tree *lt = NULL;
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, NULL, dbcmp, NULL, NULL, NULL);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;

    r = write_lock(lt, txn_a, "L"); assert(r == 0);
    printf("main locked\n");

    toku_pthread_t tids[max_threads];
    for (int i = 0 ; i < max_threads; i++) {
        struct writer_arg *writer_arg = (struct writer_arg *) toku_malloc(sizeof (struct writer_arg));
        *writer_arg = (struct writer_arg) { (TXNID) i+10, lt, "L"};
        r = toku_pthread_create(&tids[i], NULL, writer_thread, writer_arg); assert(r == 0);
    }
    sleep(10);
    r = toku_lt_unlock_txn(lt, txn_a);  assert(r == 0);
    printf("main unlocked\n");

    for (int i = 0; i < max_threads; i++) {
        void *retarg;
        r = toku_pthread_join(tids[i], &retarg); assert(r == 0);
        toku_free(retarg);
    }

    // shutdown 
    toku_lt_remove_db_ref(lt);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}