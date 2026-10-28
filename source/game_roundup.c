// Puffle Roundup controller: the original game-roundup board (title, instructions,
// field, results) driven by game_roundup_model at 24 Hz (web: src/games/roundup.ts).
#include "game_controller.h"
#include "game_roundup_model.h"
#include "game_roundup_data.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDUP_ROOM 902
enum {SCREEN_TITLE,SCREEN_INSTRUCTIONS,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_START,BUTTON_EXIT,BUTTON_MORE,BUTTON_FINISH,BUTTON_LEAVE};

typedef struct {int owner;unsigned type,frame;} Slot;
typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 RoundupModel model;bool playing;unsigned screen,total;
 double pointerX,pointerY;bool pointerShown;
 Slot slots[ROUNDUP_HERD];
 float penBox[4];
 uint32_t seed;
} Roundup;

static double random01(void *user){Roundup *r=user;uint32_t x=r->seed;x^=x<<13;x^=x>>17;x^=x<<5;r->seed=x;return (double)x/4294967296.0;}
static void target(Roundup *r,const char *path,unsigned frame){swl_target(r->scene,path,frame,NULL,false);}
static void text(Roundup *r,const char *path,const char *value){game_bank_text(r->bank,path,value);}
static void number(Roundup *r,const char *path,unsigned value){char v[16];snprintf(v,sizeof v,"%u",value);text(r,path,v);}

static void update_score(Roundup *r){
 const RoundupModel *m=&r->model;
 number(r,"board.score.escaped_txt",m->escaped);number(r,"board.score.captured_txt",m->captured);
 target(r,"board.score.clock",(120-m->remaining)/10+1);number(r,"board.score.clock.timer_txt",m->remaining);
}

static int by_y(const void *a,const void *b){const RoundupPet *x=*(const RoundupPet *const *)a,*y=*(const RoundupPet *const *)b;return x->y<y->y?-1:x->y>y->y;}
/** drawPets(): live puffles sorted by y, each showing direction + 8 while it moves. */
static void sync_pets(Roundup *r){
 const RoundupPet *sorted[ROUNDUP_HERD];unsigned n=0;bool snap=false;
 for(unsigned i=0;i<ROUNDUP_HERD;i++)if(!r->model.pets[i].escaped)sorted[n++]=&r->model.pets[i];
 qsort(sorted,n,sizeof *sorted,by_y);
 for(unsigned i=0;i<ROUNDUP_HERD;i++){
  Slot *s=&r->slots[i];char path[32],pet[40];snprintf(path,sizeof path,"board.game.h%u",i);snprintf(pet,sizeof pet,"%s.pet",path);
  if(i>=n){if(s->owner>=0||s->type!=ROUNDUP_PETS){target(r,path,ROUNDUP_PETS+1);*s=(Slot){.owner=-1,.type=ROUNDUP_PETS};}continue;}
  const RoundupPet *p=sorted[i];int owner=(int)(p-r->model.pets);unsigned frame=p->direction+(p->moving?8:0);
  if(s->owner>=0&&s->owner!=owner&&s->type==p->type)snap=true;
  if(s->type!=p->type){target(r,path,p->type+1);s->type=p->type;s->frame=0;}
  if(s->frame!=frame){target(r,pet,frame);s->frame=frame;}
  s->owner=owner;
  swl_transform(r->scene,path,(SWLMatrix){1,0,0,1,(float)p->x,(float)p->y});
 }
 if(snap)game_port_snap(r->port);
}

static void title(Roundup *r,unsigned frame){
 r->screen=frame==1?SCREEN_TITLE:SCREEN_INSTRUCTIONS;r->playing=false;
 target(r,"board",1);target(r,"board.title_mc",frame);
}

static void start_round(Roundup *r){
 unsigned types[ROUNDUP_HERD],map;roundup_model_herd(random01,r,types,&map);
 roundup_model_init(&r->model,types,ROUNDUP_MAP_POSITIONS[map]);
 target(r,"board",2);
 for(unsigned i=0;i<ROUNDUP_HERD;i++)r->slots[i]=(Slot){.owner=-1,.type=ROUNDUP_PETS+1};
 sync_pets(r);update_score(r);game_port_snap(r->port);
 r->screen=SCREEN_GAME;r->playing=true;r->pointerX=r->pointerY=-1000;r->pointerShown=false;
}

static void results(Roundup *r){
 const RoundupModel *m=&r->model;char line[64];
 r->total+=m->coins;r->playing=false;r->screen=SCREEN_RESULTS;target(r,"board",3);
 snprintf(line,sizeof line,"TIMER: %u",m->remaining);text(r,"board.gameOver_mc.secondsRemaining",line);
 snprintf(line,sizeof line,"PUFFLES CAUGHT: %u",m->captured);text(r,"board.gameOver_mc.totalCaught",line);
 snprintf(line,sizeof line,"SCORE THIS ROUND: %u",m->remaining*m->captured);text(r,"board.gameOver_mc.totalScore",line);
 snprintf(line,sizeof line,"COINS THIS ROUND: %u",m->coins);text(r,"board.gameOver_mc.theseCoins",line);
 snprintf(line,sizeof line,"TOTAL COINS: %u",r->total);text(r,"board.gameOver_mc.totalCoins",line);
}

