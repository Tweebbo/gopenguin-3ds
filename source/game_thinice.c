// Thin Ice controller: the original game-thinice movie (title pages, board, HUD,
// ending) driven by game_thinice_model at the movie's 23 Hz (web: src/games/thinice.ts).
#include "game_controller.h"
#include "game_thinice_model.h"
#include "game_thinice_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THINICE_ROOM 909
enum {SCREEN_TITLE,SCREEN_GAME};
enum {BUTTON_START=1,BUTTON_PLAY,BUTTON_NEXT,BUTTON_BACK,BUTTON_EXIT,BUTTON_RESET,BUTTON_PAY,BUTTON_LEAVE};
// ThinIce.playSounds(): the script's attachSound linkage per event.
static const char *const SOUNDS[]={"29","9","19","23","12","25"};
static const IceDirection PADS[4]={ICE_LEFT,ICE_UP,ICE_DOWN,ICE_RIGHT};

typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 ThinIceModel model;bool started;unsigned screen,titlePage;IcePhase pose;bool posed;
 uint8_t shown[THINICE_H][THINICE_W];bool boardShown;
 int touchPad,touchX,touchY;bool touchBoard;uint32_t seed;
} ThinIce;

static double random01(void *user){ThinIce *t=user;uint32_t x=t->seed;x^=x<<13;x^=x>>17;x^=x<<5;t->seed=x;return (double)x/4294967296.0;}
static void target(ThinIce *t,const char *path,unsigned frame,const char *label){swl_target(t->scene,path,frame,label,false);}
static void place(ThinIce *t,const char *path,double x,double y){swl_transform(t->scene,path,(SWLMatrix){1,0,0,1,(float)(156+x*24),(float)(60+y*24)});}

static void sync_text(ThinIce *t){
 const ThinIceModel *m=&t->model;unsigned frame=m->phase==ICE_WON?3:2;char v[16];
 for(unsigned i=0;i<THINICE_FIELDS;i++){
  if(THINICE_FIELD_DATA[i].frame!=frame)continue;
  const char *n=THINICE_FIELD_DATA[i].variable;
  unsigned value=!strcmp(n,"goal")?thinice_model_goal(m):!strcmp(n,"stageMap")?m->level:!strcmp(n,"tilesFlipped")?m->tiles:!strcmp(n,"saveTilesFlipped")?m->savedTiles:
   !strcmp(n,"timepoints")?m->timePoints:!strcmp(n,"coins")?thinice_model_coins(m):thinice_model_score(m);
  snprintf(v,sizeof v,"%u",value);game_bank_text(t->bank,THINICE_FIELD_DATA[i].path,v);
 }
 snprintf(v,sizeof v,"%u",m->solved);game_bank_text(t->bank,"game.solvedStages_tf",v);
 snprintf(v,sizeof v,"%u",m->firstTries);game_bank_text(t->bank,"game.firstTries_tf",v);
 snprintf(v,sizeof v,"%u",m->savedBags);game_bank_text(t->bank,"game.saveBonusPoints_tf",v);
}

static void sync(ThinIce *t,float alpha){
 const ThinIceModel *m=&t->model;
 target(t,"game",m->phase==ICE_WON?3:2,NULL);
 if(!t->posed||t->pose!=m->phase){
  t->posed=true;t->pose=m->phase;
  target(t,"game.icePlayer",1,m->phase==ICE_SINKING?"sink":m->phase==ICE_ENTERING?"ignite":NULL);
  if(m->phase==ICE_WON){
   target(t,"game.ending",0,m->solved<4?"end1":m->solved<18?"end2":"end3");
   swl_visible(t->scene,"game.memberExtras",false);game_port_stop_sounds(t->port);
  }
 }
 sync_text(t);
 if(m->phase==ICE_WON)return;
 // Only changed cells get a new frame (the board keeps one override per cell).
 for(unsigned y=0;y<THINICE_H;y++)for(unsigned x=0;x<THINICE_W;x++)if(!t->boardShown||t->shown[y][x]!=m->board[y][x]){
  char path[32];snprintf(path,sizeof path,"game.board.t%u_%u",y,x);target(t,path,m->board[y][x],NULL);t->shown[y][x]=m->board[y][x];
 }
 t->boardShown=true;
 for(unsigned i=0;i<2;i++){
  char path[24];snprintf(path,sizeof path,"game.warp%u",i);
  if(i>=m->warpCount){place(t,path,-10,-10);continue;}
  IcePoint p=m->warps[i];int d=abs(p.x-m->position.x)+abs(p.y-m->position.y);
  target(t,path,0,m->warped?"gone":d==1?"charge":"normal");place(t,path,p.x,p.y);
 }
 // The block and the player move between cells with the model's own easing; the
 // controller sets the eased position each tick and the scene interpolates the rest.
 double x,y;(void)alpha;
 if(thinice_model_render_block(m,0,&x,&y))place(t,"game.iceBlock",x,y);else place(t,"game.iceBlock",-10,-10);
 thinice_model_render_position(m,0,&x,&y);place(t,"game.icePlayer",x,y);
}

