#pragma once
#include <stdint.h>
#define ACTOR_INDEX_CAPACITY 80
#define ACTOR_INDEX_BUCKETS 256
typedef struct {unsigned ids[ACTOR_INDEX_CAPACITY],count;uint16_t slots[ACTOR_INDEX_BUCKETS];} ActorIndex;
void actor_index_build(ActorIndex *index,const unsigned *ids,unsigned count);
int actor_index_find(const ActorIndex *index,unsigned id);
