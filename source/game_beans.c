// Bean Counters controller (classic mode): the original game-beans board and logo
// driven by game_beans_model at 24 Hz (web: src/games/beans.ts).
#include "game_controller.h"
#include "game_beans_model.h"
#include "game_beans_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BEANS_ROOM 901
#define BEANS_SLOTS 16
enum {SCREEN_TITLE,SCREEN_INSTRUCTIONS,SCREEN_GAME};
enum {BUTTON_PLAY=1,BUTTON_HOW,BUTTON_START,BUTTON_EXIT,BUTTON_PAY,BUTTON_LEAVE,BUTTON_QUIT};
static const char *const CHILDREN[BEANS_KINDS]={"bag","bag","bag","bag","item","hazard","hazard","hazard"};

/** A slot shows one flight, then (for bags) its spill. */
typedef struct {unsigned id,kind,frame,bagFrame;bool used,spilling,seen;} Slot;
typedef struct {
 GamePort *port;SWLScene *scene,*logo;GameBank *bank;
 BeansModel model;bool started;unsigned screen;BeansPhase shownPhase;
 double pointer;Slot slots[BEANS_SLOTS];
 unsigned lastImpact,lastNotify;bool truckMoving;
 uint32_t seed;
} Beans;

static double random01(void *user){Beans *b=user;uint32_t x=b->seed;x^=x<<13;x^=x>>17;x^=x<<5;b->seed=x;return (double)x/4294967296.0;}
static void target(Beans *b,const char *path,unsigned frame,bool playing){swl_target(b->scene,path,frame,NULL,playing);}
static void visible(Beans *b,const char *path,bool shown){swl_visible(b->scene,path,shown);}
static void number(Beans *b,const char *path,unsigned value){char v[16];snprintf(v,sizeof v,"%u",value);game_bank_text(b->bank,path,v);}

static void sync_hud(Beans *b){
 const BeansModel *m=&b->model;
 number(b,"board.livesBoard",m->lives>0?(unsigned)m->lives:0);number(b,"board.truckNumber",m->level);number(b,"board.scoreBoard",m->score);
 target(b,"board.stack1",m->placed+1>61?61:m->placed+1,false);
}

static int slot_of(Beans *b,unsigned id){for(unsigned i=0;i<BEANS_SLOTS;i++)if(b->slots[i].used&&b->slots[i].id==id)return (int)i;return -1;}
static int free_slot(Beans *b){for(unsigned i=0;i<BEANS_SLOTS;i++)if(!b->slots[i].used)return (int)i;return -1;}

/** Flights and spills keep their slot (and so their interpolation) for their whole life. */
static void sync_flights(Beans *b){
 const BeansModel *m=&b->model;bool snap=false;
 for(unsigned i=0;i<BEANS_SLOTS;i++)b->slots[i].seen=false;
 for(unsigned n=0;n<m->count+m->spillCount;n++){
  bool spill=n>=m->count;unsigned id=spill?m->spills[n-m->count].id:m->flights[n].id,kind=spill?m->spills[n-m->count].kind:m->flights[n].kind;
  int i=slot_of(b,id);if(i<0){i=free_slot(b);if(i<0)continue;b->slots[i]=(Slot){.id=id,.kind=kind,.used=true};snap=true;}
  Slot *s=&b->slots[i];s->seen=true;
  char path[24],holder[32],child[48],bag[56];snprintf(path,sizeof path,"board.s%d",i);snprintf(holder,sizeof holder,"%s.h",path);
  snprintf(child,sizeof child,"%s.%s",holder,CHILDREN[kind]);snprintf(bag,sizeof bag,"%s.bag_mc",child);
  if(!s->frame){target(b,path,kind+1,false);target(b,holder,2,false);if(kind>=5){char collider[64];snprintf(collider,sizeof collider,"%s.collider",child);visible(b,collider,false);}}
  if(spill){
   // The final flight frame stays while the bag spills (bag_mc plays from 1).
   if(!s->spilling){s->spilling=true;target(b,child,BEANS_KIND_DATA[kind].length,false);target(b,bag,1,true);}
  }else{
   unsigned age=m->flights[n].age;if(age!=s->frame){target(b,child,age,false);s->frame=age;}
   if(kind<4){unsigned f=(age-2)%50+1;if(f!=s->bagFrame){target(b,bag,f,false);s->bagFrame=f;}}
  }
  if(!s->frame)s->frame=1;
 }
 for(unsigned i=0;i<BEANS_SLOTS;i++)if(b->slots[i].used&&!b->slots[i].seen){char path[24];snprintf(path,sizeof path,"board.s%u",i);target(b,path,BEANS_KINDS+1,false);b->slots[i]=(Slot){0};}
 if(snap)game_port_snap(b->port);
}

