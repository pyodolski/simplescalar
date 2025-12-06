#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include "host.h"
#include "misc.h"
#include "machine.h"

enum cache_policy {
    LRU,
    Random,
    FIFO
};

/* 추가: Write policy 정의 */
enum write_policy {
    WRITE_BACK_ALLOC,         /* 기존 기본 동작 */
    WRITE_THROUGH_NO_ALLOC    /* L1에서 사용 */
};

typedef struct cache_blk_t {
    md_addr_t tag;
    unsigned int status;
    tick_t ready;
} cache_blk_t;

struct cache_t {
    char *name;

    int nsets;
    int bsize;
    int assoc;
    enum cache_policy policy;

    /* 추가: write policy */
    enum write_policy wpolicy;

    cache_blk_t **sets;

    int hits, misses, replacements, writebacks;
};

struct cache_t *cache_create(char *name, int nsets, int bsize,
                             int assoc, enum cache_policy policy,
                             enum write_policy wpolicy);

unsigned int cache_access(struct cache_t *cp,
                          enum mem_cmd cmd,
                          md_addr_t addr,
                          byte_t *p,
                          int nbytes,
                          tick_t now);

#endif

