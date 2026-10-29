#include "game_fire_model.h"
#include "game_card_catalog.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static bool integer(const char *text,int min,int max,int *value){
 if(!text||!*text||strlen(text)>10)return false;
 char *end;long n=strtol(text,&end,10);if(*end||n<min||n>max)return false;for(const char *p=text;*p;p++)if((*p<'0'||*p>'9')&&!(p==text&&*p=='-'))return false;*value=(int)n;return true;
}
static unsigned list(const char *text,int *out,unsigned capacity,int min,int max){
 if(!text||!*text||strlen(text)>255)return 0;
 char buffer[256];strcpy(buffer,text);char *part=buffer;unsigned count=0;
 while(part&&*part){if(count==capacity)return 0;char *end=strchr(part,',');if(end)*end=0;if(!integer(part,min,max,&out[count++]))return 0;part=end?end+1:NULL;if(part&&!*part)return 0;}return count;
}
static bool cards(const char *text,unsigned *out){int values[5];if(list(text,values,5,1,99999)!=5)return false;for(unsigned i=0;i<5;i++)if(!card_info((unsigned)values[i]))return false;for(unsigned i=0;i<5;i++)out[i]=(unsigned)values[i];return true;}
static unsigned names(const char *text,char out[4][40]){
 if(!text||!*text||strlen(text)>159)return 0;
 char buffer[160];strcpy(buffer,text);char *part=buffer;unsigned count=0;
 while(part){if(count==4)return 0;char *end=strchr(part,',');if(end)*end=0;if(!*part||strlen(part)>=40)return 0;strcpy(out[count++],part);part=end?end+1:NULL;}return count;
}
void fire_model_init(FireModel *m){memset(m,0,sizeof *m);m->phase=FIRE_MENU;m->seat=-1;m->trump='b';snprintf(m->message,sizeof m->message,"Choose a Card-Jitsu Fire match.");}
void fire_model_begin(FireModel *m,bool sensei){bool ready=m->menuReady;unsigned rank=m->rank,percentage=m->percentage;fire_model_init(m);m->menuReady=ready;m->rank=rank;m->percentage=percentage;m->mode=sensei?1:0;m->phase=sensei?FIRE_HANDSHAKE:FIRE_QUEUE;snprintf(m->message,sizeof m->message,sensei?"Challenging Fire Sensei...":"Finding Fire opponents...");}
static void board(FireModel *m){m->phase=m->seat==(int)m->active?FIRE_BOARD:FIRE_WAIT;m->inputPending=false;m->elapsed=0;memset(m->participants,0,sizeof m->participants);memset(m->played,0,sizeof m->played);snprintf(m->message,sizeof m->message,m->phase==FIRE_BOARD?"Your turn. Spin the stone.":"Waiting for the active player...");}
static bool snapshot(FireModel *m,unsigned count,char *const *a){
 if(count<8||count>10||m->seat<0||m->started)return false;
 int active;char playerNames[4][40]={{0}};unsigned players=names(a[1],playerNames),hand[5];int colours[4],energies[4],positions[4],spin[3];
 if(players<2||!integer(a[0],0,(int)players-1,&active)||(unsigned)m->seat>=players||list(a[2],colours,4,-1,15)!=players||list(a[3],energies,4,0,99)!=players||list(a[4],positions,4,0,15)!=players||list(a[6],spin,3,0,15)!=3||spin[0]<1||spin[0]>6||!cards(a[5],hand))return false;
 m->players=players;m->active=(unsigned)active;memcpy(m->names,playerNames,sizeof m->names);memcpy(m->hand,hand,sizeof m->hand);
 for(unsigned i=0;i<players;i++){m->colour[i]=colours[i]<0?14:(unsigned)colours[i];m->energy[i]=(unsigned)energies[i];m->position[i]=(unsigned)positions[i];}
 for(unsigned i=0;i<3;i++)m->spin[i]=(unsigned)spin[i];
 m->started=true;m->round=1;board(m);
 while(m->deferredCount){FireDeferred d=m->deferred[0];memmove(&m->deferred[0],&m->deferred[1],(--m->deferredCount)*sizeof d);char *args[10];for(unsigned i=0;i<d.count;i++)args[i]=d.args[i];if(!fire_model_packet(m,"zm",d.count,args))return false;}
 return true;
}
static bool move(FireModel *m,unsigned count,char *const *a){
 if(!count)return false;
 int number,values[5];
 if(!strcmp(a[0],"is")){if(count!=3||!integer(a[2],0,6,&number))return false;if(m->active==(unsigned)m->seat)m->phase=FIRE_MOVE;m->inputPending=false;return true;}
 if(!strcmp(a[0],"ub")){
  if(count<3||!integer(a[1],0,(int)m->players-1,&number)||list(a[2],values,4,0,15)!=m->players)return false;
  for(unsigned i=0;i<m->players;i++)m->position[i]=(unsigned)values[i];
  m->phase=FIRE_WAIT;m->inputPending=false;return true;
 }
 if(!strcmp(a[0],"ct")){m->phase=m->active==(unsigned)m->seat?FIRE_TRUMP:FIRE_WAIT;m->inputPending=false;return true;}
 if(!strcmp(a[0],"co")){
  if(count!=3)return false;
  unsigned n=list(a[2],values,4,0,(int)m->players-1);if(!n)return false;memset(m->opponents,0,sizeof m->opponents);for(unsigned i=0;i<n;i++)if(values[i]!=m->seat&&m->energy[values[i]])m->opponents[values[i]]=true;m->phase=m->active==(unsigned)m->seat?FIRE_OPPONENT:FIRE_WAIT;m->inputPending=false;return true;
 }
 if(!strcmp(a[0],"sb")){
  if(count!=4||(!strcmp(a[1],"bt")?(strlen(a[3])!=1||!strchr("fws",a[3][0])):strcmp(a[1],"be")))return false;
  unsigned n=list(a[2],values,4,0,(int)m->players-1);if(n<2)return false;bool participants[4]={0};for(unsigned i=0;i<n;i++){if(participants[values[i]])return false;participants[values[i]]=true;}memcpy(m->participants,participants,sizeof participants);
  m->trump=!strcmp(a[1],"be")?'b':a[3][0];m->phase=m->participants[m->seat]?FIRE_SELECT:FIRE_WAIT;m->inputPending=false;m->elapsed=0;snprintf(m->message,sizeof m->message,m->phase==FIRE_SELECT?"Choose a battle card.":"Watching this battle...");return true;
 }
 if(!strcmp(a[0],"ic")){return count==2&&integer(a[1],0,(int)m->players-1,&number);}
 if(!strcmp(a[0],"rb")){
  int seats[4],types[4],energies[4],results[4],podium[4];unsigned hand[5];if(count!=8)return false;unsigned n=list(a[1],seats,4,0,(int)m->players-1);
  if(n<2||list(a[2],types,4,1,99999)!=n||list(a[3],energies,4,0,99)!=n||list(a[4],results,4,1,4)!=n||list(a[7],podium,4,1,4)!=m->players||!cards(a[6],hand))return false;
  bool seen[4]={0};for(unsigned i=0;i<n;i++){if(!card_info((unsigned)types[i])||seen[seats[i]])return false;seen[seats[i]]=true;}
  memset(m->played,0,sizeof m->played);for(unsigned i=0;i<n;i++){m->played[seats[i]]=(unsigned)types[i];m->energy[seats[i]]=(unsigned)energies[i];m->result[seats[i]]=(unsigned)results[i];}
  for(unsigned i=0;i<m->players;i++)m->standing[i]=(unsigned)podium[i];
  memcpy(m->hand,hand,sizeof m->hand);m->phase=FIRE_REVEAL;m->elapsed=0;m->inputPending=false;m->requestReady=true;m->readySent=false;unsigned result=m->result[m->seat];snprintf(m->message,sizeof m->message,!m->played[m->seat]?"Battle complete.":result==1?"You lost 1 energy.":result==2?"You tied this battle.":"You won this battle.");return true;
 }
 if(!strcmp(a[0],"nt")){
  int spin[3];unsigned hand[5];if(count!=4||!integer(a[1],0,(int)m->players-1,&number)||list(a[2],spin,3,0,15)!=3||spin[0]<1||spin[0]>6||!cards(a[3],hand))return false;
  m->active=(unsigned)number;for(unsigned i=0;i<3;i++)m->spin[i]=(unsigned)spin[i];memcpy(m->hand,hand,sizeof m->hand);m->round++;m->requestReady=false;m->readySent=false;board(m);return true;
 }
 if(!strcmp(a[0],"tb")){m->phase=FIRE_WAIT;m->inputPending=false;return true;}
 if(!strcmp(a[0],"nr")){if(count!=3||!integer(a[2],0,5,&number))return false;m->rank=(unsigned)number;m->rankAwarded=true;return true;}
 if(!strcmp(a[0],"zo")){
  if(count!=2||list(a[1],values,4,1,4)!=m->players)return false;
  for(unsigned i=0;i<m->players;i++)m->standing[i]=(unsigned)values[i];
  m->phase=FIRE_OVER;m->requestReady=false;m->inputPending=false;snprintf(m->message,sizeof m->message,"Match finished. Your progress is saved.");return true;
 }
 if(!strcmp(a[0],"cz")){if(count!=2||!integer(a[1],0,(int)m->players-1,&number))return false;m->energy[number]=0;return true;}
 return false;
}
bool fire_model_packet(FireModel *m,const char *command,unsigned count,char *const *args){
 if(!m||!command||count>10||(count&&!args))return false;
 for(unsigned i=0;i<count;i++)if(!args[i]||strlen(args[i])>255)return false;
 int value;
 if(!strcmp(command,"jg")){if(count!=1||!integer(args[0],953,953,&value))return false;m->menuReady=true;return true;}
 if(!strcmp(command,"tmm"))return m->phase==FIRE_QUEUE;
 if(!strcmp(command,"scard")){if(count<4||!integer(args[0],997,997,&value)||m->started)return false;m->phase=FIRE_HANDSHAKE;m->elapsed=0;m->requestGet=true;return true;}
 if(!strcmp(command,"jz")){if(count!=1||!integer(args[0],0,3,&value))return false;m->seat=value;return true;}
 if(!strcmp(command,"sz"))return snapshot(m,count,args);
 if(!strcmp(command,"zm")){
  if(!count)return false;
  if(m->phase==FIRE_OVER)return false;
  if(!m->started){
   if(m->phase!=FIRE_HANDSHAKE||m->deferredCount==FIRE_DEFERRED)return false;
   for(unsigned i=0;i<count;i++)if(strlen(args[i])>=sizeof m->deferred[0].args[i])return false;
   FireDeferred *d=&m->deferred[m->deferredCount++];d->count=count;for(unsigned i=0;i<count;i++)strcpy(d->args[i],args[i]);return true;
  }
  return move(m,count,args);
 }
 if(!strcmp(command,"cz")){if(!m->started||m->seat<0)return false;m->phase=FIRE_OVER;m->requestReady=false;m->standing[m->seat]=1;snprintf(m->message,sizeof m->message,"The other player left. Progress saved.");return true;}
 return false;
}
bool fire_model_playable(const FireModel *m,unsigned slot){
 if(m->phase!=FIRE_SELECT||m->inputPending||slot>=5||m->seat<0)return false;
 const CardInfo *card=card_info(m->hand[slot]);if(!card)return false;
 if(m->trump=='b'||card->element==m->trump)return true;
 for(unsigned i=0;i<5;i++){const CardInfo *other=card_info(m->hand[i]);if(other&&other->element==m->trump)return false;}return true;
}
bool fire_model_destination(const FireModel *m,unsigned tile){return m->phase==FIRE_MOVE&&!m->inputPending&&m->seat>=0&&m->active==(unsigned)m->seat&&(tile==m->spin[1]||tile==m->spin[2]);}