static void sync_phase(Beans *b){
 const BeansModel *m=&b->model;
 if(m->phase==BEANS_RETRY)target(b,"board.tryAgain",m->phaseTick+2>92?92:m->phaseTick+2,false);
 if(m->phase==BEANS_TRUCK)target(b,"board.levelUp",m->phaseTick+2>88?88:m->phaseTick+2,false);
 if(b->shownPhase==m->phase)return;
 b->shownPhase=m->phase;
 visible(b,"board.tryAgain",m->phase==BEANS_RETRY);visible(b,"board.levelUp",m->phase==BEANS_TRUCK);
 visible(b,"board.gameOver",m->phase==BEANS_LOST);visible(b,"board.youWin",m->phase==BEANS_WON);
 if(m->phase==BEANS_WON)target(b,"board.youWin",2,false);
 // The truck drives away and returns (its own 97-frame animation).
 if(m->phase==BEANS_TRUCK){target(b,"board.truck",2,true);b->truckMoving=true;}
 if(m->phase==BEANS_WON||m->phase==BEANS_LOST)game_port_stop_sounds(b->port);
}

static void sync_penguin(Beans *b){
 const BeansModel *m=&b->model;
 target(b,"board.peng",m->pose,false);
 swl_transform(b->scene,"board.peng",(SWLMatrix){1,0,0,1,(float)m->x,345.65f});
 if(m->impact!=b->lastImpact){visible(b,"board.crash_mc",m->impact>0);if(m->impact){target(b,"board.crash_mc",m->impact,false);swl_transform(b->scene,"board.crash_mc",(SWLMatrix){1,0,0,1,(float)m->impactX,352.45f});}b->lastImpact=m->impact;}
 if(m->notify!=b->lastNotify){visible(b,"board.oneUpNotify",m->notify>0);if(m->notify){target(b,"board.oneUpNotify",m->notify,false);swl_transform(b->scene,"board.oneUpNotify",(SWLMatrix){1,0,0,1,(float)m->notifyX,335.7f});}b->lastNotify=m->notify;}
}

static void play_sounds(Beans *b){for(unsigned i=0;i<b->model.soundCount;i++)game_port_sound(b->port,b->model.sounds[i].name,b->model.sounds[i].volume,false);b->model.soundCount=0;}

static void start(Beans *b){
 if(b->started)return;
 b->started=true;b->screen=SCREEN_GAME;
 beans_model_init(&b->model,random01,b);b->pointer=b->model.x;b->shownPhase=(BeansPhase)-1;
 target(b,"board",2,false);
 // PenguinClip.init: the shield body stays hidden in classic mode.
 visible(b,"board.peng.bodyShield",false);
 visible(b,"board.crash_mc",false);visible(b,"board.oneUpNotify",false);
 // Slot frame 1 is the first bag thrower: unused slots show the empty last frame.
 for(unsigned i=0;i<BEANS_SLOTS;i++){char path[24];snprintf(path,sizeof path,"board.s%u",i);target(b,path,BEANS_KINDS+1,false);b->slots[i]=(Slot){0};}
 sync_hud(b);sync_phase(b);sync_penguin(b);
}

