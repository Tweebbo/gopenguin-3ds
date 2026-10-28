#include "profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int compare(const void *a,const void *b){unsigned x=*(const unsigned *)a,y=*(const unsigned *)b;return x<y?-1:x>y;}
static unsigned value(const char *s){char *end;unsigned long n=strtoul(s,&end,10);return *end||n>0x7fffffff?0:(unsigned)n;}
void profile_init(GPProfile *p,bool offline){memset(p,0,sizeof *p);if(offline){for(unsigned id=1;id<=15;id++)p->inventory[p->count++]=id;p->ready=true;}p->revision=p->inventoryRevision=p->epfRevision=1;}
void profile_epf_reset(GPProfile *p){p->epfReady=p->epfAgent=false;p->epfRevision++;p->revision++;}
bool profile_has(const GPProfile *p,unsigned id){return bsearch(&id,p->inventory,p->count,sizeof *p->inventory,compare)!=NULL;}
void profile_apply(GPProfile *p,const PNetMessage *m,unsigned mine){
 if(!strcmp(m->command,"epfga")){
  if(m->count==1&&(!strcmp(m->args[0],"0")||!strcmp(m->args[0],"1"))){p->epfReady=true;p->epfAgent=!strcmp(m->args[0],"1");p->epfRevision++;p->revision++;}
  else profile_epf_reset(p);
 }
 else if(!strcmp(m->command,"lp")&&m->count>=6){p->coins=value(m->args[1]);p->age=value(m->args[5]);p->revision++;}
 else if(!strcmp(m->command,"gi")){p->count=0;for(unsigned i=0;i<m->count&&p->count<INVENTORY_LIMIT;i++){unsigned id=value(m->args[i]);if(id)p->inventory[p->count++]=id;}qsort(p->inventory,p->count,sizeof *p->inventory,compare);p->ready=true;p->revision++;p->inventoryRevision++;}
 else if(!strcmp(m->command,"ai")&&m->count>=2){unsigned id=value(m->args[0]);if(id&&!profile_has(p,id)&&p->count<INVENTORY_LIMIT){p->inventory[p->count++]=id;qsort(p->inventory,p->count,sizeof *p->inventory,compare);}p->coins=value(m->args[1]);p->revision++;p->inventoryRevision++;}
 else if((!strcmp(m->command,"af")||!strcmp(m->command,"au")||!strcmp(m->command,"ag"))&&m->count>=2){p->coins=value(m->args[1]);p->revision++;}
 else if((!strcmp(m->command,"gc")||!strcmp(m->command,"gtc")||!strcmp(m->command,"ac")||!strcmp(m->command,"ms"))&&m->count){p->coins=value(m->args[0]);p->revision++;}
 else if(!strcmp(m->command,"gps")&&m->count>=2&&value(m->args[0])==mine){p->stamps=0;for(const char *c=m->args[1];*c;c++)if(*c=='|')p->stamps++;if(*m->args[1])p->stamps++;p->revision++;}
 else if(!strcmp(m->command,"e")&&m->count){unsigned code=value(m->args[0]);snprintf(p->error,sizeof p->error,"%s",code==400?"You already have this item.":code==401?"You need more coins.":code==402?"This item is not available.":"The server refused this action.");}
}
