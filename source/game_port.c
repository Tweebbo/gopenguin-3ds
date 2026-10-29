#include "game_controller.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_BUTTONS 12
#define GAME_JOIN_TIMEOUT 10.0f
#define GAME_PAY_TIMEOUT 15.0f

extern const GameController GAME_HYDRO_CONTROLLER,GAME_ROUNDUP_CONTROLLER,GAME_BEANS_CONTROLLER,GAME_CART_CONTROLLER,GAME_THINICE_CONTROLLER,GAME_FOUR_CONTROLLER,GAME_MANCALA_CONTROLLER,GAME_SLED_CONTROLLER,GAME_PIZZATRON_CONTROLLER,GAME_FISH_CONTROLLER;
#ifdef GP_NINJA_GAMES
extern const GameController GAME_CARD_CONTROLLER,GAME_FIRE_CONTROLLER;
#endif
static const GameController *const CONTROLLERS[]={&GAME_HYDRO_CONTROLLER,&GAME_ROUNDUP_CONTROLLER,&GAME_BEANS_CONTROLLER,&GAME_CART_CONTROLLER,&GAME_FOUR_CONTROLLER,&GAME_MANCALA_CONTROLLER,&GAME_SLED_CONTROLLER,&GAME_PIZZATRON_CONTROLLER,&GAME_FISH_CONTROLLER,
#ifdef GP_NINJA_GAMES
 &GAME_CARD_CONTROLLER,&GAME_FIRE_CONTROLLER,
#endif
// Thin Ice's 285-cell board needs the SWL override table proposed in build/games/swl-override-hash.patch.
#ifdef GAME_THINICE_READY
 &GAME_THINICE_CONTROLLER,
#endif
};
static const char *const KEYS[]={"hydro","roundup","beans","cart","thinice","fish","four","mancala","sled","pizzatron"
#ifdef GP_NINJA_GAMES
 ,"card","fire"
#endif
};

typedef struct {unsigned id;float x,y,w,h;bool enabled;} Button;
typedef enum {PAY_NONE,PAY_UNSENT,PAY_SENT,PAY_CONFIRMED,PAY_LOST} PayState;
struct GamePort {
 const GameController *controller;void *game;
 GameDescriptor descriptor;GameHost host;
 GameBank *banks[GAME_PORT_BANKS];
 GameSessionState session;
 float clock,accumulator,requestAt;bool paused,joinSent,snap,disconnected,rejoining;
 PayState pay;unsigned score,failedBank;
 char status[96];bool retry;
 Button buttons[GAME_BUTTONS],next[GAME_BUTTONS];unsigned buttonCount,nextCount,pressed,clicked;
};

static const GameController *controller(const char *key){for(size_t i=0;i<sizeof CONTROLLERS/sizeof *CONTROLLERS;i++)if(!strcmp(CONTROLLERS[i]->key,key))return CONTROLLERS[i];return NULL;}
bool game_port_supported(const char *key){return key&&controller(key)!=NULL;}
static void set_status(GamePort *p,const char *text,bool retry){snprintf(p->status,sizeof p->status,"%s",text?text:"");p->retry=retry;if(text&&*text&&p->host.notice)p->host.notice(p->host.user,text);}

GamePort *game_port_open(const GameDescriptor *d,const GameHost *host){
 if(!d||!host)return NULL;
 bool known=false;for(size_t i=0;i<sizeof KEYS/sizeof *KEYS;i++)if(!strncmp(KEYS[i],d->key,sizeof d->key))known=true;
 const GameController *c=known?controller(d->key):NULL;if(!c)return NULL;
 if(d->online&&(!host->send||!host->leave))return NULL;
 GamePort *p=calloc(1,sizeof *p);if(!p)return NULL;
 p->controller=c;p->descriptor=*d;p->descriptor.key[sizeof p->descriptor.key-1]=0;p->host=*host;p->session=GAME_SESSION_LOADING;
 p->game=c->create(p);if(!p->game){free(p);return NULL;}
 return p;
}