static void *create(GamePort *port){Beans *b=calloc(1,sizeof *b);if(!b)return NULL;b->port=port;b->seed=0x6C8E9CF5u^(uint32_t)(uintptr_t)b;if(!b->seed)b->seed=11;return b;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Beans *b=game;b->bank=game_port_bank(b->port,0);b->scene=game_bank_scene(b->bank);b->logo=game_bank_scene(game_port_bank(b->port,1));
 if(!b->scene||!b->logo)return false;
 target(b,"board",1,false);visible(b,"board.title_adv_mc",false);
 // playbackgroundAudio(): "backgroundLoop" at volume 45.
 game_port_sound(b->port,"backgroundLoop",45,true);
 return true;
}

static void tick(void *game){
 Beans *b=game;if(!b->scene)return;
 if(b->screen!=SCREEN_GAME||game_port_session(b->port)!=GAME_SESSION_PLAYING){swl_tick(b->scene);return;}
 BeansModel *m=&b->model;
 if(m->phase!=BEANS_WON&&m->phase!=BEANS_LOST)beans_model_tick(m,b->pointer);
 swl_tick(b->scene);
 if(b->truckMoving&&swl_frame(b->scene,"board.truck")==1)b->truckMoving=false;
 play_sounds(b);sync_flights(b);sync_penguin(b);sync_hud(b);sync_phase(b);
}

static void drop(Beans *b){
 // Web pointer(): a click moves to the pointer and drops when near the truck.
 BeansModel *m=&b->model;m->oldX=m->x;m->x=b->pointer<190?190:b->pointer>555?555:b->pointer;beans_model_drop(m);play_sounds(b);sync_hud(b);sync_phase(b);
}

static void frame(void *game,float seconds,const GameInput *input){
 Beans *b=game;if(!b->scene)return;
 unsigned clicked=game_port_clicked(b->port);bool a=input->down&GAME_KEY_A,online=game_port_session(b->port)==GAME_SESSION_PLAYING;
 if(b->screen==SCREEN_TITLE){if(online&&(clicked==BUTTON_PLAY||a))start(b);else if(clicked==BUTTON_HOW){b->screen=SCREEN_INSTRUCTIONS;target(b,"board.title_norm_mc",2,false);}else if(clicked==BUTTON_EXIT)game_port_finish(b->port,0);return;}
 if(b->screen==SCREEN_INSTRUCTIONS){if(online&&(clicked==BUTTON_START||a))start(b);else if(clicked==BUTTON_EXIT)game_port_finish(b->port,0);return;}
 BeansModel *m=&b->model;
 if(m->phase==BEANS_WON||m->phase==BEANS_LOST){
  if(game_port_session(b->port)==GAME_SESSION_PAYING&&clicked==BUTTON_LEAVE)game_port_leave(b->port);
  else if(clicked==BUTTON_PAY||a)game_port_finish(b->port,beans_model_payout(m));
  return;
 }
 if(clicked==BUTTON_QUIT){game_port_finish(b->port,beans_model_payout(m));return;}
 float x=input->stickX;x=input->held&GAME_KEY_LEFT?-1:input->held&GAME_KEY_RIGHT?1:x;
 if(fabsf(x)>.15f){b->pointer+=x*420*seconds;b->pointer=b->pointer<150?150:b->pointer>600?600:b->pointer;}
 if(input->down&(GAME_KEY_A|GAME_KEY_B|GAME_KEY_Y|GAME_KEY_DOWN))drop(b);
}

// Lower screen strip: the penguin's 190..555 walkway.
static void touch(void *game,GameTouch phase,float x,float y){
 Beans *b=game;if(b->screen!=SCREEN_GAME||y<40)return;
 b->pointer=190+(double)x*(555.0-190.0)/320.0;
 if(phase==GAME_TOUCH_UP&&y>170)drop(b);
}

