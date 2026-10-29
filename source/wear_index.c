#include "wear_index.h"
#include <string.h>
static unsigned bucket(unsigned id){id^=id>>16;id*=0x7feb352du;id^=id>>15;return id&(WEAR_INDEX_BUCKETS-1);}
void wear_index_build(WearIndex *index,const unsigned ids[WEAR_INDEX_SLOTS]){
 memset(index->heads,0,sizeof index->heads);memcpy(index->ids,ids,sizeof index->ids);
 for(unsigned slot=WEAR_INDEX_SLOTS;slot-->0;){unsigned id=ids[slot],hash=bucket(id);index->next[slot]=0;if(id){index->next[slot]=index->heads[hash];index->heads[hash]=(uint16_t)(slot+1);}}
}
static int match(const WearIndex *index,unsigned link,unsigned id){while(link){unsigned slot=link-1;if(index->ids[slot]==id)return (int)slot;link=index->next[slot];}return -1;}
int wear_index_first(const WearIndex *index,unsigned id){return id?match(index,index->heads[bucket(id)],id):-1;}
int wear_index_next(const WearIndex *index,unsigned slot){return slot<WEAR_INDEX_SLOTS?match(index,index->next[slot],index->ids[slot]):-1;}