static void play_sounds(ThinIce *t){for(unsigned i=0;i<t->model.soundCount;i++)game_port_sound(t->port,SOUNDS[t->model.sounds[i]],100,false);t->model.soundCount=0;}

static void title(ThinIce *t,unsigned page){t->titlePage=page;target(t,"game.title",page,NULL);}
static void start(ThinIce *t){
 if(t->started)return;
 t->started=true;t->screen=SCREEN_GAME;
 thinice_model_init(&t->model,random01,t);t->boardShown=t->posed=false;
 game_port_release_bank(t->port,1);
 sync(t,0);game_port_snap(t->port);
}

static void *create(GamePort *port){ThinIce *t=calloc(1,sizeof *t);if(!t)return NULL;t->port=port;t->touchPad=-1;t->seed=0x85EBCA6Bu^(uint32_t)(uintptr_t)t;if(!t->seed)t->seed=3;return t;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 ThinIce *t=game;t->bank=game_port_bank(t->port,0);t->scene=game_bank_scene(t->bank);if(!t->scene||!game_port_bank(t->port,1))return false;
 target(t,"game",1,NULL);title(t,1);return true;
}

static void tick(void *game){
 ThinIce *t=game;if(!t->scene)return;
 swl_tick(t->scene);
 if(t->screen!=SCREEN_GAME||game_port_session(t->port)!=GAME_SESSION_PLAYING||t->model.phase==ICE_WON)return;
 thinice_model_step(&t->model);play_sounds(t);sync(t,0);
}

static void frame(void *game,float seconds,const GameInput *input){
 ThinIce *t=game;(void)seconds;if(!t->scene)return;
 unsigned clicked=game_port_clicked(t->port);bool online=game_port_session(t->port)==GAME_SESSION_PLAYING;
 if(t->screen==SCREEN_TITLE){
  if(clicked==BUTTON_EXIT){game_port_finish(t->port,0);return;}
  if(t->titlePage==1&&(clicked==BUTTON_START||(input->down&GAME_KEY_A)))title(t,2);
  else if(online&&(clicked==BUTTON_PLAY||(t->titlePage>1&&(input->down&GAME_KEY_A))))start(t);
  else if(clicked==BUTTON_NEXT&&t->titlePage<5)title(t,t->titlePage+1);
  else if(clicked==BUTTON_BACK&&t->titlePage>2)title(t,t->titlePage-1);
  return;
 }
 ThinIceModel *m=&t->model;
 if(m->phase==ICE_WON){
  if(game_port_session(t->port)==GAME_SESSION_PAYING&&clicked==BUTTON_LEAVE)game_port_leave(t->port);
  else if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(t->port,thinice_model_score(m));
  return;
 }
 if(clicked==BUTTON_EXIT){game_port_finish(t->port,thinice_model_score(m));return;}
 if(clicked==BUTTON_RESET||(input->down&GAME_KEY_Y)){thinice_model_reset(m);sync(t,0);}
 bool keys[4]={(input->held&GAME_KEY_LEFT)||input->stickX<-.5f,(input->held&GAME_KEY_UP)||input->stickY>.5f,(input->held&GAME_KEY_DOWN)||input->stickY<-.5f,(input->held&GAME_KEY_RIGHT)||input->stickX>.5f};
 if(t->touchBoard){int dx=t->touchX-m->position.x,dy=t->touchY-m->position.y;t->touchPad=!dx&&!dy?-1:abs(dx)>abs(dy)?(dx<0?0:3):(dy<0?1:2);}
 for(unsigned i=0;i<4;i++){if(keys[i]||t->touchPad==(int)i)thinice_model_press(m,PADS[i]);else thinice_model_release(m,PADS[i]);}
}

