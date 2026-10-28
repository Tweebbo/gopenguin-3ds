#include "puffles.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *colours[PUFFLES_COLOURS]={"blue","pink","black","green","purple","red","yellow","white","orange"};
const char *puffles_colour(unsigned type){return type<PUFFLES_COLOURS?colours[type]:NULL;}
static bool number(const char *text,unsigned maximum,unsigned *out){
 if(!text||!*text||*text=='-'||*text=='+')return false;
 char *end;errno=0;unsigned long value=strtoul(text,&end,10);
 if(errno||*end||value>maximum)return false;
 *out=(unsigned)value;return true;
}
static bool record(const char *source,Puffle *out){
 if(!source||strlen(source)>=512)return false;
 char text[512],*fields[13];strcpy(text,source);unsigned count=1;fields[0]=text;
 for(char *c=text;*c;c++)if(*c=='|'){if(count==13)return false;*c=0;fields[count++]=c+1;}
 if(count<6||!number(fields[0],0x7fffffff,&out->id)||!out->id||!*fields[1]||strlen(fields[1])>=sizeof out->name)return false;
 for(const unsigned char *c=(unsigned char *)fields[1];*c;c++)if(*c<32||*c==127)return false;
 if(!number(fields[2],PUFFLES_COLOURS-1,&out->type)||!number(fields[3],100,&out->health)||!number(fields[4],100,&out->food)||!number(fields[5],100,&out->rest))return false;
 if(count==13){unsigned walk;if(!number(fields[12],1,&walk))return false;out->walking=walk!=0;}
 else out->walking=false;
 strcpy(out->name,fields[1]);return true;
}
static const Actor *actor(const Demo *world,unsigned owner){
 if(world)for(unsigned i=0;i<world->count&&i<CAST_COUNT;i++)if(world->actors[i].id==owner)return &world->actors[i];
 return NULL;
}
Puffle *puffles_find(Puffles *p,unsigned owner,unsigned id){
 for(unsigned i=0;i<p->count;i++)if(p->pets[i].owner==owner&&p->pets[i].id==id)return &p->pets[i];
 return NULL;
}
static PuffleWalk *walking(Puffles *p,unsigned owner,bool create){
 for(unsigned i=0;i<p->nWalks;i++)if(p->walks[i].owner==owner)return &p->walks[i];
 if(!create||p->nWalks>=CAST_COUNT+1)return NULL;
 PuffleWalk *walk=&p->walks[p->nWalks++];*walk=(PuffleWalk){.owner=owner};return walk;
}
static void reconcile(Puffles *p,unsigned owner){
 PuffleWalk *walk=walking(p,owner,false);
 if(walk)for(unsigned i=0;i<p->count;i++)if(p->pets[i].owner==owner)p->pets[i].walking=walk->id==p->pets[i].id;
 if(owner!=p->mine&&owner!=p->roomOwner)for(unsigned i=0;i<p->count;){if(p->pets[i].owner==owner&&!p->pets[i].walking)p->pets[i]=p->pets[--p->count];else i++;}
}
static unsigned owned(const Puffles *p){unsigned n=0;for(unsigned i=0;i<p->count;i++)n+=p->pets[i].owner==p->mine;return n;}
static Puffle *put(Puffles *p,const Puffle *info,unsigned index){
 Puffle *pet=puffles_find(p,info->owner,info->id);
 if(!pet){
  if(p->count==PUFFLES_LIMIT||(info->owner==p->mine&&owned(p)>=PUFFLES_OWNED))return NULL;
  pet=&p->pets[p->count++];*pet=(Puffle){.x=280+(index%5)*46,.y=300+(index/5)*38,.frame=1,.direction=1};pet->oldX=pet->x;pet->oldY=pet->y;
 }
 pet->owner=info->owner;pet->id=info->id;pet->type=info->type;pet->health=info->health;pet->food=info->food;pet->rest=info->rest;pet->walking=info->walking;strcpy(pet->name,info->name);pet->revision=++p->revision;
 return pet;
}
static void fail(Puffles *p,const char *text){snprintf(p->error,sizeof p->error,"%s",text);}
static bool send(Puffles *p,const char *command,unsigned count,const char *const *args){
 if(p->send&&p->send(p->user,command,count,args))return true;
 fail(p,"Could not send the puffle request.");return false;
}
void puffles_init(Puffles *p,PuffleSend callback,void *user){memset(p,0,sizeof *p);p->send=callback;p->user=user;p->epoch=1;}
void puffles_room(Puffles *p,unsigned mine,unsigned owner){
 if(p->mine&&mine!=p->mine){PuffleSend callback=p->send;void *user=p->user;puffles_init(p,callback,user);}
 p->mine=mine;p->roomOwner=owner;p->epoch++;p->nWalks=0;
 for(unsigned i=0;i<p->count;)if(p->pets[i].owner!=mine){p->pets[i]=p->pets[--p->count];}else i++;
 for(unsigned i=0;i<p->count;i++)if(p->pets[i].walking){PuffleWalk *walk=walking(p,mine,true);if(walk){walk->id=p->pets[i].id;walk->revision=++p->revision;}}
}
bool puffles_request(Puffles *p,unsigned owner){
 if(!p->mine||!owner||owner>0x7fffffff||p->snapshotPending)return false;
 char id[16];snprintf(id,sizeof id,"%u",owner);
 if(!send(p,"p#pg",1,(const char *[]){id}))return false;
 p->snapshotPending=true;p->snapshotOwner=owner;p->snapshotEpoch=p->epoch;p->snapshotRevision=p->revision;p->snapshotAge=0;return true;
}
static bool snapshot(Puffles *p,const PNetMessage *m){
 if(!p->snapshotPending)return false;
 unsigned owner=p->snapshotOwner;bool current=p->snapshotEpoch==p->epoch;
 p->snapshotPending=false;p->snapshotAge=0;
 if(!current)return true;
 Puffle rows[PUFFLES_OWNED];unsigned count=0;bool valid=true;
 for(unsigned i=0;i<m->count;i++){
  if(!*m->args[i])continue;
  if(count==PUFFLES_OWNED){valid=false;break;}
  Puffle next={.owner=owner};if(!record(m->args[i],&next)){valid=false;break;}
  for(unsigned j=0;j<count;j++)if(rows[j].id==next.id)valid=false;
  if(!valid)break;
  rows[count++]=next;
 }
 if(!valid){fail(p,"The server returned an invalid puffle list.");return true;}
 for(unsigned i=0;i<p->count;){
  Puffle *pet=&p->pets[i];bool found=false;for(unsigned j=0;j<count;j++)if(rows[j].id==pet->id)found=true;
  if(pet->owner==owner&&!found&&pet->revision<=p->snapshotRevision){p->pets[i]=p->pets[--p->count];}else i++;
 }
 PuffleWalk *walk=walking(p,owner,true);
 if(walk&&walk->revision<=p->snapshotRevision){walk->id=0;for(unsigned i=0;i<count;i++)if(rows[i].walking)walk->id=rows[i].id;walk->revision=++p->revision;}
 for(unsigned i=0;i<count;i++){Puffle *old=puffles_find(p,owner,rows[i].id);if(!old||old->revision<=p->snapshotRevision)put(p,&rows[i],i);}
 reconcile(p,owner);return true;
}
static void reaction(Puffle *pet,unsigned item){if(!pet)return;pet->reaction=item==27?1:item==37?2:3;pet->reactionTicks=0;pet->clock=0;}
static bool observed(const Puffles *p,const Demo *world,unsigned owner){return owner==p->mine||owner==p->roomOwner||actor(world,owner)!=NULL;}
bool puffles_apply(Puffles *p,const PNetMessage *m,const Demo *world){
 if(!strcmp(m->command,"pg"))return snapshot(p,m);
 if((!strcmp(m->command,"pw")||!strcmp(m->command,"pcare"))&&m->count>=2){
  unsigned owner;Puffle next={0};if(!number(m->args[0],0x7fffffff,&owner)||!owner||!observed(p,world,owner)||!record(m->args[1],&next))return false;
  next.owner=owner;bool isWalk=!strcmp(m->command,"pw");PuffleWalk *walk=walking(p,owner,true);
  if(walk&&(isWalk||!walk->revision)){
   if(next.walking)walk->id=next.id;else if(!walk->revision||walk->id==next.id)walk->id=0;
   walk->revision=++p->revision;
  }
  Puffle *pet=put(p,&next,0);
  if(!isWalk&&m->count>=3){unsigned item;if(number(m->args[2],1000000,&item))reaction(pet,item);}
  reconcile(p,owner);
  if(isWalk&&owner==p->mine&&p->action==PUFFLE_WALK&&p->actionId==next.id){p->action=PUFFLE_IDLE;p->actionAge=0;}
  return true;
 }
 if(!strcmp(m->command,"pcid")&&m->count>=3){
  unsigned owner,coins;if(!number(m->args[0],0x7fffffff,&owner)||owner!=p->mine||!number(m->args[2],0x7fffffff,&coins))return false;
  if(p->action!=PUFFLE_CARE)return false;
  if(m->count>=4&&*m->args[3]){unsigned id;if(!number(m->args[1],0x7fffffff,&id)||id!=p->actionId)return false;fail(p,!strcmp(m->args[3],"coins")?"You need more coins to care for your puffle.":"This puffle care action is unavailable.");p->action=PUFFLE_IDLE;p->coins=coins;p->coinsChanged=true;return true;}
  char text[256];if(strlen(m->args[1])>=sizeof text)return false;strcpy(text,m->args[1]);char *fields[6];unsigned count=1;fields[0]=text;for(char *c=text;*c;c++)if(*c=='|'){if(count==6)return false;*c=0;fields[count++]=c+1;}
  unsigned id,food,rest,health;if(count!=6||!number(fields[0],0x7fffffff,&id)||id!=p->actionId||!number(fields[1],100,&food)||!number(fields[3],100,&rest)||!number(fields[4],100,&health))return false;
  Puffle *pet=puffles_find(p,p->mine,id);if(pet){pet->food=food;pet->rest=rest;pet->health=health;pet->revision=++p->revision;reaction(pet,p->actionItem);}
  p->action=PUFFLE_IDLE;p->actionAge=0;p->coins=coins;p->coinsChanged=true;return true;
 }
 if(!strcmp(m->command,"pcn")&&m->count>=2&&p->action==PUFFLE_CHECK_NAME&&!strcmp(m->args[0],p->adoptName)){
  if(strcmp(m->args[1],"1")){fail(p,"Please choose another puffle name.");p->action=PUFFLE_IDLE;return true;}
  char type[16];snprintf(type,sizeof type,"%u",p->adoptType);
  if(send(p,"p#pn",2,(const char *[]){type,p->adoptName})){p->action=PUFFLE_ADOPT;p->actionAge=0;}else p->action=PUFFLE_IDLE;
  return true;
 }
 if(!strcmp(m->command,"pn")&&m->count>=2&&p->action==PUFFLE_ADOPT){
  unsigned coins;Puffle next={.owner=p->mine};if(!number(m->args[0],0x7fffffff,&coins)||!record(m->args[1],&next)||next.type!=p->adoptType||strcmp(next.name,p->adoptName))return false;
  if(!put(p,&next,owned(p))){fail(p,"The puffle list is full. Reopen your igloo to refresh it.");}
  p->coins=coins;p->coinsChanged=true;p->action=PUFFLE_IDLE;p->actionAge=0;return true;
 }
 if(!strcmp(m->command,"e")&&p->action!=PUFFLE_IDLE){p->action=PUFFLE_IDLE;p->actionAge=0;fail(p,"The server refused the puffle action.");return true;}
 return false;
}
bool puffles_visible(const Puffles *p,const Puffle *pet,const Demo *world){return pet->walking?actor(world,pet->owner)!=NULL:p->roomOwner&&pet->owner==p->roomOwner;}
void puffles_departed(Puffles *p,unsigned owner){
 if(owner==p->mine||owner==p->roomOwner)return;
 for(unsigned i=0;i<p->count;)if(p->pets[i].owner==owner)p->pets[i]=p->pets[--p->count];else i++;
 for(unsigned i=0;i<p->nWalks;)if(p->walks[i].owner==owner)p->walks[i]=p->walks[--p->nWalks];else i++;
}
static bool start(Puffles *p,PuffleAction action,unsigned id,unsigned item,const char *command,unsigned count,const char *const *args){
 if(!p->mine||p->action!=PUFFLE_IDLE)return false;
 p->error[0]=0;if(!send(p,command,count,args))return false;
 p->action=action;p->actionId=id;p->actionItem=item;p->actionAge=0;return true;
}
bool puffles_adopt(Puffles *p,unsigned type,const char *name){
 if(type>=PUFFLES_COLOURS||!name||!*name||strlen(name)>12||owned(p)>=PUFFLES_OWNED)return false;
 for(const char *c=name;*c;c++)if(!(*c>='A'&&*c<='Z')&&!(*c>='a'&&*c<='z')&&!(*c>='0'&&*c<='9')&&*c!=' ')return false;
 bool nonspace=false;for(const char *c=name;*c;c++)nonspace|=*c!=' ';if(!nonspace)return false;
 if(!start(p,PUFFLE_CHECK_NAME,0,0,"p#pcn",1,(const char *[]){name}))return false;
 p->adoptType=type;strcpy(p->adoptName,name);return true;
}
bool puffles_walk(Puffles *p,unsigned id,bool walk){
 Puffle *pet=puffles_find(p,p->mine,id);if(!pet||pet->walking==walk)return false;
 char text[16];snprintf(text,sizeof text,"%u",id);return start(p,PUFFLE_WALK,id,0,"p#pw",2,(const char *[]){text,walk?"1":"0"});
}
bool puffles_care(Puffles *p,unsigned id,unsigned item){
 if(!puffles_find(p,p->mine,id)||(item!=3&&item!=4&&item!=5&&item!=8&&item!=27&&item!=37))return false;
 char text[16],care[16];snprintf(text,sizeof text,"%u",id);snprintf(care,sizeof care,"%u",item);return start(p,PUFFLE_CARE,id,item,"p#pcid",2,(const char *[]){text,care});
}
bool puffles_take_coins(Puffles *p,unsigned *coins){if(!p->coinsChanged)return false;if(coins)*coins=p->coins;p->coinsChanged=false;return true;}
void puffles_step(Puffles *p,const Demo *world,float elapsed){
 if(!isfinite(elapsed)||elapsed<=0)return;
 float dt=fminf(elapsed,.25f);if(p->snapshotPending){float before=p->snapshotAge;p->snapshotAge+=dt;if(before<15&&p->snapshotAge>=15)fail(p,"The puffle list timed out. Reconnect if it does not arrive.");}
 if(p->action!=PUFFLE_IDLE){float before=p->actionAge;p->actionAge+=dt;if(before<15&&p->actionAge>=15)fail(p,"The puffle action timed out. Reconnect before trying again.");}
 p->accumulator+=dt;
 while(p->accumulator>=1.0f/24){
  p->accumulator-=1.0f/24;
  for(unsigned i=0;i<p->count;i++){
   Puffle *pet=&p->pets[i];if(pet->owner!=p->mine&&!puffles_visible(p,pet,world))continue;
   pet->oldX=pet->x;pet->oldY=pet->y;pet->clock++;unsigned frame=1;
   if(pet->reaction){pet->reactionTicks++;if(pet->reactionTicks>=(pet->reaction==2?120:48))pet->reaction=0;else frame=pet->reaction==2?24+pet->direction:pet->reaction==3?16+pet->direction:33+(pet->reactionTicks/6)%3;}
   if(!pet->reaction&&pet->walking){const Actor *owner=actor(world,pet->owner);if(owner){float dx=owner->x-20-pet->x,dy=owner->y+8-pet->y,distance=hypotf(dx,dy);if(distance>5){float step=fminf(distance,6);int angle=(int)(atan2f(dy,dx)*57.29577951308232f-90);if(angle<0)angle+=360;unsigned direction=(unsigned)floorf(angle/45.0f+.5f)+1;pet->direction=direction>8?1:direction;frame=pet->direction+8;pet->x+=dx/distance*step;pet->y+=dy/distance*step;}}}
   if(frame!=pet->frame){pet->frame=frame;pet->clock=0;}
  }
 }
}
