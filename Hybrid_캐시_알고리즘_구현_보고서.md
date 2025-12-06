# SimpleScalar Hybrid 캐시 교체 알고리즘 구현 보고서

## 1. 구현 개요

SimpleScalar의 캐시 교체 정책에 **Hybrid (하이브리드)** 알고리즘을 추가했습니다.

## 2. Hybrid 정책이란?

### 2.1 개념

- **Hybrid 정책**: LRU와 MRU를 조합한 교체 정책
- **구현 방식**: 짝수 세트는 LRU, 홀수 세트는 MRU 사용
- **목적**: 다양한 접근 패턴에 적응적으로 대응

### 2.2 동작 원리

```
세트 인덱스가 짝수 (0, 2, 4, ...): LRU 정책 사용
  → 가장 오래 사용되지 않은 블록 교체

세트 인덱스가 홀수 (1, 3, 5, ...): MRU 정책 사용
  → 가장 최근에 사용된 블록 교체
```

### 2.3 장점

1. **적응성**: 워크로드의 특성에 따라 자동으로 다른 전략 적용
2. **균형**: LRU와 MRU의 장점을 동시에 활용
3. **실험 가능**: 단일 실행으로 두 정책의 효과를 동시에 관찰

## 3. 수정된 파일

### 3.1 `simplesim-3.0/cache.h`

```c
/* cache replacement policy */
enum cache_policy {
  LRU,      /* replace least recently used block (perfect LRU) */
  Random,   /* replace a random block */
  FIFO,     /* replace the oldest block in the set */
  MRU,      /* replace most recently used block (opposite of LRU) */
  Hybrid    /* hybrid policy combining LRU and MRU */
};
```

### 3.2 `simplesim-3.0/cache.c`

#### (1) 정책 파싱 함수

```c
enum cache_policy
cache_char2policy(char c)
{
  switch (c) {
  case 'l': return LRU;
  case 'r': return Random;
  case 'f': return FIFO;
  case 'm': return MRU;
  case 'h': return Hybrid;  // 추가
  default: fatal("bogus replacement policy, `%c'", c);
  }
}
```

#### (2) 교체 로직 (캐시 미스 시)

```c
case Hybrid:
  /* Hybrid: use LRU for even sets, MRU for odd sets */
  if (set & 1) {
    /* odd set: use MRU */
    repl = cp->sets[set].way_head;
  } else {
    /* even set: use LRU */
    repl = cp->sets[set].way_tail;
  }
  update_way_list(&cp->sets[set], repl, Head);
  break;
```

#### (3) 캐시 히트 시 업데이트

```c
/* if LRU, MRU, or Hybrid replacement and this is not the first element of list, reorder */
if (blk->way_prev && (cp->policy == LRU || cp->policy == MRU || cp->policy == Hybrid))
{
  /* move this block to head of the way (MRU) list */
  update_way_list(&cp->sets[set], blk, Head);
}
```

#### (4) 설정 출력

```c
cp->policy == LRU ? "LRU"
: cp->policy == Random ? "Random"
: cp->policy == FIFO ? "FIFO"
: cp->policy == MRU ? "MRU"
: cp->policy == Hybrid ? "Hybrid"  // 추가
: (abort(), "");
```

### 3.3 `simplesim-3.0/sim-cache.c`

도움말 메시지에 Hybrid 옵션 추가:

```
<repl>   - block replacement strategy, 'l'-LRU, 'f'-FIFO, 'r'-random,
           'm'-MRU, 'h'-Hybrid
```

## 4. 컴파일 및 실행

### 4.1 컴파일

```bash
cd simplesim-3.0
make clean
make
```

### 4.2 실행 예시

```bash
# Hybrid 정책으로 L1 캐시 실행
./sim-cache -redir:sim hybrid_L1L2.txt \
  -max:inst 1000000000 \
  -cache:il1 il1:512:64:2:h \
  -cache:dl1 dl1:512:64:2:h \
  -cache:dl2 ul2:1024:64:4:h \
  ../benchmark/gzip/gzip00.peak.ev6 \
  ../benchmark/gzip/input.combined
```

### 4.3 캐시 설정 형식

```
<name>:<nsets>:<bsize>:<assoc>:<repl>

예시: dl1:512:64:2:h
  - dl1: 캐시 이름
  - 512: 세트 개수
  - 64: 블록 크기 (바이트)
  - 2: 연관도 (2-way)
  - h: Hybrid 정책