const GameDescriptor *game_port_descriptor(const GamePort *p){return &p->descriptor;}
const GameHost *game_port_host(const GamePort *p){return &p->host;}
GameBank *game_port_bank(GamePort *p,unsigned i){return i<GAME_PORT_BANKS?p->banks[i]:NULL;}
GameSessionState game_port_session(const GamePort *p){return p->session;}
bool game_port_connected(const GamePort *p){return p&&p->descriptor.online&&!p->disconnected&&!p->rejoining;}
float game_port_clock(const GamePort *p){return p->clock;}
const char *game_port_status(const GamePort *p){return p->status;}
bool game_port_can_retry(const GamePort *p){return p->retry;}
bool game_port_active(const GamePort *p){return p&&p->session!=GAME_SESSION_LEFT;}
bool game_port_paid(const GamePort *p){return p->pay==PAY_CONFIRMED;}
static float rate(const GamePort *p){return p->controller->rate?(float)p->controller->rate:(float)GAME_PORT_RATE;}
float game_port_interpolation(const GamePort *p){if(!p||p->snap)return 1;float a=p->accumulator*rate(p);return a<0?0:a>1?1:a;}
void game_port_snap(GamePort *p){if(p)p->snap=true;}
const GameController *game_port_controller(const GamePort *p){return p->controller;}
void *game_port_game(GamePort *p){return p->game;}
static bool send_packet(GamePort *p,char extension,const char *command,unsigned count,const char *const *args);
void game_port_stage(const GamePort *p,float *width,float *height){*width=p->controller->stageWidth?(float)p->controller->stageWidth:760;*height=p->controller->stageHeight?(float)p->controller->stageHeight:480;}
bool game_port_send(GamePort *p,char extension,const char *command,unsigned count,const char *const *args){return p->descriptor.online&&send_packet(p,extension,command,count,args);}
bool game_port_drawable(const GamePort *p){return p&&p->session!=GAME_SESSION_LOADING&&p->session!=GAME_SESSION_LEFT&&!(p->session==GAME_SESSION_FAILED&&!p->game);}
size_t game_port_texture_bytes(const GamePort *p){size_t n=0;for(unsigned i=0;p&&i<GAME_PORT_BANKS;i++)n+=game_bank_texture_bytes(p->banks[i]);return n;}

static bool send_packet(GamePort *p,char extension,const char *command,unsigned count,const char *const *args){return p->host.send&&p->host.send(p->host.user,extension,command,count,args);}
static void send_score(GamePort *p);
static void join(GamePort *p){
 char room[16];snprintf(room,sizeof room,"%u",p->controller->room);
 p->requestAt=p->clock;p->joinSent=send_packet(p,'s',"j#jr",3,(const char *[]){room,"0","0"});
 if(p->joinSent)set_status(p,"",false);else set_status(p,"Could not reach the server.",true);
}

static GameResource load_bank(GamePort *p,unsigned i){
 if(p->banks[i])return GAME_RESOURCE_READY;
 const char *name=p->controller->banks[i];if(!name)return GAME_RESOURCE_FAILED;
#ifdef GP_NINJA_GAMES
 if(p->controller->bank_resource)name=p->controller->bank_resource(p->game,i);
 if(!name)return GAME_RESOURCE_FAILED;
#endif
 char path[256];GameResource state=p->host.resource?p->host.resource(p->host.user,name,path,sizeof path):GAME_RESOURCE_FAILED;
 if(state!=GAME_RESOURCE_READY){if(state==GAME_RESOURCE_FAILED)set_status(p,"The game resources are not available.",true);return state;}
 size_t used=game_port_texture_bytes(p);const char *error=NULL;
 p->banks[i]=game_bank_load(path,used<GAME_PORT_TEXTURE_LIMIT?GAME_PORT_TEXTURE_LIMIT-used:0,&error);
 if(!p->banks[i]){set_status(p,error?error:"Could not load the game.",true);return GAME_RESOURCE_FAILED;}
 return GAME_RESOURCE_READY;
}
GameResource game_port_require_bank(GamePort *p,unsigned i){
 if(!p||i>=GAME_PORT_BANKS)return GAME_RESOURCE_FAILED;
 GameResource state=load_bank(p,i);if(state==GAME_RESOURCE_READY&&p->retry&&p->failedBank==i+1){set_status(p,"",false);}
 p->failedBank=state==GAME_RESOURCE_FAILED?i+1:0;return state;
}
void game_port_release_bank(GamePort *p,unsigned i){if(p&&i<GAME_PORT_BANKS&&p->banks[i]){game_bank_free(p->banks[i]);p->banks[i]=NULL;}}

