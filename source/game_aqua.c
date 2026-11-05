#include "game_controller.h"
#include "game_aqua_model.h"
#include "game_aqua.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AQUA_ROOM 916
enum {SCREEN_TITLE,SCREEN_INTRO,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_NEXT,BUTTON_RETRY,BUTTON_PAY,BUTTON_EXIT};

typedef struct {
 GamePort *port;
 GameBank *levelBank,*subBank;
 SWLScene *levelScene,*subScene;
 AquaModel model;
 unsigned screen,loadedLevel,introTicks,ticks,flood;
 bool levelReady,clawTouch,left,right,up,down;
 int pad,vpad;bool clawPad;
 bool onTop,bandDrawn,steering;
 float touchX,touchY; /* stylus point while steering (touch-screen pixels) */
 double camX,camY; /* level point at the top-left of the two-screen playfield */
 char resource[32];
} Aqua;

AquaModel *game_aqua_model(void *game){return &((Aqua *)game)->model;}
static bool level_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Aqua *a=game;return bank==0&&a->levelReady?true:(bank==1&&a->subScene!=NULL);}
/* One playfield over both screens (as the console holds them): the top screen is 400x240 at (0,0)
   and the touch screen 320x240 at (40,240) of a 400x480 window, both at the original 760-wide scale.
   The sub sits high on the top screen, so the touch screen is the deeper water under it. */
#define VIEW_S (400.0f/760)
#define SUB_ROW 140.0f /* window y of the sub */
#define HEAD_H 24
#define FOOT_Y 198
static void camera(Aqua *a){
 const AquaModel *m=&a->model;double width=m->width*80.0,height=m->height*80.0;
 double cx=m->x-200/VIEW_S,cy=m->y-SUB_ROW/VIEW_S;
 cx=fmin(cx,width-400/VIEW_S);cy=fmin(cy,height-480/VIEW_S);
 a->camX=fmax(0,cx);a->camY=fmax(0,cy);
}
/* Sub position on the touch screen (negative y: it is up on the top screen). */
static void sub_lower(const Aqua *a,float *x,float *y){*x=(float)(a->model.x-a->camX)*VIEW_S-40;*y=(float)(a->model.y-a->camY)*VIEW_S-240;}
/* top_view runs only for the top screen draw and lower_view right before the touch-screen draw,
   so this tells overlay() which screen the sub bank is being drawn on. */
static bool top_view(void *game,SWLMatrix *view){((Aqua *)game)->onTop=true;*view=(SWLMatrix){VIEW_S,0,0,VIEW_S,0,0};return true;}
static bool lower_view(void *game,SWLMatrix *view){
 Aqua *a=game;a->onTop=false;
 /* Menus: a close-up of the sub at the net (the top screen keeps the wide view), not a second copy of it. */
 if(a->screen!=SCREEN_GAME){const float k=1.05f;float sx=(float)(a->model.x-a->camX),sy=(float)(a->model.y-a->camY);
  *view=(SWLMatrix){k,0,0,k,160-sx*k,a->screen==SCREEN_TITLE?112-sy*k:84-sy*k};return a->levelReady;}
 *view=(SWLMatrix){VIEW_S,0,0,VIEW_S,-40,-240};return a->levelReady;
}
/* Touch screen in play: the sub bank's frame 2 (same sub/puffle/claw plus the hudbar icons). */
/* Touch-screen header band, painted inside the sub bank before its first entry (the bank reports a
   single root depth, then FLT_MAX) so the hudbar icons sit on it; draw_bottom writes the text over it. */