static void *create(GamePort *port){Roundup *r=calloc(1,sizeof *r);if(!r)return NULL;r->port=port;r->seed=0x2545F491u^(uint32_t)(uintptr_t)r;if(!r->seed)r->seed=7;return r;}
static void destroy(void *game){free(game);}

static bool ready(void *game){
 Roundup *r=game;r->bank=game_port_bank(r->port,0);r->scene=game_bank_scene(r->bank);if(!r->scene)return false;
 // The web hides the collision clips and the start-position markers.
 swl_visible(r->scene,"board.game.wall",false);swl_visible(r->scene,"board.game.pen",false);swl_visible(r->scene,"board.game.map",false);
 // Pen bounds for the lower-screen radar, from the original hit mask.
 r->penBox[0]=r->penBox[1]=1e9f;r->penBox[2]=r->penBox[3]=-1e9f;
 for(int y=0;y<400;y+=2)for(int x=0;x<600;x+=2)if(roundup_pen(x,y)){if(x<r->penBox[0])r->penBox[0]=(float)x;if(y<r->penBox[1])r->penBox[1]=(float)y;if(x>r->penBox[2])r->penBox[2]=(float)x;if(y>r->penBox[3])r->penBox[3]=(float)y;}
 title(r,1);return true;
}

#ifdef GP_LAN_ONLY
// LAN test builds only: puffle positions for the emulator autopilot that steers the stylus.
// Scores and results are still read from the screen and the payout from the server.
static void pet_trace(const Roundup *r){
 static unsigned ticks;if(ticks++%2)return;
 FILE *log=fopen("sdmc:/3ds/gopenguin/roundup-pets.log","a");if(!log)return;
 fprintf(log,"tick=%u done=%u",ticks,r->model.done?1u:0u);
 for(unsigned i=0;i<ROUNDUP_HERD;i++){const RoundupPet *pet=&r->model.pets[i];if(!pet->escaped)fprintf(log," %u,%.1f,%.1f,%u",pet->type,pet->x,pet->y,pet->captured?1u:0u);}
 fputc('\n',log);fclose(log);
}
#endif
static void tick(void *game){
 Roundup *r=game;if(!r->scene)return;
 if(!r->playing||game_port_session(r->port)!=GAME_SESSION_PLAYING){swl_tick(r->scene);return;}
 bool captured[ROUNDUP_HERD],escaped[ROUNDUP_HERD];
 for(unsigned i=0;i<ROUNDUP_HERD;i++){captured[i]=r->model.pets[i].captured;escaped[i]=r->model.pets[i].escaped;}
 roundup_model_step(&r->model,r->pointerX,r->pointerY);
 // addSound("capture" + Math.round(Math.random() + 1)) entering the pen; "escape" leaving the field.
 for(unsigned i=0;i<ROUNDUP_HERD;i++){
  if(r->model.pets[i].captured&&!captured[i])game_port_sound(r->port,random01(r)<.5?"capture1":"capture2",100,false);
  if(r->model.pets[i].escaped&&!escaped[i])game_port_sound(r->port,"escape",100,false);
 }
 swl_tick(r->scene);
 sync_pets(r);update_score(r);
#ifdef GP_LAN_ONLY
 pet_trace(r);
#endif
 if(r->model.done)results(r);
}

static void frame(void *game,float seconds,const GameInput *input){
 Roundup *r=game;if(!r->scene)return;
 unsigned clicked=game_port_clicked(r->port);bool a=input->down&GAME_KEY_A,online=game_port_session(r->port)==GAME_SESSION_PLAYING;
 switch(r->screen){
  case SCREEN_TITLE:if(online&&(clicked==BUTTON_PLAY||a))title(r,2);break;
  case SCREEN_INSTRUCTIONS:if(online&&(clicked==BUTTON_START||a))start_round(r);else if(clicked==BUTTON_EXIT)game_port_finish(r->port,r->total*10);break;
  case SCREEN_RESULTS:
   if(game_port_session(r->port)==GAME_SESSION_PAYING){if(clicked==BUTTON_LEAVE)game_port_leave(r->port);else if(clicked==BUTTON_FINISH)game_port_finish(r->port,r->total*10);break;}
   if(clicked==BUTTON_MORE||a)start_round(r);else if(clicked==BUTTON_FINISH)game_port_finish(r->port,r->total*10);break;
  case SCREEN_GAME:{
   if(clicked==BUTTON_FINISH){game_port_finish(r->port,r->total*10);break;}
   float x=input->stickX,y=-input->stickY;
   x=input->held&GAME_KEY_LEFT?-1:input->held&GAME_KEY_RIGHT?1:x;
   y=input->held&GAME_KEY_UP?-1:input->held&GAME_KEY_DOWN?1:y;
   if(fabsf(x)>.15f||fabsf(y)>.15f){
    if(!r->pointerShown){r->pointerX=300;r->pointerY=390;r->pointerShown=true;}
    r->pointerX+=x*260*seconds;r->pointerY+=y*260*seconds;
    r->pointerX=fmax(-20,fmin(620,r->pointerX));r->pointerY=fmax(-20,fmin(420,r->pointerY));
   }
   break;}
 }
}