static void load_banks(GamePort *p){
 for(unsigned i=0;i<GAME_PORT_BANKS;i++){
  if(!p->controller->banks[i]||(p->controller->lazy&1u<<i))continue;
  GameResource state=load_bank(p,i);
  if(state==GAME_RESOURCE_PENDING)return;
  if(state==GAME_RESOURCE_FAILED){p->session=GAME_SESSION_FAILED;return;}
 }
 if(!p->controller->ready(p->game)){p->session=GAME_SESSION_FAILED;set_status(p,"The game resources are incomplete.",true);return;}
 if(p->descriptor.online&&p->controller->kind==GAME_SOLO){p->session=GAME_SESSION_JOINING;join(p);}else p->session=GAME_SESSION_PLAYING;
}

void game_port_retry(GamePort *p){
 if(!p||!p->retry)return;
 set_status(p,"",false);
 if(p->failedBank){unsigned i=p->failedBank-1;p->failedBank=0;game_port_require_bank(p,i);return;}
 if(p->session==GAME_SESSION_FAILED){for(unsigned i=0;i<GAME_PORT_BANKS;i++)if(!p->banks[i]){p->session=GAME_SESSION_LOADING;break;}if(p->session==GAME_SESSION_FAILED)game_port_leave(p);}
 else if(p->disconnected)set_status(p,"Connection lost. Reconnecting...",false);
 else if(p->session==GAME_SESSION_JOINING||p->rejoining)join(p);
 else if(p->session==GAME_SESSION_PAYING&&p->pay==PAY_UNSENT)send_score(p);
}

static void send_score(GamePort *p){
 char value[16];snprintf(value,sizeof value,"%u",p->score);
 if(send_packet(p,'z',"zo",1,(const char *[]){value})){p->pay=PAY_SENT;p->requestAt=p->clock;set_status(p,"",false);}
 else{p->pay=PAY_UNSENT;set_status(p,"Could not send your score. Try again.",true);}
}
void game_port_finish(GamePort *p,unsigned score){
 if(!p||p->session==GAME_SESSION_LEFT)return;
 if(!p->descriptor.online||p->controller->kind==GAME_TABLE||p->controller->kind==GAME_CARD||(p->session!=GAME_SESSION_PLAYING&&p->session!=GAME_SESSION_PAYING)){game_port_leave(p);return;}
 if(p->pay==PAY_CONFIRMED){game_port_leave(p);return;}
 if(p->pay==PAY_SENT||p->pay==PAY_LOST)return; // never a second zo for one game
 p->session=GAME_SESSION_PAYING;p->score=score;
 // The server only pays a player inside the game room: wait for the reconnection and jg.
 if(p->disconnected||p->rejoining){p->pay=PAY_UNSENT;set_status(p,"Reconnecting before sending your score...",false);return;}
 send_score(p);
}

void game_port_connection(GamePort *p,bool online){
 if(!p||!p->descriptor.online||p->session==GAME_SESSION_LEFT||p->session==GAME_SESSION_LOADING||p->session==GAME_SESSION_FAILED)return;
 // Waddle ends a table or waddle game when its player disconnects.
 if(p->controller->kind!=GAME_SOLO){
  if(!online){p->disconnected=true;set_status(p,"Connection lost. The game has ended.",false);}
  else if(p->disconnected)game_port_leave(p);
  return;
 }
 if(!online){
  if(p->disconnected)return;
  p->disconnected=true;p->rejoining=false;
  // A sent score whose reply is lost may or may not have been paid; it is never resent.
  if(p->pay==PAY_SENT){p->pay=PAY_LOST;set_status(p,"The connection was lost before your coins were confirmed.",false);}
  else set_status(p,"Connection lost. Reconnecting...",false);
  return;
 }
 if(!p->disconnected)return;
 p->disconnected=false;
 if(p->pay==PAY_LOST){set_status(p,"The connection was lost before your coins were confirmed.",false);return;}
 // Reconnection puts the player back in a room: rejoin the game room before any payout.
 if(p->session==GAME_SESSION_JOINING){join(p);return;}
 p->rejoining=true;join(p);
}

