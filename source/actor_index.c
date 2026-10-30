#include "actor_index.h"
#include <string.h>
static unsigned bucket(unsigned id){id^=id>>16;id*=0x7feb352du;id^=id>>15;return id&(ACTOR_INDEX_BUCKETS-1);}
void actor_index_build(ActorIndex *index,const unsigned *ids,unsigned count){
 memset(index,0,sizeof *index);if(count>ACTOR_INDEX_CAPACITY)count=ACTOR_INDEX_CAPACITY;index->count=count;memcpy(index->ids,ids,count*sizeof *ids);
 for(unsigned i=0;i<count;i++){unsigned id=ids[i];if(!id)continue;unsigned slot=bucket(id);while(index->slots[slot]){if(index->ids[index->slots[slot]-1]==id)break;slot=(slot+1)&(ACTOR_INDEX_BUCKETS-1);}if(!index->slots[slot])index->slots[slot]=(uint16_t)(i+1);}
}
int actor_index_find(const ActorIndex *index,unsigned id){
 if(!id)return -1;
 for(unsigned slot=bucket(id),n=0;n<ACTOR_INDEX_BUCKETS;n++,slot=(slot+1)&(ACTOR_INDEX_BUCKETS-1)){
  unsigned link=index->slots[slot];if(!link)return -1;if(index->ids[link-1]==id)return (int)link-1;
 }
 return -1;
}