static void touch(void *game,GameTouch phase,float x,float y){ThinIce *t=game;if(t->screen!=SCREEN_GAME)return;t->touchBoard=phase!=GAME_TOUCH_UP&&x>=40&&x<268&&y>=29&&y<209;if(t->touchBoard){t->touchX=(int)((x-40)/12);t->touchY=(int)((y-29)/12);}else t->touchPad=-1;}
static void close_game(void *game){ThinIce *t=game;game_port_finish(t->port,t->started?thinice_model_score(&t->model):0);}

static void draw_bottom(void *game){
 ThinIce *t=game;GamePort *p=t->port;char line[96];bool online=game_port_session(p)==GAME_SESSION_PLAYING;
 bool active=t->screen==SCREEN_GAME&&t->model.phase!=ICE_WON;
 if(!(t->screen==SCREEN_TITLE&&t->titlePage==1))game_port_rect(p,0,0,320,active?28:240,0x003366);
 if(t->screen==SCREEN_TITLE){
  if(t->titlePage==1){
   game_port_title_band(p);
   game_port_button(p,BUTTON_START,40,197,140,36,"Start",true);game_port_button(p,BUTTON_EXIT,196,197,84,36,"Exit",true);
   return;
  }
  game_port_text(p,160,30,16,0xffffff,GAME_ALIGN_CENTER,"THIN ICE");
  {
   game_port_button(p,BUTTON_PLAY,90,70,140,42,"Play",online);
   game_port_button(p,BUTTON_BACK,20,130,120,32,"Back",t->titlePage>2);
   game_port_button(p,BUTTON_NEXT,180,130,120,32,"Next",t->titlePage<5);
  }
  game_port_button(p,BUTTON_EXIT,110,190,100,28,"Exit",true);
  return;
 }
 const ThinIceModel *m=&t->model;
 snprintf(line,sizeof line,"Stage %u  Melted %u/%u  Points %u",m->level,m->tiles,thinice_model_goal(m),thinice_model_score(m));
 game_port_text(p,8,10,11,0xffffff,GAME_ALIGN_LEFT,line);
 if(m->phase==ICE_WON){
  snprintf(line,sizeof line,"All stages done! You earned %u coins.",thinice_model_coins(m));game_port_text(p,160,60,13,0xffffff,GAME_ALIGN_CENTER,line);
  if(!game_port_descriptor(p)->online)game_port_text(p,160,82,11,0xd0f0ff,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
  bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
  game_port_button(p,BUTTON_PAY,90,104,140,42,waiting?"Paying...":game_port_descriptor(p)->online?"Get coins":"Exit",!waiting);
  if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,156,140,30,"Leave without waiting",true);
  return;
 }
 game_port_rect(p,0,212,320,28,0x003366);game_port_button(p,BUTTON_RESET,4,215,94,22,"Reset (Y)",true);game_port_button(p,BUTTON_EXIT,246,215,70,22,"Exit",true);game_port_text(p,171,222,10,0xffffff,GAME_ALIGN_CENTER,"Drag / Circle Pad");
}

static bool bank_view(void *game,unsigned bank,SWLMatrix *local){
 ThinIce *t=game;*local=(SWLMatrix){1,0,0,1,0,0};if(!bank)return true;
 if(t->screen!=SCREEN_TITLE)return false;
 SWLInstance holder;if(!swl_instance(t->scene,"game.title.localeTitle",&holder))return false;
 *local=holder.matrix;return true;
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
static bool lower_view(void *game,SWLMatrix *view){ThinIce *t=game;if(t->screen==SCREEN_TITLE&&t->titlePage==1){*view=game_port_art_view(180,30,400,300);return true;}if(t->screen!=SCREEN_GAME||t->model.phase==ICE_WON)return false;*view=(SWLMatrix){.5f,0,0,.5f,-32,5};return true;}

const GameController GAME_THINICE_CONTROLLER={
 .key="thinice",.room=THINICE_ROOM,.rate=23,.banks={"games/thinice.gpk","games/thinice-title.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.lower_view=lower_view,
};