```

## 5. 실험 설계

### 5.1 비교 실험

```bash
# LRU 정책
./sim-cache -redir:sim lru_result.txt \
  -cache:dl1 dl1:512:64:2:l \
  benchmark

# MRU 정책
./sim-cache -redir:sim mru_result.txt \
  -cache:dl1 dl1:512:64:2:m \
  benchmark

# Hybrid 정책
./sim-cache -redir:sim hybrid_result.txt \
  -cache:dl1 dl1:512:64:2:h \
  benchmark
```

### 5.2 분석 지표

- **Hit Rate**: 캐시 적중률
- **Miss Rate**: 캐시 미스율
- **Replacements**: 교체 횟수
- **Writebacks**: 쓰기 백 횟수

### 5.3 예상 결과

| 워크로드 유형       | LRU  | MRU  | Hybrid   |
| ------------------- | ---- | ---- | -------- |
| 순차 접근 (한 번만) | 낮음 | 높음 | **중간** |
| 지역성 강함         | 높음 | 낮음 | **중간** |
| 혼합 패턴           | 중간 | 중간 | **최적** |

## 6. 구현 상세

### 6.1 왜 세트 인덱스 기반인가?

- **공간적 분산**: 메모리 주소가 여러 세트에 분산되므로 다양한 정책 적용
- **구현 단순성**: 복잡한 런타임 분석 없이 효율적 구현
- **예측 가능성**: 동일 주소는 항상 동일 정책 적용

### 6.2 대안적 Hybrid 구현

더 복잡한 구현을 원한다면:

#### (1) 카운터 기반

```c
static int access_counter = 0;
if ((access_counter++ % 2) == 0)
  repl = cp->sets[set].way_tail;  // LRU
else
  repl = cp->sets[set].way_head;  // MRU
```

#### (2) 미스율 기반 적응형

```c
if (cp->misses > cp->hits)
  repl = cp->sets[set].way_head;  // MRU
else
  repl = cp->sets[set].way_tail;  // LRU
```

## 7. 검증 방법

### 7.1 정상 동작 확인

```bash
# 실행 후 출력에서 확인
grep "replacement policy" hybrid_L1L2.txt

# 예상 출력:
# cache: il1: 2-way, `Hybrid' replacement policy, write-back
# cache: dl1: 2-way, `Hybrid' replacement policy, write-back
```

### 7.2 통계 확인

```bash
# 캐시 통계 확인
grep -A 10 "cache stats" hybrid_L1L2.txt
```

## 8. 보고서 작성 가이드

### 8.1 포함할 내용

1. **서론**: Hybrid 정책의 필요성
2. **설계**: 구현 방식 및 알고리즘
3. **구현**: 코드 변경 사항 (이 문서 참조)
4. **실험**: 벤치마크 결과 및 그래프
5. **분석**: LRU/MRU/Hybrid 성능 비교
6. **결론**: 각 정책의 적합한 사용 사례

### 8.2 그래프 예시

- Miss Rate 비교 (LRU vs MRU vs Hybrid)
- Hit Rate 비교
- 세트별 교체 패턴 분석
- 워크로드별 성능 비교

## 9. 참고사항

### 9.1 기존 정책과의 호환성

- 모든 기존 정책 (LRU, FIFO, Random, MRU)은 정상 동작
- Hybrid는 독립적으로 추가되어 기존 코드에 영향 없음

### 9.2 제한사항

- 현재 구현은 세트 인덱스 기반 (짝수/홀수)
- 더 복잡한 적응형 알고리즘은 추가 구현 필요

### 9.3 확장 가능성

- 런타임 통계 기반 동적 전환
- 워크로드 특성 분석 기반 자동 선택
- 세트별 독립적인 정책 학습

## 10. 요약

✅ **완료된 작업**

- Hybrid enum 추가 (cache.h)
- 'h' 문자 파싱 추가 (cache.c)
- 교체 로직 구현 (짝수 세트: LRU, 홀수 세트: MRU)
- 캐시 히트 시 업데이트 로직 추가
- 설정 출력 함수 업데이트
- 도움말 메시지 업데이트 (sim-cache.c)

✅ **테스트 준비 완료**

- 컴파일 후 즉시 사용 가능
- 명령어: `-cache:dl1 dl1:512:64:2:h`

---

**작성일**: 2025년 12월 7일  
**SimpleScalar 버전**: 3.0  
**구현자**: [이름]