static void close_game(void *game){Beans *b=game;game_port_finish(b->port,b->screen==SCREEN_GAME?beans_model_payout(&b->model):0);}

static void draw_bottom(void *game){
 Beans *b=game;GamePort *p=b->port;char line[96];bool online=game_port_session(p)==GAME_SESSION_PLAYING;
 bool active=b->screen==SCREEN_GAME&&b->model.phase!=BEANS_WON&&b->model.phase!=BEANS_LOST;
 if(b->screen!=SCREEN_TITLE)game_port_rect(p,0,0,320,active?34:240,0x4a2e12);
 if(b->screen==SCREEN_TITLE){
  game_port_title_band(p);
  game_port_button(p,BUTTON_PLAY,6,197,112,36,"Play",online);
  game_port_button(p,BUTTON_HOW,124,197,112,36,"Instructions",true);
  game_port_button(p,BUTTON_EXIT,242,197,72,36,"Exit",true);
  return;
 }
 if(b->screen==SCREEN_INSTRUCTIONS){
  game_port_text(p,160,24,12,0xffffff,GAME_ALIGN_CENTER,"Catch the bags. Drop them on the truck");
  game_port_text(p,160,42,12,0xffffff,GAME_ALIGN_CENTER,"side with A or a tap on the strip below.");
  game_port_button(p,BUTTON_START,90,96,140,42,"Start",online);game_port_button(p,BUTTON_EXIT,110,160,100,28,"Exit",true);
  return;
 }
 const BeansModel *m=&b->model;
 snprintf(line,sizeof line,"Score %u  Truck %u  Lives %d",m->score,m->level,m->lives);game_port_text(p,8,10,11,0xffffff,GAME_ALIGN_LEFT,line);
 if(m->phase==BEANS_WON||m->phase==BEANS_LOST){
  snprintf(line,sizeof line,m->phase==BEANS_WON?"All trucks unloaded! %u coins":"Game over. %u coins",beans_model_coins(m));
  game_port_text(p,160,60,14,0xffffff,GAME_ALIGN_CENTER,line);
  if(!game_port_descriptor(p)->online)game_port_text(p,160,82,11,0xf0e0c0,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
  bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
  game_port_button(p,BUTTON_PAY,90,104,140,42,waiting?"Paying...":game_port_descriptor(p)->online?"Get paid":"Exit",!waiting);
  if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,156,140,30,"Leave without waiting",true);
  return;
 }
 game_port_button(p,BUTTON_QUIT,244,4,72,26,"Quit",true);
 game_port_rect(p,0,215,320,25,0x4a2e12);game_port_text(p,160,222,11,0xffffff,GAME_ALIGN_CENTER,m->held?"Release below / A: drop the bags":"Drag to move and catch the bags");
}

static bool bank_view(void *game,unsigned bank,SWLMatrix *local){
 Beans *b=game;*local=(SWLMatrix){1,0,0,1,0,0};if(!bank)return true;
 if(b->screen!=SCREEN_TITLE)return false;
 SWLInstance logo;if(!swl_instance(b->scene,"board.title_mc.logo_mc",&logo))return false;
 *local=logo.matrix;return true;
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
static bool lower_view(void *game,SWLMatrix *view){Beans *b=game;if(b->screen==SCREEN_TITLE){*view=game_port_art_view(0,90,440,330);return true;}if(b->screen!=SCREEN_GAME||b->model.phase==BEANS_WON||b->model.phase==BEANS_LOST)return false;float scale=320.0f/(555-190);*view=(SWLMatrix){scale,0,0,scale,-190*scale,34-250*scale};return true;}

const GameController GAME_BEANS_CONTROLLER={
 .key="beans",.room=BEANS_ROOM,.banks={"games/beans.gpk","games/beans-title.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.lower_view=lower_view,
};