// The lower-screen radar: the 600x400 field at half scale.
#define RADAR_X 10.0f
#define RADAR_Y 34.0f
static void touch(void *game,GameTouch phase,float x,float y){
 Roundup *r=game;if(r->screen!=SCREEN_GAME||y<RADAR_Y-10)return;(void)phase;
 r->pointerX=(x-RADAR_X)*2;r->pointerY=(y-RADAR_Y)*2;r->pointerShown=true;
}

static void close_game(void *game){Roundup *r=game;game_port_finish(r->port,r->total*10);}

static void draw_bottom(void *game){
 Roundup *r=game;GamePort *p=r->port;char line[96];bool online=game_port_session(p)==GAME_SESSION_PLAYING;
 if(r->screen!=SCREEN_GAME&&r->screen!=SCREEN_TITLE)game_port_rect(p,0,0,320,240,0x2d5a1b);
 switch(r->screen){
  case SCREEN_TITLE:
   game_port_title_band(p);
   game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",online);
   break;
  case SCREEN_INSTRUCTIONS:
   game_port_text(p,160,22,12,0xffffff,GAME_ALIGN_CENTER,"Herd the puffles into the brown pen.");
   game_port_text(p,160,40,12,0xffffff,GAME_ALIGN_CENTER,"Drag the stylus on the field below");
   game_port_text(p,160,58,12,0xffffff,GAME_ALIGN_CENTER,"or move the Circle Pad.");
   game_port_button(p,BUTTON_START,90,92,140,44,"Start",online);
   game_port_button(p,BUTTON_EXIT,110,150,100,30,"Exit",true);
   break;
  case SCREEN_GAME:{
   const RoundupModel *m=&r->model;
   game_port_rect(p,0,0,320,30,0x234311);
   snprintf(line,sizeof line,"Caught %u  Escaped %u  Time %u",m->captured,m->escaped,m->remaining);
   game_port_text(p,8,8,11,0xffffff,GAME_ALIGN_LEFT,line);
   game_port_button(p,BUTTON_FINISH,244,4,72,24,"Finish",true);
   break;}
  case SCREEN_RESULTS:
   snprintf(line,sizeof line,"Caught %u - %u coins this round",r->model.captured,r->model.coins);
   game_port_text(p,160,30,13,0xffffff,GAME_ALIGN_CENTER,line);
   snprintf(line,sizeof line,"Total: %u coins",r->total);game_port_text(p,160,52,14,0xffffff,GAME_ALIGN_CENTER,line);
   if(!game_port_descriptor(p)->online)game_port_text(p,160,72,11,0xd8f0d0,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
   game_port_button(p,BUTTON_MORE,90,96,140,40,"Play more",true);
   {bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
    game_port_button(p,BUTTON_FINISH,90,144,140,34,waiting?"Paying...":"Finish",!waiting);
    if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,184,140,26,"Leave without waiting",true);}
   break;
 }
}

static void overlay(void *game,unsigned bank,float depth,const GameCanvas *canvas){
 Roundup *r=game;if(bank||depth<FLT_MAX||r->screen!=SCREEN_GAME||!r->pointerShown||!canvas->line)return;
 // Herding pointer (the Flash mouse cursor), in stage coordinates.
 float x=(float)r->pointerX+(float)ROUNDUP_GAME_X,y=(float)r->pointerY+(float)ROUNDUP_GAME_Y;
 canvas->line(canvas->user,x-9,y,x+9,y,3,0x000000);canvas->line(canvas->user,x,y-9,x,y+9,3,0x000000);
 canvas->line(canvas->user,x-8,y,x+8,y,1,0xffffff);canvas->line(canvas->user,x,y-8,x,y+8,1,0xffffff);
}

static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
static bool lower_view(void *game,SWLMatrix *view){Roundup *r=game;if(r->screen==SCREEN_TITLE){*view=game_port_art_view(200,50,380,285);return true;}if(r->screen!=SCREEN_GAME)return false;*view=(SWLMatrix){.5f,0,0,.5f,RADAR_X-ROUNDUP_GAME_X*.5f,RADAR_Y-ROUNDUP_GAME_Y*.5f};return true;}

const GameController GAME_ROUNDUP_CONTROLLER={
 .key="roundup",.room=ROUNDUP_ROOM,.banks={"games/roundup.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.overlay=overlay,.lower_view=lower_view,
};
