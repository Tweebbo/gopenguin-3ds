#include "../source/game_fire_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void list(const unsigned *values,unsigned count){
 for(unsigned i=0;i<count;i++)printf("%u,",values[i]);
 putchar('|');
}
static void bools(const bool *values,unsigned count){
 for(unsigned i=0;i<count;i++)printf("%u,",values[i]?1u:0u);
 putchar('|');
}

static void print(const FireModel *m,bool ok){
 printf("%d|%d|%d|%u|%u|",ok?1:0,(int)m->phase,m->seat,m->players,m->active);
 list(m->colour,4);list(m->energy,4);list(m->position,4);list(m->spin,3);list(m->hand,5);
 list(m->played,4);list(m->result,4);list(m->standing,4);
 printf("%c|",m->trump);bools(m->participants,4);bools(m->opponents,4);
 printf("%u%u%u%u%u%u%u|%u|%u|",m->menuReady,m->started,m->requestGet,m->requestReady,m->readySent,m->inputPending,m->rankAwarded,m->rank,m->percentage);
 for(unsigned i=0;i<4;i++)printf("%s,",m->names[i]);
 printf("\n");
}

int main(void){
 FireModel model;fire_model_init(&model);
 char line[2048];
 while(fgets(line,sizeof line,stdin)){
  line[strcspn(line,"\r\n")]=0;
  if(!*line)continue;
  char *tokens[12];unsigned count=0;char *part=line;
  while(part&&count<12){
   tokens[count++]=part;
   char *bar=strchr(part,'|');
   if(bar){*bar=0;part=bar+1;}else part=NULL;
  }
  if(!count)continue;
  if(!strcmp(tokens[0],"begin")){
   fire_model_begin(&model,count>1&&atoi(tokens[1]));
   print(&model,true);
  }else if(!strcmp(tokens[0],"reset")){
   fire_model_init(&model);
   print(&model,true);
  }else{
   bool ok=fire_model_packet(&model,tokens[0],count-1,tokens+1);
   print(&model,ok);
  }
 }
 assert(feof(stdin));
 return 0;
}
