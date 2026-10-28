#include "igloo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static bool integer(const char *text,int minimum,int maximum,int *out){if(!text||!*text)return false;char *end;errno=0;long n=strtol(text,&end,10);if(errno||*end||n<minimum||n>maximum)return false;*out=(int)n;return true;}
static bool number(const char *text,unsigned *out){int n;if(!integer(text,0,0x7fffffff,&n))return false;*out=(unsigned)n;return true;}
static unsigned split(char *text,char separator,char **fields,unsigned limit){unsigned n=1;fields[0]=text;for(char *p=text;*p;p++)if(*p==separator){*p=0;if(n==limit)return 0;fields[n++]=p+1;}return n;}
static bool furniture(IglooLayout *layout,const char *source){
 if(strlen(source)>=IGLOO_LAYOUT_TEXT)return false;
 char text[IGLOO_LAYOUT_TEXT];strcpy(text,source);layout->count=0;if(!*text)return true;
 char *entry=text;
 for(;;){char *next=strchr(entry,',');if(next)*next++=0;if(layout->count==IGLOO_ITEMS)return false;char *fields[5];if(split(entry,'|',fields,5)!=5)return false;IglooFurniture *f=&layout->items[layout->count];
  if(!number(fields[0],&f->id)||!f->id||f->id>1000000||!integer(fields[1],-4096,4096,&f->x)||!integer(fields[2],-4096,4096,&f->y)||!number(fields[3],&f->rotation)||!number(fields[4],&f->frame)||f->rotation>4096||f->frame>4096)return false;
  if(!f->rotation)f->rotation=1;
  if(!f->frame)f->frame=1;
  layout->count++;if(!next)break;entry=next;
 }
 return true;
}
bool igloo_serialize(const IglooLayout *layout,char *out,size_t capacity){if(!capacity||layout->count>IGLOO_ITEMS)return false;out[0]=0;size_t used=0;for(unsigned i=0;i<layout->count;i++){const IglooFurniture *f=&layout->items[i];int n=snprintf(out+used,capacity-used,"%s%u|%d|%d|%u|%u",i?",":"",f->id,f->x,f->y,f->rotation,f->frame);if(n<0||(size_t)n>=capacity-used)return false;used+=(size_t)n;}return true;}
bool igloo_request(Igloo *i,unsigned owner){if(!owner||owner>0x7fffffff-2000||i->dirty||i->awaitingSave||i->awaitingStyle||i->awaitingMusic)return false;i->wantedOwner=owner;i->saveFailed=i->saveVerified=false;return true;}
static bool receive_layout(Igloo *i,const PNetMessage *m){
 if(m->count<5)return false;
 IglooLayout next={0};if(!number(m->args[0],&next.owner)||!next.owner||!number(m->args[1],&next.type)||next.type>1000000||!number(m->args[2],&next.music)||!number(m->args[3],&next.floor)||next.music>1000000||next.floor>1000000||!furniture(&next,m->args[4]))return false;
 if(i->wantedOwner&&next.owner!=i->wantedOwner)return false;
 if(i->awaitingMusic){
  if(next.owner!=i->layout.owner)return false;
  char actual[IGLOO_LAYOUT_TEXT];i->awaitingMusic=false;i->musicVerified=next.music==i->targetMusic&&next.type==i->layout.type&&next.floor==i->layout.floor&&igloo_serialize(&next,actual,sizeof actual)&&!strcmp(actual,i->expected);i->musicFailed=!i->musicVerified;
 }
 if(i->awaitingStyle){
  if(next.owner!=i->layout.owner)return false;
  i->awaitingStyle=false;i->styleVerified=next.type==i->targetType&&next.music==i->targetMusic&&!next.floor&&!next.count;i->styleFailed=!i->styleVerified;
  i->slotsReady=i->activeKnown=false;
 }
 if(i->awaitingSave&&next.owner!=i->layout.owner)return false;
 if(i->awaitingSave){char actual[IGLOO_LAYOUT_TEXT];i->awaitingSave=false;i->saveVerified=igloo_serialize(&next,actual,sizeof actual)&&!strcmp(actual,i->expected);i->saveFailed=!i->saveVerified;if(!i->saveVerified){i->revision++;return true;}i->dirty=false;}
 else if(i->editing&&i->dirty)return false;
 i->layout=next;i->ready=true;i->wantedOwner=0;i->revision++;return true;
}
static bool same_furniture(const IglooLayout *a,const IglooLayout *b){if(a->count!=b->count)return false;for(unsigned n=0;n<a->count;n++){const IglooFurniture *x=&a->items[n],*y=&b->items[n];if(x->id!=y->id||x->x!=y->x||x->y!=y->y||x->rotation!=y->rotation||x->frame!=y->frame)return false;}return true;}
static void owned_add(Igloo *i,unsigned id,unsigned amount){if(!id||id>1000000||!amount)return;for(unsigned n=0;n<i->nOwned;n++)if(i->owned[n].id==id){i->owned[n].amount=amount>65535-i->owned[n].amount?65535:i->owned[n].amount+amount;return;}if(i->nOwned<IGLOO_OWNED)i->owned[i->nOwned++]=(IglooOwned){id,amount>65535?65535:amount};}
bool igloo_apply(Igloo *i,const PNetMessage *m){
 if(!strcmp(m->command,"gm"))return receive_layout(i,m);
 if(!strcmp(m->command,"gr")){i->nOpen=0;for(unsigned n=0;n<m->count&&i->nOpen<256;n++){if(strlen(m->args[n])>160)continue;char text[161],*f[3];strcpy(text,m->args[n]);unsigned count=split(text,'|',f,3),owner;if(count!=2||!number(f[0],&owner)||!owner||owner>0x7fffffff-2000||!*f[1])continue;bool duplicate=false;for(unsigned j=0;j<i->nOpen;j++)if(i->open[j].owner==owner)duplicate=true;if(duplicate)continue;OpenIgloo *entry=&i->open[i->nOpen++];entry->owner=owner;snprintf(entry->name,sizeof entry->name,"%.48s",f[1]);}i->openReady=true;i->revision++;return true;}
 if(!strcmp(m->command,"gf")){i->nOwned=0;for(unsigned n=0;n<m->count;n++){if(strlen(m->args[n])>80)continue;char text[81],*fields[3];strcpy(text,m->args[n]);unsigned count=split(text,'|',fields,3),id,amount=1;if(!count||!number(fields[0],&id))continue;if(count>=2&&!number(fields[count-1],&amount))continue;owned_add(i,id,amount);}i->ownedReady=true;}
 else if(!strcmp(m->command,"af")&&m->count){unsigned id;if(!number(m->args[0],&id))return false;if(i->ownedReady)owned_add(i,id,1);}
 else if(!strcmp(m->command,"go")&&i->awaitingTypes){i->nTypes=0;for(unsigned n=0;n<m->count;n++){if(strlen(m->args[n])>2047)continue;char text[2048];strcpy(text,m->args[n]);char *save=NULL;for(char *p=strtok_r(text,"|",&save);p&&i->nTypes<128;p=strtok_r(NULL,"|",&save)){unsigned id;if(number(p,&id)&&id)i->types[i->nTypes++]=id;}}i->awaitingTypes=false;i->typesReady=true;}
 else if(!strcmp(m->command,"gail")&&m->count>=2){unsigned owner;if(!number(m->args[0],&owner)||owner!=i->layout.owner)return false;i->nSlots=i->activeSlot=0;i->activeKnown=false;unsigned matches=0;
  for(unsigned n=2;n<m->count&&i->nSlots<IGLOO_LAYOUTS;n++){if(strlen(m->args[n])>=IGLOO_LAYOUT_TEXT)continue;char text[IGLOO_LAYOUT_TEXT],*f[10];strcpy(text,m->args[n]);if(split(text,':',f,10)!=10)continue;IglooSlot slot={.layout.owner=owner};unsigned locked;if(!number(f[0],&slot.id)||!number(f[3],&locked)||!number(f[4],&slot.layout.music)||!number(f[5],&slot.layout.floor)||!number(f[6],&slot.location)||!number(f[7],&slot.layout.type)||!furniture(&slot.layout,f[9]))continue;slot.locked=locked!=0;i->slots[i->nSlots++]=slot;if(slot.layout.type==i->layout.type&&slot.layout.music==i->layout.music&&slot.layout.floor==i->layout.floor&&same_furniture(&slot.layout,&i->layout)){i->activeSlot=slot.id;matches++;}}
  i->slotsReady=true;i->activeKnown=matches==1;if(!i->activeKnown)i->activeSlot=0;
 }else return false;
 i->revision++;return true;
}
bool igloo_begin_edit(Igloo *i,unsigned player){if(!i->ready||i->layout.owner!=player||i->awaitingSave||i->awaitingStyle||i->awaitingMusic)return false;i->editing=true;return true;}
unsigned igloo_available(const Igloo *i,unsigned id){unsigned total=0;for(unsigned n=0;n<i->nOwned;n++)if(i->owned[n].id==id)total=i->owned[n].amount;for(unsigned n=0;n<i->layout.count;n++)if(i->layout.items[n].id==id&&total)total--;return total;}
bool igloo_add(Igloo *i,unsigned id,int x,int y){if(!i->editing||i->layout.count==IGLOO_ITEMS||!igloo_available(i,id)||x<=20||x>=740||y<=20||y>=460)return false;i->layout.items[i->layout.count++]=(IglooFurniture){id,x,y,1,1};i->dirty=true;i->revision++;return true;}
bool igloo_remove(Igloo *i,unsigned index){if(!i->editing||index>=i->layout.count)return false;memmove(i->layout.items+index,i->layout.items+index+1,(--i->layout.count-index)*sizeof *i->layout.items);i->dirty=true;i->revision++;return true;}
bool igloo_place(Igloo *i,unsigned index,int x,int y,unsigned rotation,unsigned frame){if(!i->editing||index>=i->layout.count||x<=20||x>=740||y<=20||y>=460||!rotation||rotation>4096||!frame||frame>4096)return false;IglooFurniture *f=&i->layout.items[index];f->x=x;f->y=y;f->rotation=rotation;f->frame=frame;i->dirty=true;i->revision++;return true;}
bool igloo_save_prepare(Igloo *i){if(!i->editing||!i->dirty||i->awaitingSave||i->awaitingStyle||i->awaitingMusic||!igloo_serialize(&i->layout,i->expected,sizeof i->expected))return false;i->awaitingSave=true;i->saveVerified=i->saveFailed=false;return true;}
bool igloo_styles_request(Igloo *i,unsigned player){
 if(!i->ready||!player||i->layout.owner!=player||i->dirty||i->awaitingSave||i->awaitingStyle||i->awaitingMusic)return false;
 i->awaitingTypes=true;i->typesReady=i->slotsReady=i->activeKnown=false;i->styleVerified=i->styleFailed=false;return true;
}
bool igloo_style_prepare(Igloo *i,unsigned player,unsigned type){
 if(!i->ready||!player||i->layout.owner!=player||i->dirty||i->awaitingSave||i->awaitingStyle||i->awaitingMusic||!i->typesReady||!i->slotsReady||!i->activeKnown||type==i->layout.type)return false;
 bool owned=false,slot=false;for(unsigned n=0;n<i->nTypes;n++)if(i->types[n]==type)owned=true;
 for(unsigned n=0;n<i->nSlots;n++)if(i->slots[n].id==i->activeSlot)slot=true;
 if(!owned||!slot)return false;
 i->targetType=type;i->targetMusic=i->layout.music;i->awaitingStyle=true;i->styleVerified=i->styleFailed=false;i->editing=false;return true;
}
bool igloo_floor_purchase(Igloo *i,const PNetMessage *m,unsigned player){
 unsigned floor;if(strcmp(m->command,"ag")||m->count<2||!i->ready||!player||i->layout.owner!=player||i->dirty||i->awaitingSave||i->awaitingStyle||i->awaitingMusic||!number(m->args[0],&floor)||floor>1000000)return false;
 i->layout.floor=floor;i->slotsReady=i->activeKnown=false;i->revision++;return true;
}
bool igloo_music_prepare(Igloo *i,unsigned player,unsigned music){
 if(!i->ready||!player||i->layout.owner!=player||music>1000000||i->dirty||i->awaitingSave||i->awaitingStyle||i->awaitingMusic||!igloo_serialize(&i->layout,i->expected,sizeof i->expected))return false;
 i->targetMusic=music;i->awaitingMusic=true;i->musicVerified=i->musicFailed=false;return true;
}
