#include "game_card_model.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool number(const char *text,unsigned max,unsigned *out){
 if(!text||!*text||strlen(text)>10)return false;
 unsigned n=0;for(const char *p=text;*p;p++){if(*p<'0'||*p>'9'||n>(max-(unsigned)(*p-'0'))/10u||(unsigned)(*p-'0')>max)return false;n=n*10u+(unsigned)(*p-'0');}
 *out=n;return true;
}
static bool element(char c){return c=='f'||c=='w'||c=='s';}
static bool colour(char c){return c&&strchr("bgopry",c)!=NULL;}
static CardItem *find(CardModel *m,unsigned id){for(unsigned i=0;i<m->count;i++)if(m->cards[i].session==id)return &m->cards[i];return NULL;}
const CardItem *card_model_find(const CardModel *m,unsigned id){for(unsigned i=0;i<m->count;i++)if(m->cards[i].session==id)return &m->cards[i];return NULL;}
static bool fields(const char *text,char *storage,size_t cap,char **parts,unsigned amount){
 if(!text||strlen(text)>=cap)return false;
 strcpy(storage,text);char *p=storage;
 for(unsigned i=0;i<amount;i++){parts[i]=p;char *end=strchr(p,'|');if(i+1==amount)return end==NULL;if(!end)return false;*end=0;p=end+1;}
 return false;
}
static bool parse_card(const char *text,CardItem *card,unsigned seat){
 char storage[96],*p[6];CardItem value={.seat=seat,.held=true};
 if(!fields(text,storage,sizeof storage,p,6)||!number(p[0],INT_MAX,&value.session)||!value.session||!number(p[1],99999,&value.type)||!value.type||strlen(p[2])!=1||!element(p[2][0])||!number(p[3],99,&value.value)||!value.value||strlen(p[4])!=1||!colour(p[4][0])||!number(p[5],255,&value.power))return false;
 value.element=p[2][0];value.colour=p[4][0];*card=value;return true;
}
void card_model_init(CardModel *m){memset(m,0,sizeof *m);m->phase=CARD_MENU;m->seat=m->winner=m->matchWinner=-1;snprintf(m->message,sizeof m->message,"Choose a Card-Jitsu match.");}
void card_model_begin(CardModel *m,bool sensei){bool ready=m->menuReady;card_model_init(m);m->menuReady=ready;m->mode=sensei?1:0;m->phase=sensei?CARD_HANDSHAKE:CARD_QUEUE;snprintf(m->message,sizeof m->message,sensei?"Challenging Sensei...":"Waiting for another ninja...");}
static bool player(CardModel *m,const char *text){
 char storage[112],*p[4];unsigned seat,colourId,rank;
 if(!fields(text,storage,sizeof storage,p,4)||!number(p[0],1,&seat)||!p[1][0]||strlen(p[1])>=sizeof m->names[seat]||!number(p[2],15,&colourId)||!number(p[3],10,&rank))return false;
 snprintf(m->names[seat],sizeof m->names[seat],"%s",p[1]);m->colour[seat]=colourId;m->rank[seat]=rank;return true;
}
static bool deal(CardModel *m,unsigned count,char *const *args){
 unsigned seat;if(count<3||!number(args[1],1,&seat)||count-2>CARD_HAND||m->hands[seat]+count-2>CARD_HAND||m->count+count-2>CARD_HISTORY)return false;
 CardItem parsed[CARD_HAND];
 for(unsigned i=2;i<count;i++){
  if(!parse_card(args[i],&parsed[i-2],seat)||find(m,parsed[i-2].session))return false;
  for(unsigned j=2;j<i;j++)if(parsed[j-2].session==parsed[i-2].session)return false;
 }
 // Another client can finish its battle animation first. Its next deal marks
 // the new round; do not discard the pick that can follow in the same batch.
 if(m->phase==CARD_REVEAL)card_model_next_round(m);
 for(unsigned i=2;i<count;i++){m->cards[m->count++]=parsed[i-2];m->hand[seat][m->hands[seat]++]=parsed[i-2].session;}
 if((int)seat==m->seat&&m->phase==CARD_DEALING){m->phase=CARD_SELECT;m->elapsed=0;snprintf(m->message,sizeof m->message,"Choose your card.");}
 return true;
}
static bool pick(CardModel *m,unsigned count,char *const *args){
 unsigned seat,id;if(count!=3||!number(args[1],1,&seat)||!number(args[2],INT_MAX,&id)||m->picked[seat])return false;
 CardItem *card=find(m,id);if(!card||!card->held||card->seat!=seat)return false;
 unsigned index=0;while(index<m->hands[seat]&&m->hand[seat][index]!=id)index++;if(index==m->hands[seat])return false;
 memmove(&m->hand[seat][index],&m->hand[seat][index+1],(m->hands[seat]-index-1)*sizeof(unsigned));m->hands[seat]--;card->held=false;m->picked[seat]=id;
 if((int)seat==m->seat){m->phase=CARD_PICK_WAIT;m->choicePending=false;snprintf(m->message,sizeof m->message,"Waiting for the other card...");}
 return true;
}
static bool power(CardModel *m,unsigned count,char *const *args){
 unsigned sender,recipient,id;if(count<4||!number(args[1],1,&sender)||!number(args[2],1,&recipient)||!number(args[3],18,&id)||!id)return false;
 unsigned discards[CARD_HISTORY];if(count-4>CARD_HISTORY)return false;
 for(unsigned i=4;i<count;i++)if(args[i][0]&&(!number(args[i],INT_MAX,&discards[i-4])||!find(m,discards[i-4])))return false;
 for(unsigned i=4;i<count;i++)if(args[i][0])find(m,discards[i-4])->scored=false;
 if(id==1)m->nextSwap=true;
 if(id==2)m->nextModifier[recipient]+=2;
 if(id==3)m->nextModifier[recipient]-=2;
 if(id>=13&&id<=15)m->nextBlock[recipient]=id==13?'s':id==14?'f':'w';
 m->powerId=id;return true;
}
bool card_model_packet(CardModel *m,const char *command,unsigned count,char *const *args){
 if(!m||!command||count>520||(count&&!args))return false;
 for(unsigned i=0;i<count;i++)if(!args[i]||strlen(args[i])>2048)return false;
 if(!strcmp(command,"jg")){unsigned room;if(count!=1||!number(args[0],9999,&room)||room!=951)return false;m->menuReady=true;return true;}
 if(!strcmp(command,"tmm")){if(m->phase!=CARD_QUEUE)return false;m->elapsed=0;return true;}
 if(!strcmp(command,"scard")){
  unsigned room,players;if(count<4||!number(args[0],9999,&room)||room!=998||!number(args[2],2,&players)||!players||m->started)return false;
  m->phase=CARD_HANDSHAKE;m->requestGet=true;m->elapsed=0;return true;
 }
 if(!strcmp(command,"gz")){unsigned players;return count==2&&number(args[0],2,&players)&&players>0;}
 if(!strcmp(command,"jz")){
  unsigned seat,colourId,rank;if(count!=4||!number(args[0],1,&seat)||!args[1][0]||strlen(args[1])>=sizeof m->names[seat]||!number(args[2],15,&colourId)||!number(args[3],10,&rank))return false;
  m->seat=(int)seat;m->rank[seat]=rank;m->colour[seat]=colourId;snprintf(m->names[seat],sizeof m->names[seat],"%s",args[1]);m->requestSeats=true;return true;
 }
 if(!strcmp(command,"uz")){if(count<1||count>2)return false;CardModel copy=*m;for(unsigned i=0;i<count;i++)if(!player(&copy,args[i]))return false;*m=copy;return true;}
 if(!strcmp(command,"sz")){
  if(m->seat<0||m->started||(count&&!(count==1&&!args[0][0])))return false;
  m->started=true;m->round=1;m->phase=CARD_DEALING;m->requestDeal=true;m->dealAmount=5;return true;
 }
 if(!strcmp(command,"zm")){
  if(count<1||(!m->started&&m->phase!=CARD_HANDSHAKE)||m->phase==CARD_OVER)return false;
  if(!strcmp(args[0],"deal"))return deal(m,count,args);
  if(!strcmp(args[0],"pick"))return pick(m,count,args);
  if(!strcmp(args[0],"power"))return power(m,count,args);
  if(!strcmp(args[0],"judge")){
   int winner=-1;unsigned n;if(count!=2||(!number(args[1],1,&n)&&strcmp(args[1],"-1"))||!m->picked[0]||!m->picked[1]||m->phase==CARD_REVEAL)return false;
   if(strcmp(args[1],"-1"))winner=(int)n;
   if(winner>=0)find(m,m->picked[winner])->scored=true;
   m->winner=winner;m->phase=CARD_REVEAL;m->elapsed=0;m->revealTime=5;
   snprintf(m->message,sizeof m->message,winner<0?"Tie! Choose again next round.":winner==m->seat?"You won this round!":"Your opponent won this round.");return true;
  }
  return false;
 }
 if(!strcmp(command,"cza")){unsigned rank;if(count!=1||!number(args[0],10,&rank))return false;m->newRank=rank;m->rankAwarded=true;if(m->seat>=0)m->rank[m->seat]=rank;return true;}
 if(!strcmp(command,"cjsi")){unsigned have,total;if(count<3||!number(args[1],10000,&have)||!number(args[2],10000,&total))return false;m->stampInfo=true;m->stampCount=have;m->stampTotal=total;return true;}
 if(!strcmp(command,"czo")){
  unsigned coins,winner;if(count<2||!number(args[0],INT_MAX,&coins)||!number(args[1],1,&winner))return false;
  m->matchWinner=(int)winner;m->phase=CARD_OVER;m->requestDeal=false;snprintf(m->message,sizeof m->message,winner==(unsigned)m->seat?"You won the match!":"Match finished. Keep training!");return true;
 }
 if(!strcmp(command,"cz")||!strcmp(command,"lz")){m->phase=CARD_OVER;m->requestDeal=false;snprintf(m->message,sizeof m->message,"The other player left the match.");return true;}
 return false;
}
bool card_model_playable(const CardModel *m,unsigned slot){
 if(m->seat<0||m->phase!=CARD_SELECT||m->choicePending||slot>=m->hands[m->seat])return false;
 const CardItem *card=card_model_find(m,m->hand[m->seat][slot]);return card&&card->element!=m->blocked[m->seat];
}
bool card_model_choose(CardModel *m,unsigned slot,unsigned *session){if(!card_model_playable(m,slot))return false;*session=m->hand[m->seat][slot];m->choicePending=true;return true;}
void card_model_choice_failed(CardModel *m){m->choicePending=false;}
void card_model_next_round(CardModel *m){
 if(m->phase!=CARD_REVEAL)return;
 m->round++;m->phase=CARD_DEALING;m->picked[0]=m->picked[1]=0;m->winner=-1;m->powerId=0;m->elapsed=0;
 memcpy(m->blocked,m->nextBlock,sizeof m->blocked);memset(m->nextBlock,0,sizeof m->nextBlock);
 memcpy(m->modifier,m->nextModifier,sizeof m->modifier);memset(m->nextModifier,0,sizeof m->nextModifier);m->swap=m->nextSwap;m->nextSwap=false;
 m->requestDeal=true;m->dealAmount=1;
}
unsigned card_model_score(const CardModel *m,unsigned seat,char e){
 unsigned colours=0;if(seat>1)return 0;for(unsigned i=0;i<m->count;i++){const CardItem *c=&m->cards[i];if(c->seat==seat&&c->scored&&c->element==e){const char *p=strchr("bgopry",c->colour);if(p)colours|=1u<<(unsigned)(p-"bgopry");}}
 unsigned n=0;while(colours){n+=colours&1u;colours>>=1;}return n;
}
