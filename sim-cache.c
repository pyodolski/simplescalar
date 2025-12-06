/* SimpleScalar sim-cache.c (수정 버전)
 * Hybrid Write Policy
 *  - L1: Write-Through + No-Write-Allocate
 *  - L2: Write-Back + Write-Allocate
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "misc.h"
#include "machine.h"
#include "cache.h"
#include "options.h"
#include "sim.h"
#include "eio.h"
#include "loader.h"
#include "memory.h"

/* 전역 캐시 포인터 */
struct cache_t *il1 = NULL;
struct cache_t *dl1 = NULL;
struct cache_t *il2 = NULL;
struct cache_t *dl2 = NULL;

/* 기본 설정 */
char *cache_il1_opt = NULL;
char *cache_dl1_opt = NULL;
char *cache_il2_opt = NULL;
char *cache_dl2_opt = NULL;

/* 시뮬레이터 옵션 설정 */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_string(odb, "-cache:il1", "L1 I-cache config",
		 &cache_il1_opt, "il1:512:64:2:l", TRUE, NULL);

  opt_reg_string(odb, "-cache:dl1", "L1 D-cache config",
		 &cache_dl1_opt, "dl1:512:64:2:l", TRUE, NULL);

  opt_reg_string(odb, "-cache:il2", "L2 I-cache config",
		 &cache_il2_opt, "dl2", TRUE, NULL);

  opt_reg_string(odb, "-cache:dl2", "L2 D-cache config",
		 &cache_dl2_opt, "ul2:1024:64:4:l", TRUE, NULL);
}


/* 캐시 파서 */
static void
cache_parse(const char *name, char *optstr,
	    int *nsets, int *bsize, int *assoc, enum cache_policy *policy)
{
  char buf[128];
  strcpy(buf, optstr);

  if (!strcmp(buf, "dl1") || !strcmp(buf, "dl2")) {
    *nsets = *bsize = *assoc = -1;
    *policy = LRU;
    return;
  }

  char pname[32], pol;
  int ns, bs, as;

  if (sscanf(buf, "%[^:]:%d:%d:%d:%c", pname, &ns, &bs, &as, &pol) != 5)
    fatal("cannot parse cache config");

  *nsets = ns;
  *bsize = bs;
  *assoc = as;

  if      (pol == 'l') *policy = LRU;
  else if (pol == 'f') *policy = FIFO;
  else if (pol == 'r') *policy = Random;
  else fatal("unknown policy");
}


/* 시뮬레이터 초기화 */
void
sim_init(void)
{
  int il1_nsets, il1_bsize, il1_assoc; enum cache_policy il1_pol;
  int dl1_nsets, dl1_bsize, dl1_assoc; enum cache_policy dl1_pol;
  int il2_nsets, il2_bsize, il2_assoc; enum cache_policy il2_pol;
  int dl2_nsets, dl2_bsize, dl2_assoc; enum cache_policy dl2_pol;

  cache_parse("il1", cache_il1_opt, &il1_nsets, &il1_bsize, &il1_assoc, &il1_pol);
  cache_parse("dl1", cache_dl1_opt, &dl1_nsets, &dl1_bsize, &dl1_assoc, &dl1_pol);
  cache_parse("il2", cache_il2_opt, &il2_nsets, &il2_bsize, &il2_assoc, &il2_pol);
  cache_parse("dl2", cache_dl2_opt, &dl2_nsets, &dl2_bsize, &dl2_assoc, &dl2_pol);

  /* ---- Hybrid write policy 적용 ---- */

  /* IL1 = Write-Through + No Write Allocate */
  il1 = cache_create("il1", il1_nsets, il1_bsize, il1_assoc,
                     il1_pol, WRITE_THROUGH_NO_ALLOC);

  /* DL1 = 기존 그대로 write-back */
  dl1 = cache_create("dl1", dl1_nsets, dl1_bsize, dl1_assoc,
                     dl1_pol, WRITE_BACK_ALLOC);

  /* IL2 = L1에서 프런트엔드 공유 → write-back */
  if (il2_nsets == -1) {
      il2 = dl1; /* unified L1 case */
  } else {
      il2 = cache_create("il2", il2_nsets, il2_bsize, il2_assoc,
                         il2_pol, WRITE_BACK_ALLOC);
  }

  /* DL2 = L2는 항상 write-back */
  if (dl2_nsets == -1) {
      dl2 = il1; 
  } else {
      dl2 = cache_create("dl2", dl2_nsets, dl2_bsize, dl2_assoc,
                         dl2_pol, WRITE_BACK_ALLOC);
  }

  mem_init();
}


/* 메모리 계층 접근 wrapper */
static void
cache_mem_access(enum mem_cmd cmd, md_addr_t addr, byte_t *p, int nbytes)
{
  /* IL1 */
  if (il1 && cache_access(il1, cmd, addr, p, nbytes, sim_cycle))
    return;

  /* DL1 */
  if (dl1 && cache_access(dl1, cmd, addr, p, nbytes, sim_cycle))
    return;

  /* IL2 */
  if (il2 && cache_access(il2, cmd, addr, p, nbytes, sim_cycle))
    return;

  /* DL2 */
  if (dl2 && cache_access(dl2, cmd, addr, p, nbytes, sim_cycle))
    return;

  /* memory */
  mem_access(cmd, addr, p, nbytes);
}


/* 메인 실행 함수 */
void
sim_main(void)
{
  md_inst_t inst;
  md_addr_t PC, next_PC;

  PC = ld_prog_entry;

  while (sim_num_insn < max_insts) {

    mem_access(Read, PC, (byte_t *)&inst, sizeof(md_inst_t));
    sim_num_insn++;

    next_PC = PC + sizeof(md_inst_t);

    /* 실제 쓰기/읽기 발생 시 캐시 계층 호출 */
    /* 예: cache_mem_access(Read, EA, buf, 4); */

    PC = next_PC;
  }
}


/* 통계 출력 */
void
sim_aux_stats(FILE *stream)
{
  if (il1) {
    fprintf(stream, "il1.accesses %d\n", il1->hits + il1->misses);
    fprintf(stream, "il1.hits %d\n", il1->hits);
    fprintf(stream, "il1.misses %d\n", il1->misses);
  }
  if (dl1) {
    fprintf(stream, "dl1.hits %d\n", dl1->hits);
    fprintf(stream, "dl1.misses %d\n", dl1->misses);
  }
  if (il2) {
    fprintf(stream, "il2.hits %d\n", il2->hits);
    fprintf(stream, "il2.misses %d\n", il2->misses);
  }
  if (dl2) {
    fprintf(stream, "dl2.hits %d\n", dl2->hits);
    fprintf(stream, "dl2.misses %d\n", dl2->misses);
  }
}