void game_port_leave(GamePort *p){
 if(!p||p->session==GAME_SESSION_LEFT)return;
 game_port_stop_sounds(p);p->session=GAME_SESSION_LEFT;
 if(p->host.leave)p->host.leave(p->host.user,p->descriptor.returnRoom,p->descriptor.returnX,p->descriptor.returnY);
}

bool game_port_packet(GamePort *p,const char *command,unsigned count,char *const *args){
 if(!p||!command||p->session==GAME_SESSION_LEFT)return false;
 if(p->controller->kind==GAME_TABLE||p->controller->kind==GAME_CARD)return p->controller->packet?p->controller->packet(p->game,command,count,args):false;
 if(!strcmp(command,"jg")&&(p->session==GAME_SESSION_JOINING||p->rejoining)){
  if(count<1||strtoul(args[0],NULL,10)!=p->controller->room)return false;
  if(p->session==GAME_SESSION_JOINING)p->session=GAME_SESSION_PLAYING;
  p->rejoining=p->joinSent=false;set_status(p,"",false);
  if(p->session==GAME_SESSION_PAYING&&p->pay==PAY_UNSENT)send_score(p);
  return true;
 }
 if(!strcmp(command,"zo")&&p->session==GAME_SESSION_PAYING&&p->pay==PAY_SENT){
  p->pay=PAY_CONFIRMED;
  // Pre-CPIP Waddle replies without a total; the host refreshes coins with the room.
  if(count>=1&&*args[0]){char *end;unsigned long total=strtoul(args[0],&end,10);if(!*end&&p->host.coins)p->host.coins(p->host.user,(unsigned)total);}
  game_port_leave(p);return true;
 }
 return p->controller->packet?p->controller->packet(p->game,command,count,args):false;
}

void game_port_update(GamePort *p,float seconds,const GameInput *input){
 if(!p||p->session==GAME_SESSION_LEFT)return;
 seconds=seconds>0?seconds>.25f?.25f:seconds:0;
 p->clock+=seconds;
 if(p->session==GAME_SESSION_LOADING)load_banks(p);
 if((p->session==GAME_SESSION_JOINING||p->rejoining)&&!p->disconnected&&p->joinSent&&p->clock-p->requestAt>GAME_JOIN_TIMEOUT&&!p->retry)set_status(p,"The server did not answer. Try again.",true);
 if(p->session==GAME_SESSION_PAYING&&p->pay==PAY_SENT&&p->clock-p->requestAt>GAME_PAY_TIMEOUT&&!p->retry)
  set_status(p,"Waiting for the server to confirm your coins.",false);
 static const GameInput none={0};
 if(p->clicked==GAME_BUTTONS+1){p->clicked=0;game_port_retry(p);}
 else if(p->clicked==GAME_BUTTONS+2){p->clicked=0;game_port_leave(p);return;}
 if(p->session!=GAME_SESSION_LOADING&&p->session!=GAME_SESSION_FAILED&&p->controller->frame)p->controller->frame(p->game,seconds,p->paused?&none:input?input:&none);
 p->clicked=0;
 if(p->paused||p->session==GAME_SESSION_LOADING||p->session==GAME_SESSION_FAILED||p->session==GAME_SESSION_LEFT)return;
 p->accumulator+=seconds;
 const float tick=1.0f/rate(p);
 while(p->accumulator>=tick&&p->session!=GAME_SESSION_LEFT){p->accumulator-=tick;p->snap=false;p->controller->tick(p->game);}
}

void game_port_touch(GamePort *p,GameTouch phase,float x,float y){
 if(!p||p->session==GAME_SESSION_LEFT)return;
 unsigned hit=0;for(unsigned i=0;i<p->buttonCount;i++){Button *b=&p->buttons[i];if(b->enabled&&x>=b->x&&y>=b->y&&x<b->x+b->w&&y<b->y+b->h)hit=b->id;}
 if(phase==GAME_TOUCH_DOWN&&hit){p->pressed=hit;return;}
 if(p->pressed){if(phase==GAME_TOUCH_UP){if(hit==p->pressed)p->clicked=hit;p->pressed=0;}return;}
 if(p->session==GAME_SESSION_PLAYING&&!p->paused&&p->controller->touch)p->controller->touch(p->game,phase,x,y);
}