static void overlay(void *game,unsigned bank,float depth,const GameCanvas *canvas){
 Aqua *a=game;if(bank!=1||a->onTop||a->screen!=SCREEN_GAME)return;
 if(depth>=FLT_MAX){a->bandDrawn=false;return;}
 if(a->bandDrawn)return;
 a->bandDrawn=true;
 const float s=VIEW_S,x0=40/s,x1=360/s;
 canvas->line(canvas->user,x0,(240+HEAD_H/2.0f)/s,x1,(240+HEAD_H/2.0f)/s,HEAD_H/s,0x062646);
 canvas->line(canvas->user,x0,(240+HEAD_H+.5f)/s,x1,(240+HEAD_H+.5f)/s,1/s,0x4fc0ff);
}
static unsigned lower_frame(void *game,unsigned bank){Aqua *a=game;return bank==1&&a->screen==SCREEN_GAME?2:1;}
static void hide_claw(Aqua *a){swl_visible(a->subScene,"root.claw",false);}
static void sync(Aqua *a);
static void attach_level(Aqua *a){a->levelBank=game_port_bank(a->port,0);a->levelScene=game_bank_scene(a->levelBank);a->levelReady=a->levelScene!=NULL;if(a->levelReady){sync(a);game_port_snap(a->port);}}
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
 camera(a);double camX=a->camX,camY=a->camY;
 swl_transform(a->levelScene,"root.bg",(SWLMatrix){1,0,0,1,(float)-camX,(float)-camY});
 swl_visible(a->levelScene,"root.bg",true);
 swl_transform(a->subScene,"root.sub",(SWLMatrix){1,0,0,1,(float)(m->x-camX),(float)(m->y-camY)});
 swl_visible(a->subScene,"root.sub",true);
 /* The original air readout: the cabin floods (water frames 1-100) and the pilot reacts. */
 unsigned flood=(unsigned)lround(100-(m->air<0?0:m->air));if(flood<1)flood=1;if(flood>100)flood=100;
 if(flood!=a->flood){a->flood=flood;swl_target(a->subScene,"root.sub.cabin.water",flood,NULL,false);swl_target(a->subScene,"root.sub.cabin.water2",flood,NULL,false);
  swl_target(a->subScene,"root.sub.cabin.pilot",1,flood>=60?"freakin":flood>=40?"concerned":"calm",false);}
 swl_goto(a->subScene,"root.puffle",1,m->pufflePinched?"pinched":"normal",false);
 swl_transform(a->subScene,"root.puffle",(SWLMatrix){1,0,0,1,(float)(m->puffleX-camX),(float)(m->puffleY-camY)});
 swl_visible(a->subScene,"root.puffle",true);
 if(m->claw){swl_target(a->subScene,"root.claw",m->claw+1,NULL,false);swl_transform(a->subScene,"root.claw",(SWLMatrix){1,0,0,1,(float)(m->x-camX),(float)(m->y-camY+34)});swl_visible(a->subScene,"root.claw",true);}else hide_claw(a);
 /* hudbar icons, fixed on the touch screen: stage = (screen + (40,240)) / VIEW_S. */
 const float k=.5f/VIEW_S;
 swl_transform(a->subScene,"root.hudsub",(SWLMatrix){k,0,0,k,(214+40)/VIEW_S,(13+240)/VIEW_S});
 swl_transform(a->subScene,"root.hudcoin",(SWLMatrix){k,0,0,k,(160+40)/VIEW_S,(12+240)/VIEW_S});
 game_port_snap(a->port);
}
static void start(Aqua *a){a->screen=SCREEN_INTRO;a->introTicks=48;aqua_model_start(&a->model);request_level(a,0);sync(a);}
static void *create(GamePort *port){Aqua *a=calloc(1,sizeof *a);if(!a)return NULL;a->port=port;aqua_model_init(&a->model);return a;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Aqua *a=game;a->subBank=game_port_bank(a->port,1);a->subScene=game_bank_scene(a->subBank);if(!a->subScene)return false;
 hide_claw(a);a->screen=SCREEN_TITLE;aqua_model_start(&a->model);request_level(a,0);sync(a);return true;
}
static void tick(void *game){
 Aqua *a=game;if(!a->levelReady){GameResource state=game_port_require_bank(a->port,0);if(state==GAME_RESOURCE_READY)attach_level(a);else return;}
 if(a->levelScene)swl_tick_deferred(a->levelScene);
 if(a->subScene)swl_tick_deferred(a->subScene);
 a->ticks++;
 if(a->screen==SCREEN_INTRO){if(a->introTicks){a->introTicks--;return;}a->screen=SCREEN_GAME;}
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
 /* A held stylus steers on its own (it names a target point; buttons would fight it). */
 if(a->steering){a->left=a->pad<0;a->right=a->pad>0;a->up=a->vpad<0;a->down=a->vpad>0;}
 a->clawTouch=(input->held&(GAME_KEY_A|GAME_KEY_B))!=0||a->clawPad;
 if(a->screen==SCREEN_INTRO){if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))a->introTicks=0;return;}
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
/* Touch: the Claw pad in the footer; anywhere on the water steers the sub toward the stylus. */
static void touch(void *game,GameTouch phase,float x,float y){
 Aqua *a=game;if(a->screen!=SCREEN_GAME)return;
 a->pad=a->vpad=0;a->clawPad=false;a->steering=false;
 if(phase==GAME_TOUCH_UP||x<0||x>=320||y<HEAD_H||y>=240)return;
 if(y>=FOOT_Y-4&&x>=204){a->clawPad=true;return;}
 if(y>=FOOT_Y)return;
 a->steering=true;a->touchX=x;a->touchY=y;
 float sx,sy;sub_lower(a,&sx,&sy);sy+=20; /* aim from the cabin */
 float dx=x-sx,dy=y-sy,dead=12;
 a->pad=dx<-dead?-1:dx>dead?1:0;
 /* Steer mostly by direction: a shallow drag sideways does not also dive. */
 a->vpad=dy<-dead&&fabsf(dy)>fabsf(dx)*.4f?-1:dy>dead&&fabsf(dy)>fabsf(dx)*.4f?1:0;
}
bool game_aqua_claw_held(void *game){return ((Aqua *)game)->clawTouch;}
static void close_game(void *game){Aqua *a=game;if(a->screen==SCREEN_TITLE)game_port_leave(a->port);else game_port_finish(a->port,aqua_model_total(&a->model));}
static void draw_bottom(void *game){
 Aqua *a=game;GamePort *p=a->port;AquaModel *m=&a->model;char line[96];
 if(a->screen==SCREEN_INTRO){game_port_rect(p,0,0,320,26,0x062646);game_port_rect(p,0,26,320,1,0x4fc0ff);
  game_port_text(p,160,5,14,0xffffff,GAME_ALIGN_CENTER,a->model.level==0?"Level 1 - The Iceberg":"Level 2 - The Deep");
  game_port_rect(p,0,134,320,1,0x4fc0ff);game_port_rect(p,0,135,320,55,0x062646);
  game_port_text(p,160,140,11,0xd6ecff,GAME_ALIGN_CENTER,"Grab five treasures with the claw,");
  game_port_text(p,160,155,11,0xd6ecff,GAME_ALIGN_CENTER,"then bring them back to the net.");
  game_port_text(p,160,172,10,0xa9d4f5,GAME_ALIGN_CENTER,"Rise to the surface for air.");
  game_port_title_band(p);game_port_button(p,BUTTON_PLAY,90,196,140,38,"Dive!",true);return;}
 if(a->screen==SCREEN_TITLE){game_port_rect(p,0,0,320,26,0x062646);game_port_rect(p,0,26,320,1,0x4fc0ff);
  game_port_text(p,160,5,14,0xffffff,GAME_ALIGN_CENTER,"Aqua Grabber");
  game_port_title_band(p);game_port_button(p,BUTTON_PLAY,40,197,150,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);game_port_button(p,BUTTON_EXIT,206,197,74,36,"Exit",true);return;}
 if(a->screen==SCREEN_RESULTS){
  game_port_rect(p,0,0,320,240,0x0b3a63);game_port_rect(p,0,0,320,30,0x062646);game_port_rect(p,0,30,320,1,0x4fc0ff);
  game_port_text(p,160,8,13,0xffffff,GAME_ALIGN_CENTER,m->phase==AQUA_DONE?"Level complete!":m->phase==AQUA_OVER?"Out of air":"Aqua Grabber complete!");
  snprintf(line,sizeof line,"Score %u",aqua_model_total(m));game_port_text(p,160,62,18,0xffd23c,GAME_ALIGN_CENTER,line);
  if(m->phase==AQUA_DONE){game_port_button(p,BUTTON_NEXT,80,110,160,44,"Next level",true);game_port_text(p,160,164,10,0xa9d4f5,GAME_ALIGN_CENTER,"or press A");}
  else if(m->phase==AQUA_OVER){game_port_button(p,BUTTON_RETRY,80,110,160,44,"Retry",true);game_port_text(p,160,164,10,0xa9d4f5,GAME_ALIGN_CENTER,"or press A");}
  else game_port_button(p,BUTTON_PAY,80,110,160,44,"Get coins",true);
  return;
 }
 /* Header band: level, treasures, coin and sub icons from the original hudbar (drawn by the scene under it). */
 snprintf(line,sizeof line,"Level %u",m->level+1);game_port_text(p,6,6,11,0xffffff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"Treasure %u/5",m->treasure);game_port_text(p,62,6,11,m->treasure>=5?0x7dff7a:0xffffff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"%u",aqua_model_total(m));game_port_text(p,172,6,11,0xffd23c,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"x%u",m->lives);game_port_text(p,232,6,11,0xffffff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,266,2,50,20,"Exit",true);
 /* Footer: air (the cabin floods as it runs out), time, the Claw pad. */
 game_port_rect(p,0,FOOT_Y,320,240-FOOT_Y,0x062646);game_port_rect(p,0,FOOT_Y-1,320,1,0x4fc0ff);
 float air=(float)(m->air<0?0:m->air)/100;bool low=air<.3f;
 game_port_text(p,8,FOOT_Y+6,10,low&&(a->ticks/6)%2?0xff5a4f:0xffffff,GAME_ALIGN_LEFT,"Air");
 game_port_rect(p,34,FOOT_Y+6,122,12,0xffffff);game_port_rect(p,35,FOOT_Y+7,120,10,0x0b3d6e);
 game_port_rect(p,35,FOOT_Y+7,120*air,10,low?0xff5a4f:air<.6f?0xffc83c:0x39d3f0);
 unsigned seconds=m->time/24;snprintf(line,sizeof line,"Time %u:%02u",seconds/60,seconds%60);
 game_port_text(p,8,FOOT_Y+23,10,seconds<20?0xff5a4f:0xd6ecff,GAME_ALIGN_LEFT,line);
 game_port_text(p,82,FOOT_Y+23,9,0xa9d4f5,GAME_ALIGN_LEFT,"Touch water to steer");
 game_port_pad(p,206,FOOT_Y+3,110,36,"Claw (A)",a->clawTouch);
 /* Steering feedback: a small target ring under the stylus. */
 if(a->steering){float tx=a->touchX,ty=fmaxf(HEAD_H+8,fminf(a->touchY,FOOT_Y-9));
  game_port_rect(p,tx-7,ty-1,5,2,0xffffff);game_port_rect(p,tx+2,ty-1,5,2,0xffffff);
  game_port_rect(p,tx-1,ty-7,2,5,0xffffff);game_port_rect(p,tx-1,ty+2,2,5,0xffffff);}
 if(m->treasure>=5){game_port_rect(p,60,HEAD_H+6,200,18,0x062646);game_port_text(p,160,HEAD_H+9,10,0x7dff7a,GAME_ALIGN_CENTER,"Return to the net at the surface!");}
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Aqua *a=game;return bank==0?a->resource:bank==1?"games/aqua-sub.gpk":NULL;}
#endif
const GameController GAME_AQUA_CONTROLLER={.key="aqua",.kind=GAME_SOLO,.room=AQUA_ROOM,.rate=24,.banks={"games/aqua1.gpk","games/aqua-sub.gpk",NULL},.lazy=1,.order={0,1,2},.nightPanel=true,.backgroundRGB=0x0280cd,
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.bank_view=level_view,.top_view=top_view,.lower_view=lower_view,.lower_frame=lower_frame,.overlay=overlay};
