#include "cache.h"
#include "memory.h"
#include <string.h>

struct cache_t *
cache_create(char *name, int nsets, int bsize, int assoc,
             enum cache_policy policy, enum write_policy wpolicy)
{
    struct cache_t *cp;
    int i;

    cp = (struct cache_t *)calloc(1, sizeof(struct cache_t));

    cp->name = strdup(name);
    cp->nsets = nsets;
    cp->bsize = bsize;
    cp->assoc = assoc;
    cp->policy = policy;
    cp->wpolicy = wpolicy;

    cp->sets = (cache_blk_t **)calloc(nsets, sizeof(cache_blk_t *));
    for (i = 0; i < nsets; i++)
        cp->sets[i] = (cache_blk_t *)calloc(assoc, sizeof(cache_blk_t));

    return cp;
}

/* ---- 교체 블록 선택 ---- */
static cache_blk_t *
cache_find_repl(struct cache_t *cp, int set)
{
    /* 단순 FIFO/LRU/Random 구현 (원본 유지) */
    int way = 0;
    if (cp->policy == Random)
        way = rand() % cp->assoc;

    return &cp->sets[set][way];
}


/* ---- 캐시 접근 ---- */
unsigned int
cache_access(struct cache_t *cp,
             enum mem_cmd cmd,
             md_addr_t addr,
             byte_t *p,
             int nbytes,
             tick_t now)
{
    int set = (addr / cp->bsize) % cp->nsets;
    md_addr_t tag = addr / (cp->bsize * cp->nsets);
    cache_blk_t *blk = NULL;
    int way;

    /* ---- HIT 검사 ---- */
    for (way = 0; way < cp->assoc; way++) {
        blk = &cp->sets[set][way];

        if ((blk->status & CACHE_BLK_VALID) && blk->tag == tag) {
            /* HIT */
            cp->hits++;

            if (cmd == Write) {
                if (cp->wpolicy == WRITE_BACK_ALLOC) {
                    /* 기존 방식: dirty bit set */
                    blk->status |= CACHE_BLK_DIRTY;
                }
                else if (cp->wpolicy == WRITE_THROUGH_NO_ALLOC) {
                    /* L1 Write-through: 메모리에 즉시 반영 */
                    mem_access(Write, addr, p, nbytes);
                }
            }
            return 1;
        }
    }

    /* ---- MISS ---- */
    cp->misses++;

    /* L1 정책 처리 */
    if (cp->wpolicy == WRITE_THROUGH_NO_ALLOC && cmd == Write) {
        /* Write miss → no-write-allocate → 메모리에 바로 write */
        mem_access(Write, addr, p, nbytes);
        return 0;
    }

    /* ---- Write-back 또는 Write-allocate ---- */
    /* 블록 교체 */
    blk = cache_find_repl(cp, set);

    if ((blk->status & CACHE_BLK_VALID) && (blk->status & CACHE_BLK_DIRTY)) {
        /* writeback */
        cp->writebacks++;
        mem_access(Write, (blk->tag * cp->nsets + set) * cp->bsize,
                   NULL, cp->bsize);
    }

    /* 새 블록 로드 */
    blk->tag = tag;
    blk->status = CACHE_BLK_VALID;

    mem_access(Read, addr, NULL, cp->bsize);

    if (cmd == Write) {
        if (cp->wpolicy == WRITE_BACK_ALLOC)
            blk->status |= CACHE_BLK_DIRTY;
        else
            mem_access(Write, addr, p, nbytes);
    }

    return 0;
}

