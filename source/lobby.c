#include "lobby.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool number(const char *text,unsigned max,unsigned *out){
 if(!text||!*text)return false;
 unsigned n=0;for(const unsigned char *p=(const unsigned char *)text;*p;p++){
  if(*p<'0'||*p>'9'||n>max/10||(n==max/10&&(unsigned)(*p-'0')>max%10))return false;
  n=n*10+(unsigned)(*p-'0');
 }*out=n;return true;
}
static bool name_valid(const char *name){
 if(!name||strlen(name)>=LOBBY_NAME)return false;
 for(const unsigned char *p=(const unsigned char *)name;*p;p++)if(*p<32||*p==127||*p=='|'||*p==',')return false;
 return true;
}
const char *lobby_key(unsigned room,LobbyKind kind,unsigned id){
 if(kind==LOBBY_TABLE){
  if(room==111&&id>=100&&id<=104)return "mancala";
  if((room==220&&id>=205&&id<=207)||(room==221&&id>=200&&id<=204))return "four";
 }else if(kind==LOBBY_WADDLE&&room==230&&id>=100&&id<=103)return "sled";
 return NULL;
}
void lobby_init(Lobby *l,LobbySend send,void *user){memset(l,0,sizeof *l);l->send=send;l->user=user;l->seat=-1;}
const LobbyEntry *lobby_entry(const Lobby *l,unsigned id){for(unsigned i=0;i<l->entryCount;i++)if(l->entries[i].id==id)return &l->entries[i];return NULL;}
static LobbyEntry *entry(Lobby *l,unsigned id){for(unsigned i=0;i<l->entryCount;i++)if(l->entries[i].id==id)return &l->entries[i];return NULL;}
bool lobby_enter(Lobby *l,unsigned room,const char *username){
 if(l->needsLeave||(l->state!=LOBBY_IDLE&&l->state!=LOBBY_FAILED)||!name_valid(username))return false;
 LobbySend send=l->send;void *user=l->user;lobby_init(l,send,user);l->room=room;strcpy(l->username,username);
 l->kind=room==230?LOBBY_WADDLE:LOBBY_TABLE;
 unsigned first=room==111?100:room==220?205:room==221?200:room==230?100:0;
 l->entryCount=room==111||room==221?5:room==220?3:room==230?4:0;
 for(unsigned i=0;i<l->entryCount;i++){l->entries[i].id=first+i;l->entries[i].capacity=room==230?(i==0?4:i==1?3:2):2;}
 return true;
}
bool lobby_refresh(Lobby *l){
 if(!l->entryCount||!l->send||l->state==LOBBY_LAUNCHED||(l->state==LOBBY_READY&&l->kind==LOBBY_WADDLE))return false;
 char ids[LOBBY_ENTRIES][12];const char *args[LOBBY_ENTRIES];
 for(unsigned i=0;i<l->entryCount;i++){snprintf(ids[i],sizeof ids[i],"%u",l->entries[i].id);args[i]=ids[i];}
 return l->send(l->user,l->kind==LOBBY_TABLE?'s':'z',l->kind==LOBBY_TABLE?"gt":"gw",l->entryCount,args);
}
bool lobby_begin(Lobby *l,LobbyKind kind,unsigned id){
 if((l->state!=LOBBY_IDLE&&l->state!=LOBBY_FAILED)||l->needsLeave||kind!=l->kind||!lobby_key(l->room,kind,id))return false;
 const LobbyEntry *e=lobby_entry(l,id);if(!e||(kind==LOBBY_WADDLE&&((e->known&&e->count>=e->capacity)||!l->username[0])))return false;
 char value[12];snprintf(value,sizeof value,"%u",id);const char *args[]={value};
 l->selected=id;l->seat=-1;l->state=LOBBY_JOINING;l->elapsed=0;l->error=NULL;l->needsLeave=true;
 if(!l->send||!l->send(l->user,kind==LOBBY_TABLE?'s':'z',kind==LOBBY_TABLE?"jt":"jw",1,args)){l->state=LOBBY_FAILED;l->error="Could not send lobby join";l->needsLeave=false;return false;}
 return true;
}
static bool population(Lobby *l,const char *record){
 if(!record)return false;
 const char *bar=strchr(record,'|');if(!bar||bar==record||(size_t)(bar-record)>=12)return false;
 char idText[12];memcpy(idText,record,(size_t)(bar-record));idText[bar-record]=0;
 unsigned id;if(!number(idText,1000,&id))return false;LobbyEntry *e=entry(l,id);if(!e)return false;
 if(l->kind==LOBBY_TABLE){unsigned count;if(!number(bar+1,2,&count))return false;e->count=count;e->known=true;return true;}
 LobbyEntry next=*e;memset(next.names,0,sizeof next.names);next.count=0;const char *part=bar+1;
 for(unsigned seat=0;seat<e->capacity;seat++){
  const char *comma=strchr(part,',');size_t size=comma?(size_t)(comma-part):strlen(part);
  if(size>=LOBBY_NAME||(seat+1<e->capacity&&!comma)||(seat+1==e->capacity&&comma))return false;
  memcpy(next.names[seat],part,size);next.names[seat][size]=0;if(!name_valid(next.names[seat]))return false;
  next.count+=size!=0;if(comma)part=comma+1;
 }next.known=true;*e=next;return true;
}
bool lobby_packet(Lobby *l,const char *command,unsigned count,char *const *args){
 if(!command||count>16||(count&&!args)||!l->entryCount||l->state==LOBBY_LAUNCHED)return false;
 for(unsigned i=0;i<count;i++)if(!args[i])return false;
 if(!strcmp(command,l->kind==LOBBY_TABLE?"gt":"gw")){
  if(count>l->entryCount)return false;
  bool changed=false;for(unsigned i=0;i<count;i++)changed=population(l,args[i])||changed;return changed;
 }
 if(l->kind==LOBBY_TABLE&&!strcmp(command,"ut")){
  unsigned id,n;if(count!=2||!number(args[0],1000,&id)||!number(args[1],2,&n))return false;
  LobbyEntry *e=entry(l,id);if(!e)return false;e->count=n;e->known=true;return true;
 }
 if(l->kind==LOBBY_WADDLE&&!strcmp(command,"uw")){
  unsigned id,seat;if(count<2||count>4||!number(args[0],1000,&id)||!number(args[1],LOBBY_SEATS-1,&seat))return false;
  LobbyEntry *e=entry(l,id);const char *name=count>=3?args[2]:"";if(!e||seat>=e->capacity||!name_valid(name))return false;
  strcpy(e->names[seat],name);e->count=0;for(unsigned i=0;i<e->capacity;i++)e->count+=e->names[i][0]!=0;
  if(e->count==e->capacity)e->known=true;
  return true;
 }
 if(l->state==LOBBY_JOINING&&l->kind==LOBBY_TABLE&&!strcmp(command,"jt")){
  unsigned id,seat;if(count!=2||!number(args[0],1000,&id)||id!=l->selected||!number(args[1],99,&seat)||(seat!=1&&seat!=2&&seat!=99))return false;
  l->seat=(int)seat;l->state=LOBBY_READY;l->elapsed=0;return true;
 }
 if(l->state==LOBBY_JOINING&&l->kind==LOBBY_WADDLE&&!strcmp(command,"jw")){
  unsigned seat;const LobbyEntry *e=lobby_entry(l,l->selected);if(count!=1||!number(args[0],LOBBY_SEATS-1,&seat)||!e||seat>=e->capacity)return false;
  l->seat=(int)seat;l->state=LOBBY_WAITING;l->elapsed=0;return true;
 }
 if(l->state==LOBBY_WAITING&&l->kind==LOBBY_WADDLE&&!strcmp(command,"jg")){
  unsigned id;if(count!=1||!number(args[0],999,&id)||id!=999)return false;
  l->state=LOBBY_READY;l->needsLeave=false;l->elapsed=0;return true;
 }
 return false;
}
bool lobby_cancel(Lobby *l){
 if(l->state==LOBBY_LAUNCHED||(l->state==LOBBY_READY&&l->kind==LOBBY_WADDLE))return false;
 if(l->needsLeave&&(!l->send||!l->send(l->user,l->kind==LOBBY_TABLE?'s':'z',l->kind==LOBBY_TABLE?"lt":"lw",0,NULL))){l->state=LOBBY_FAILED;l->error="Could not leave lobby";return false;}
 l->needsLeave=false;l->state=LOBBY_IDLE;l->selected=0;l->seat=-1;l->elapsed=0;l->error=NULL;return true;
}
void lobby_tick(Lobby *l,float seconds){
 if(!isfinite(seconds)||seconds<=0||(l->state!=LOBBY_JOINING&&l->state!=LOBBY_WAITING))return;
 float limit=l->state==LOBBY_JOINING?LOBBY_JOIN_TIMEOUT:LOBBY_QUEUE_TIMEOUT;
 l->elapsed+=seconds;if(l->elapsed<limit)return;
 const char *reason=l->state==LOBBY_JOINING?"Lobby join timed out":"Lobby queue timed out";
 if(lobby_cancel(l))l->error=reason;
 l->state=LOBBY_FAILED;
}
bool lobby_take(Lobby *l,DescriptorLobbyLaunch *launch){
 if(l->state!=LOBBY_READY||!launch)return false;
 const char *key=lobby_key(l->room,l->kind,l->selected);if(!key)return false;
 memset(launch,0,sizeof *launch);strcpy(launch->key,key);strcpy(launch->username,l->username);
 launch->kind=l->kind;launch->contextId=l->selected;launch->seat=l->seat;launch->alreadyJoined=l->kind==LOBBY_WADDLE;
 l->state=LOBBY_LAUNCHED;l->needsLeave=false;return true;
}
void lobby_release(Lobby *l){if(l->state==LOBBY_LAUNCHED){l->state=LOBBY_IDLE;l->selected=0;l->seat=-1;l->error=NULL;l->elapsed=0;}}
