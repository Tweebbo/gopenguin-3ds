#pragma once
#include <stdint.h>
#define WEAR_INDEX_SLOTS 128
#define WEAR_INDEX_BUCKETS 256
typedef struct {uint16_t heads[WEAR_INDEX_BUCKETS],next[WEAR_INDEX_SLOTS];unsigned ids[WEAR_INDEX_SLOTS];} WearIndex;
void wear_index_build(WearIndex *index,const unsigned ids[WEAR_INDEX_SLOTS]);
int wear_index_first(const WearIndex *index,unsigned id);
int wear_index_next(const WearIndex *index,unsigned slot);
