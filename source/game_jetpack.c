#include "game_controller.h"
#include "game_jetpack_model.h"
#include "game_jetpack.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JETPACK_ROOM 906
#define TILE_SLOTS 96
enum {SCREEN_TITLE,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_THRUST,BUTTON_NEXT,BUTTON_RETRY,BUTTON_PAY,BUTTON_LEAVE,BUTTON_EXIT};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 JetpackModel model;
 bool started,thrust,left,right,down,sceneReady;
 unsigned screen,loadedLevel;
 double camX,camY;
 int pad;
 char resource[40];
} Jetpack;

JetpackModel *game_jetpack_model(void *game){return &((Jetpack *)game)->model;}

static void target(Jetpack *j,const char *path,unsigned frame,double x,double y){
 swl_target(j->scene,path,frame,NULL,false);
 swl_transform(j->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});
 swl_visible(j->scene,path,true);
}
static void hide(Jetpack *j,const char *path){swl_visible(j->scene,path,false);}

static void sync_tiles(Jetpack *j){
 JetpackModel *m=&j->model;const JetpackLevelData *l=jetpack_model_level(m);
 double width=l->width*80.0,height=l->height*80.0;
 j->camX=m->x-380;if(j->camX<0)j->camX=0;if(j->camX>width-760)j->camX=width-760;
 j->camY=m->y-240;if(j->camY<0)j->camY=0;if(j->camY>height-480)j->camY=height-480;
 if(j->camX<0)j->camX=0;
 if(j->camY<0)j->camY=0;
 unsigned firstX=(unsigned)(j->camX/80),lastX=(unsigned)((j->camX+760)/80);
 unsigned firstY=(unsigned)(j->camY/80),lastY=(unsigned)((j->camY+480)/80);
 unsigned slot=0;
 for(unsigned y=firstY;y<=lastY&&y<l->height;y++)for(unsigned x=firstX;x<=lastX&&x<l->width;x++){
  unsigned tile=m->map[y*m->width+x];if(tile==1)continue;
  if(slot>=TILE_SLOTS)break;
  char path[24];snprintf(path,sizeof path,"root.tile%u",slot++);
  target(j,path,tile,(double)x*80-j->camX,(double)y*80-j->camY);
 }
 for(unsigned i=slot;i<TILE_SLOTS;i++){char path[24];snprintf(path,sizeof path,"root.tile%u",i);hide(j,path);}
 char player[16];snprintf(player,sizeof player,"root.player");
 double angle=m->rotation*3.141592653589793/180.0;float c=(float)cos(angle),s=(float)sin(angle);
 swl_transform(j->scene,player,(SWLMatrix){c,s,-s,c,(float)(m->x-j->camX),(float)(m->y-j->camY)});
 swl_visible(j->scene,player,true);
}

static void hide_scene(Jetpack *j){
 for(unsigned i=0;i<TILE_SLOTS;i++){char path[24];snprintf(path,sizeof path,"root.tile%u",i);hide(j,path);}
 hide(j,"root.player");
}
static void attach_scene(Jetpack *j){
 j->bank=game_port_bank(j->port,0);j->scene=game_bank_scene(j->bank);j->sceneReady=j->scene!=NULL;
 if(j->sceneReady){hide_scene(j);game_port_snap(j->port);}
}
static void request_level(Jetpack *j,unsigned level){
 if(j->sceneReady&&j->loadedLevel==level)return;
 if(j->bank)game_port_release_bank(j->port,0);
 j->bank=NULL;j->scene=NULL;j->sceneReady=false;j->loadedLevel=level;
 if(level==0)snprintf(j->resource,sizeof j->resource,"games/jetpack.gpk");
 else snprintf(j->resource,sizeof j->resource,"games/jetpack%u.gpk",level+1);
 GameResource state=game_port_require_bank(j->port,0);
 if(state==GAME_RESOURCE_READY)attach_scene(j);
}
static void load_level(Jetpack *j){request_level(j,j->model.level);if(j->sceneReady){sync_tiles(j);game_port_snap(j->port);}}
static void start(Jetpack *j){j->started=true;j->screen=SCREEN_GAME;jetpack_model_start(&j->model);load_level(j);}

static void *create(GamePort *port){Jetpack *j=calloc(1,sizeof *j);if(!j)return NULL;j->port=port;jetpack_model_init(&j->model);return j;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Jetpack *j=game;j->screen=SCREEN_TITLE;request_level(j,0);return true;
}

static void tick(void *game){
 Jetpack *j=game;
 if(!j->sceneReady){GameResource state=game_port_require_bank(j->port,0);if(state==GAME_RESOURCE_READY)attach_scene(j);else return;}
 swl_tick_deferred(j->scene);
 if(j->screen!=SCREEN_GAME||game_port_session(j->port)!=GAME_SESSION_PLAYING)return;
 jetpack_model_step(&j->model,j->thrust,j->left,j->right,j->down);
 sync_tiles(j);
 if(j->model.phase==JETPACK_DONE||j->model.phase==JETPACK_OVER||j->model.phase==JETPACK_WON)j->screen=SCREEN_RESULTS;
}