void game_port_pause(GamePort *p,bool paused){if(p)p->paused=paused;}
void game_port_close(GamePort *p){
 if(!p||p->session==GAME_SESSION_LEFT)return;
 if(p->session==GAME_SESSION_LOADING||p->session==GAME_SESSION_FAILED||p->session==GAME_SESSION_JOINING){game_port_leave(p);return;}
 if(p->controller->close)p->controller->close(p->game);else game_port_finish(p,0);
}
void game_port_free(GamePort *p){
 if(!p)return;
 if(p->game)p->controller->destroy(p->game);
 for(unsigned i=0;i<GAME_PORT_BANKS;i++)game_bank_free(p->banks[i]);
 free(p);
}

void game_port_sound(GamePort *p,const char *name,unsigned volume,bool loop){if(p->host.sound)p->host.sound(p->host.user,p->descriptor.key,name,volume,loop);}
void game_port_stop_sounds(GamePort *p){if(p->host.stop_sounds)p->host.stop_sounds(p->host.user,p->descriptor.key);}
void game_port_text(GamePort *p,float x,float y,float size,uint32_t rgb,GameAlign align,const char *text){if(p->host.text)p->host.text(p->host.user,x,y,size,rgb,align,text);}
void game_port_rect(GamePort *p,float x,float y,float w,float h,uint32_t rgb){if(p->host.rect)p->host.rect(p->host.user,x,y,w,h,rgb);}
void game_port_pad(GamePort *p,float x,float y,float w,float h,const char *label,bool pressed){if(p->host.button)p->host.button(p->host.user,x,y,w,h,label,pressed,true);}
bool game_port_button(GamePort *p,unsigned id,float x,float y,float w,float h,const char *label,bool enabled){
 if(p->nextCount<GAME_BUTTONS)p->next[p->nextCount++]=(Button){id,x,y,w,h,enabled};
 if(p->host.button)p->host.button(p->host.user,x,y,w,h,label,p->pressed==id,enabled);
 return enabled;
}
unsigned game_port_clicked(GamePort *p){return p->clicked;}

SWLMatrix game_port_art_view(float x,float y,float w,float h){
 float s=fmaxf(320/w,240/h);return (SWLMatrix){s,0,0,s,(320-w*s)/2-x*s,(240-h*s)/2-y*s};
}
void game_port_title_band(GamePort *p){game_port_rect(p,0,GAME_TITLE_BAND-1,320,1,0x4fc0ff);game_port_rect(p,0,GAME_TITLE_BAND,320,240-GAME_TITLE_BAND,0x062646);}
void game_port_draw_bottom(GamePort *p){
 if(!p||p->session==GAME_SESSION_LEFT)return;
 p->nextCount=0;
 if(p->session==GAME_SESSION_LOADING||p->session==GAME_SESSION_FAILED||(p->session==GAME_SESSION_JOINING&&!p->retry)){
  game_port_rect(p,0,0,320,240,0x003366);
  game_port_text(p,160,96,14,0xffffff,GAME_ALIGN_CENTER,p->session==GAME_SESSION_JOINING?"Joining the game...":p->session==GAME_SESSION_FAILED?p->status:"Loading...");
  if(p->retry)game_port_button(p,GAME_BUTTONS+1,100,140,120,36,"Try again",true);
  game_port_button(p,GAME_BUTTONS+2,100,190,120,30,"Back",true);
 }else{
  if(p->controller->draw_bottom)p->controller->draw_bottom(p->game);
  if(p->status[0]){game_port_rect(p,0,196,320,44,0x003366);game_port_text(p,8,202,11,0xffffff,GAME_ALIGN_LEFT,p->status);if(p->retry)game_port_button(p,GAME_BUTTONS+1,228,204,84,30,"Try again",true);}
 }
 memcpy(p->buttons,p->next,sizeof p->next);p->buttonCount=p->nextCount;
}
