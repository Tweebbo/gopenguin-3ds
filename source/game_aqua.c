#include "game_controller.h"
#include "game_aqua_model.h"
#include "game_aqua.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AQUA_ROOM 916
enum {SCREEN_TITLE,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_NEXT,BUTTON_RETRY,BUTTON_PAY,BUTTON_EXIT};

typedef struct {
 GamePort *port;
 GameBank *levelBank,*subBank;
 SWLScene *levelScene,*subScene;
 AquaModel model;
 unsigned screen,loadedLevel;
 bool levelReady,clawTouch,left,right,up,down;
 int pad;
 char resource[32];
} Aqua;

AquaModel *game_aqua_model(void *game){return &((Aqua *)game)->model;}
static bool level_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Aqua *a=game;return bank==0&&a->levelReady?true:(bank==1&&a->subScene!=NULL);}
static void hide_claw(Aqua *a){swl_visible(a->subScene,"root.claw",false);}
static void attach_level(Aqua *a){a->levelBank=game_port_bank(a->port,0);a->levelScene=game_bank_scene(a->levelBank);a->levelReady=a->levelScene!=NULL;if(a->levelReady)game_port_snap(a->port);}
static void request_level(Aqua *a,unsigned level){
 if(a->levelReady&&a->loadedLevel==level)return;
 if(a->levelBank)game_port_release_bank(a->port,0);
 a->levelBank=NULL;a->levelScene=NULL;a->levelReady=false;a->loadedLevel=level;
 snprintf(a->resource,sizeof a->resource,"games/aqua%u.gpk",level+1);
 GameResource state=game_port_require_bank(a->port,0);
 if(state==GAME_RESOURCE_READY)attach_level(a);
}
static void sync(Aqua *a){
 AquaModel *m=&a->model;if(!a->levelReady||!a->subScene)return;
 double width=m->width*80.0,height=m->height*80.0;
 double camX=m->x-380,camY=m->y-240;
 if(camX<0)camX=0;
 if(camY<0)camY=0;
 if(camX>width-760)camX=width-760;
 if(camY>height-480)camY=height-480;
 if(camX<0)camX=0;
 if(camY<0)camY=0;
 swl_transform(a->levelScene,"root.bg",(SWLMatrix){1,0,0,1,(float)-camX,(float)-camY});
 swl_visible(a->levelScene,"root.bg",true);
 swl_transform(a->subScene,"root.sub",(SWLMatrix){1,0,0,1,(float)(m->x-camX),(float)(m->y-camY)});
 swl_visible(a->subScene,"root.sub",true);
 if(m->claw){swl_target(a->subScene,"root.claw",m->claw+1,NULL,false);swl_transform(a->subScene,"root.claw",(SWLMatrix){1,0,0,1,(float)(m->x-camX),(float)(m->y-camY+34)});swl_visible(a->subScene,"root.claw",true);}else hide_claw(a);
 game_port_snap(a->port);
}
static void start(Aqua *a){a->screen=SCREEN_GAME;aqua_model_start(&a->model);request_level(a,0);sync(a);}
static void *create(GamePort *port){Aqua *a=calloc(1,sizeof *a);if(!a)return NULL;a->port=port;aqua_model_init(&a->model);return a;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Aqua *a=game;a->subBank=game_port_bank(a->port,1);a->subScene=game_bank_scene(a->subBank);if(!a->subScene)return false;
 hide_claw(a);a->screen=SCREEN_TITLE;request_level(a,0);return true;
}
static void tick(void *game){
 Aqua *a=game;if(!a->levelReady){GameResource state=game_port_require_bank(a->port,0);if(state==GAME_RESOURCE_READY)attach_level(a);else return;}
 if(a->levelScene)swl_tick_deferred(a->levelScene);
 if(a->subScene)swl_tick_deferred(a->subScene);
 if(a->screen!=SCREEN_GAME||game_port_session(a->port)!=GAME_SESSION_PLAYING)return;
 int dx=a->left?-1:a->right?1:0,dy=a->up?-1:a->down?1:0;
 aqua_model_step(&a->model,dx,dy,a->clawTouch);
 sync(a);
 if(a->model.phase==AQUA_DONE||a->model.phase==AQUA_OVER||a->model.phase==AQUA_WON)a->screen=SCREEN_RESULTS;
}
static void frame(void *game,float seconds,const GameInput *input){
 Aqua *a=game;(void)seconds;unsigned clicked=game_port_clicked(a->port);
 a->left=(input->held&GAME_KEY_LEFT)||input->stickX<-.5f;
 a->right=(input->held&GAME_KEY_RIGHT)||input->stickX>.5f;
 a->up=(input->held&GAME_KEY_UP)||input->stickY>.5f;
 a->down=(input->held&GAME_KEY_DOWN)||input->stickY<-.5f;
 if(a->pad<0)a->left=true;
 if(a->pad>0)a->right=true;
 a->clawTouch=(input->held&(GAME_KEY_A|GAME_KEY_B))!=0;
 if(a->screen==SCREEN_TITLE){
  if(clicked==BUTTON_EXIT){game_port_leave(a->port);return;}
  if((clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))&&game_port_session(a->port)==GAME_SESSION_PLAYING)start(a);
  return;
 }
 if(a->screen==SCREEN_RESULTS){
  if(a->model.phase==AQUA_DONE){if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_A)){aqua_model_next(&a->model);if(a->model.phase!=AQUA_WON){a->screen=SCREEN_GAME;request_level(a,a->model.level);sync(a);}}}
  else if(a->model.phase==AQUA_OVER){if(clicked==BUTTON_RETRY||(input->down&GAME_KEY_A)){a->model.phase=AQUA_PLAYING;a->model.lives=2;aqua_model_load(&a->model);a->screen=SCREEN_GAME;sync(a);}}
  else if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(a->port,aqua_model_total(&a->model));
  return;
 }
 if(clicked==BUTTON_EXIT){game_port_finish(a->port,aqua_model_total(&a->model));return;}
}
static void touch(void *game,GameTouch phase,float x,float y){
 Aqua *a=game;if(a->screen!=SCREEN_GAME||y<190)return;
 if(phase==GAME_TOUCH_UP){a->pad=0;a->clawTouch=false;return;}
 a->pad=x<80?-1:x>240?1:0;
 if(x>=110&&x<=210)a->clawTouch=true;
}
static void close_game(void *game){Aqua *a=game;if(a->screen==SCREEN_TITLE)game_port_leave(a->port);else game_port_finish(a->port,aqua_model_total(&a->model));}
static void draw_bottom(void *game){
 Aqua *a=game;GamePort *p=a->port;AquaModel *m=&a->model;char line[96];
 if(a->screen==SCREEN_TITLE){game_port_title_band(p);game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);game_port_button(p,BUTTON_EXIT,196,197,84,36,"Exit",true);return;}
 if(a->screen==SCREEN_RESULTS){
  snprintf(line,sizeof line,"Score %u",aqua_model_total(m));game_port_text(p,160,40,16,0xffffff,GAME_ALIGN_CENTER,line);
  if(m->phase==AQUA_DONE){game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Level complete!");game_port_button(p,BUTTON_NEXT,90,110,140,42,"Next level",true);}
  else if(m->phase==AQUA_OVER){game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Out of air.");game_port_button(p,BUTTON_RETRY,90,110,140,42,"Retry",true);}
  else game_port_button(p,BUTTON_PAY,90,110,140,44,"Get coins",true);
  return;
 }
 game_port_rect(p,0,0,320,28,0x003366);
 snprintf(line,sizeof line,"Level %u  Treasure %u/5  Cargo %u  Lives %u",m->level+1,m->treasure,m->cargo,m->lives);
 game_port_text(p,5,8,8,0xffffff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,266,2,50,20,"Exit",true);
 game_port_button(p,BUTTON_PLAY+10,4,196,58,40,"<",a->left);
 game_port_button(p,BUTTON_PLAY+11,258,196,58,40,">",a->right);
 game_port_button(p,BUTTON_PLAY+12,80,196,70,40,"Up",a->up);
 game_port_button(p,BUTTON_PLAY+13,150,196,70,40,"Down",a->down);
 game_port_button(p,BUTTON_PLAY+14,110,152,100,36,"Claw",a->clawTouch);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Aqua *a=game;return bank==0?a->resource:bank==1?"games/aqua-sub.gpk":NULL;}
#endif
const GameController GAME_AQUA_CONTROLLER={.key="aqua",.kind=GAME_SOLO,.room=AQUA_ROOM,.rate=24,.banks={"games/aqua1.gpk","games/aqua-sub.gpk",NULL},.lazy=1,.order={0,1,2},.backgroundRGB=0x0280cd,
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.bank_view=level_view};