static void frame(void *game,float seconds,const GameInput *input){
 Jetpack *j=game;(void)seconds;
 unsigned clicked=game_port_clicked(j->port);
 if(j->screen==SCREEN_TITLE){
  if(clicked==BUTTON_EXIT){game_port_leave(j->port);return;}
  if((clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))&&game_port_session(j->port)==GAME_SESSION_PLAYING)start(j);
  return;
 }
 if(j->screen==SCREEN_RESULTS){
  JetpackModel *m=&j->model;
  if(m->phase==JETPACK_DONE){if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_A)){jetpack_model_next(m);if(m->phase!=JETPACK_WON){j->screen=SCREEN_GAME;request_level(j,m->level);}}}
  else if(m->phase==JETPACK_OVER){if(clicked==BUTTON_RETRY||(input->down&GAME_KEY_A)){j->screen=SCREEN_GAME;m->phase=JETPACK_PLAYING;jetpack_model_reset(m);request_level(j,m->level);}}
  else{if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(j->port,jetpack_model_total(m));else if(clicked==BUTTON_LEAVE)game_port_leave(j->port);}
  return;
 }
 if(clicked==BUTTON_EXIT){game_port_finish(j->port,jetpack_model_total(&j->model));return;}
 j->thrust=(input->held&(GAME_KEY_A|GAME_KEY_B))!=0;
 j->left=(input->held&GAME_KEY_LEFT)||input->stickX<-.5f;
 j->right=(input->held&GAME_KEY_RIGHT)||input->stickX>.5f;
 j->down=(input->held&GAME_KEY_DOWN)||input->stickY<-.5f;
 if(j->pad<0)j->left=true;
 if(j->pad>0)j->right=true;
}

static void touch(void *game,GameTouch phase,float x,float y){
 Jetpack *j=game;if(j->screen!=SCREEN_GAME||y<190)return;
 if(phase==GAME_TOUCH_UP){j->pad=0;return;}
 j->pad=x<70?-1:x>250?1:0;
 if(x>=120&&x<=200)j->thrust=phase!=GAME_TOUCH_UP;
}

static void close_game(void *game){Jetpack *j=game;if(!j->started){game_port_leave(j->port);return;}game_port_finish(j->port,jetpack_model_total(&j->model));}

static void draw_bottom(void *game){
 Jetpack *j=game;GamePort *p=j->port;char line[96];
 if(j->screen==SCREEN_TITLE){
  game_port_title_band(p);
  game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);
  game_port_button(p,BUTTON_EXIT,196,197,84,36,"Exit",true);return;
 }
 JetpackModel *m=&j->model;
 if(j->screen==SCREEN_RESULTS){
  snprintf(line,sizeof line,"Score %u",jetpack_model_total(m));game_port_text(p,160,40,16,0xffffff,GAME_ALIGN_CENTER,line);
  if(m->phase==JETPACK_DONE){game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Level complete!");game_port_button(p,BUTTON_NEXT,90,110,140,42,"Next level",true);}
  else if(m->phase==JETPACK_OVER){game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Game over.");game_port_button(p,BUTTON_RETRY,90,110,140,42,"Retry",true);}
  else{bool online=game_port_descriptor(p)->online;snprintf(line,sizeof line,online?"You earned %u coins.":"Offline preview: no coins.",jetpack_model_total(m));game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,line);
   game_port_button(p,BUTTON_PAY,90,110,140,44,online?"Get coins":"Exit",true);}
  return;
 }
 game_port_rect(p,0,0,320,30,0x003366);
 snprintf(line,sizeof line,"Score %u  Level %u  Lives %u  Fuel %u",jetpack_model_total(m),m->level+1,m->lives>0?m->lives:0,(unsigned)(m->fuel>0?m->fuel:0));
 game_port_text(p,7,9,10,0xffffff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,264,3,52,22,"Exit",true);
 game_port_rect(p,0,190,320,50,0x003366);
 game_port_pad(p,4,196,58,40,"<",j->pad<0||j->left);
 game_port_button(p,BUTTON_THRUST,131,196,58,40,"Thrust",j->thrust);
 game_port_pad(p,258,196,58,40,">",j->pad>0||j->right);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Jetpack *j=game;return bank==0?j->resource:NULL;}
#endif
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Jetpack *j=game;return bank==0&&j->sceneReady;}

const GameController GAME_JETPACK_CONTROLLER={
 .key="jetpack",.kind=GAME_SOLO,.room=JETPACK_ROOM,.rate=24,
 .banks={"games/jetpack.gpk"},.lazy=1,.order={0,1,2},.backgroundRGB=0x016691,
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,
};
