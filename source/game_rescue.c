#include "game_controller.h"
#include "game_rescue_model.h"
#include "game_rescue.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESCUE_ROOM 949
enum {SCREEN_TITLE,SCREEN_INTRO,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_UP,BUTTON_DOWN,BUTTON_LEFT,BUTTON_RIGHT,BUTTON_NEXT,BUTTON_PAY,BUTTON_EXIT};
typedef struct {GamePort *port;GameBank *bank[3];SWLScene *scene[3];RescueModel model;unsigned screen,introTicks;int pad;} Rescue;
RescueModel *game_rescue_model(void *game){return &((Rescue *)game)->model;}
static const char *STYLES[]={"skier","indy","jet"};

static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Rescue *r=game;return bank==rescue_model_level(&r->model)->theme;}
static void target(Rescue *r,const char *path,unsigned frame,double x,double y){SWLScene *s=r->scene[rescue_model_level(&r->model)->theme];swl_target(s,path,frame,NULL,false);swl_transform(s,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});swl_visible(s,path,true);}
static void hide(Rescue *r,const char *path){SWLScene *s=r->scene[rescue_model_level(&r->model)->theme];swl_visible(s,path,false);}
#define PART_SLOTS 40
static void place_part(Rescue *r,unsigned *slot,unsigned frame,double x,double y){
 if(*slot>=PART_SLOTS)return;
 char path[32];snprintf(path,sizeof path,"root.part%u",(*slot)++);target(r,path,frame,x,y);
}
static void sync(Rescue *r){
 RescueModel *m=&r->model;const RescueLevelData *l=rescue_model_level(m);unsigned theme=l->theme;SWLScene *sc=r->scene[theme];
 swl_target(sc,"root.bg",theme+1,NULL,false);swl_goto(sc,"root.player",1,STYLES[theme],false);
 double width=l->width*80.0,height=l->height*80.0;double camX=m->x*80+40-380,camY=m->y*80+40-240;
 if(camX<0)camX=0;
 if(camY<0)camY=0;
 if(camX>width-760)camX=width-760;
 if(camY>height-480)camY=height-480;
 if(camX<0)camX=0;
 if(camY<0)camY=0;
 unsigned slot=0,partSlot=0;char path[32];
 for(unsigned y=(unsigned)(camY/80);y<l->height&&y*80<camY+480;y++)for(unsigned x=(unsigned)(camX/80);x<l->width&&x*80<camX+760;x++){
  unsigned at=y*l->width+x,tile=m->map[at];
  if(tile!=1&&slot<96){snprintf(path,sizeof path,"root.tile%u",slot++);target(r,path,tile,(double)x*80-camX,(double)y*80-camY);}
  unsigned part=m->parts[at],part2=m->parts2[at];
  if(part==3&&!m->broken[at])place_part(r,&partSlot,3,(double)x*80-camX,(double)y*80-camY);
  if(part>=30&&part<=38)place_part(r,&partSlot,part,(double)x*80-camX,(double)y*80-camY);
  if(part2>=30&&part2<=38)place_part(r,&partSlot,part2,(double)x*80-camX,(double)y*80-camY);
 }
 for(unsigned i=slot;i<96;i++){snprintf(path,sizeof path,"root.tile%u",i);hide(r,path);}
 for(unsigned i=0;i<m->platformCount;i++){unsigned x=0,y=0,frame=0;if(rescue_model_platform_cell(m,i,&x,&y,&frame))place_part(r,&partSlot,frame,(double)x*80-camX,(double)y*80-camY);}
 unsigned snowSlot=0,batSlot=0,secretSlot=0;
 for(unsigned i=0;i<m->turretCount;i++)place_part(r,&partSlot,25,(double)m->turrets[i].x*80-camX,(double)m->turrets[i].y*80-camY);
 for(unsigned i=0;i<m->shotCount;i++){if(snowSlot<4){snprintf(path,sizeof path,"root.snowball%u",snowSlot++);target(r,path,1,(double)m->shots[i].x*80-camX,(double)m->shots[i].y*80-camY);}}
 for(unsigned i=0;i<m->hazardCount;i++){unsigned x=0,y=0;if(!rescue_model_hazard_cell(m,i,&x,&y))continue;
  if(m->hazards[i].kind==0)place_part(r,&partSlot,2,(double)x*80-camX,(double)y*80-camY);
  else if(m->hazards[i].kind==1&&snowSlot<4){snprintf(path,sizeof path,"root.snowball%u",snowSlot++);target(r,path,1,(double)x*80-camX,(double)y*80-camY);}
  else if(m->hazards[i].kind==2&&batSlot<4){snprintf(path,sizeof path,"root.bat%u",batSlot++);target(r,path,1,(double)x*80-camX,(double)y*80-camY);}
  else if(m->hazards[i].kind==3)target(r,"root.kraken",1,(double)x*80-camX,(double)y*80-camY);
  else if(m->hazards[i].kind==4&&secretSlot<2){snprintf(path,sizeof path,"root.secret%u",secretSlot++);target(r,path,1,(double)x*80-camX,(double)y*80-camY);}
 }
 for(unsigned i=snowSlot;i<4;i++){snprintf(path,sizeof path,"root.snowball%u",i);hide(r,path);}
 for(unsigned i=batSlot;i<4;i++){snprintf(path,sizeof path,"root.bat%u",i);hide(r,path);}
 for(unsigned i=secretSlot;i<2;i++){snprintf(path,sizeof path,"root.secret%u",i);hide(r,path);}
 for(unsigned i=0;i<3;i++)if(!m->collected[i])place_part(r,&partSlot,40,(double)l->puffleX[i]*80-camX,(double)l->puffleY[i]*80-camY);
 for(unsigned i=partSlot;i<PART_SLOTS;i++){snprintf(path,sizeof path,"root.part%u",i);hide(r,path);}
 swl_transform(sc,"root.player",(SWLMatrix){1,0,0,1,(float)(m->x*80-camX),(float)(m->y*80-camY)});
 game_port_snap(r->port);
}
static void start(Rescue *r){r->screen=SCREEN_INTRO;r->introTicks=48;rescue_model_start(&r->model);sync(r);}
static void next(Rescue *r){rescue_model_next(&r->model);if(r->model.phase!=RESCUE_WON){r->screen=SCREEN_GAME;sync(r);}}
static void *create(GamePort *port){Rescue *r=calloc(1,sizeof *r);if(!r)return NULL;r->port=port;rescue_model_init(&r->model);return r;}
static void destroy(void *game){free(game);}
static bool ready(void *game){Rescue *r=game;for(unsigned i=0;i<3;i++){r->bank[i]=game_port_bank(r->port,i);r->scene[i]=game_bank_scene(r->bank[i]);if(!r->scene[i])return false;}r->screen=SCREEN_TITLE;return true;}
static void tick(void *game){Rescue *r=game;for(unsigned i=0;i<3;i++)if(r->scene[i])swl_tick_deferred(r->scene[i]);if(r->screen==SCREEN_INTRO){if(r->introTicks){r->introTicks--;return;}r->screen=SCREEN_GAME;}if(r->screen==SCREEN_GAME&&game_port_session(r->port)==GAME_SESSION_PLAYING){rescue_model_tick(&r->model,1000.0/24.0);sync(r);}}
static void frame(void *game,float seconds,const GameInput *input){
 Rescue *r=game;(void)seconds;unsigned clicked=game_port_clicked(r->port);RescueModel *m=&r->model;
 if(r->screen==SCREEN_INTRO){if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))r->introTicks=0;return;}
 if(r->screen==SCREEN_TITLE){if(clicked==BUTTON_EXIT){game_port_leave(r->port);return;}if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))start(r);return;}
 if(r->screen==SCREEN_RESULTS){if(m->phase==RESCUE_DONE){if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_A))next(r);}else if(m->phase==RESCUE_WON){if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(r->port,rescue_model_total(m));}return;}
 if(clicked==BUTTON_EXIT){game_port_finish(r->port,rescue_model_total(m));return;}
 int dx=0,dy=0;if(clicked==BUTTON_LEFT||(input->down&GAME_KEY_LEFT))dx=-1;else if(clicked==BUTTON_RIGHT||(input->down&GAME_KEY_RIGHT))dx=1;else if(clicked==BUTTON_UP||(input->down&GAME_KEY_UP))dy=-1;else if(clicked==BUTTON_DOWN||(input->down&GAME_KEY_DOWN))dy=1;
 if(dx||dy){rescue_model_step(m,dx,dy);if(m->phase==RESCUE_DONE||m->phase==RESCUE_WON)r->screen=SCREEN_RESULTS;sync(r);}
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Rescue *r=game;game_port_finish(r->port,rescue_model_total(&r->model));}
static void draw_bottom(void *game){
 Rescue *r=game;GamePort *p=r->port;RescueModel *m=&r->model;char line[80];
 if(r->screen==SCREEN_INTRO){game_port_title_band(p);snprintf(line,sizeof line,"%s",rescue_model_level(m)->name);game_port_text(p,160,70,14,0xffffff,GAME_ALIGN_CENTER,line);game_port_text(p,160,100,10,0xd6ecff,GAME_ALIGN_CENTER,"Collect three puffles and bring them home.");game_port_text(p,160,140,9,0xffffff,GAME_ALIGN_CENTER,"Tap Play or press A to start.");game_port_button(p,BUTTON_PLAY,90,180,140,40,"Play",true);return;}
 if(r->screen==SCREEN_TITLE){game_port_title_band(p);game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);game_port_button(p,BUTTON_EXIT,196,197,84,36,"Exit",true);return;}
 if(r->screen==SCREEN_RESULTS){snprintf(line,sizeof line,"Score %u",m->score);game_port_text(p,160,40,16,0xffffff,GAME_ALIGN_CENTER,line);if(m->phase==RESCUE_DONE){game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Level complete!");game_port_button(p,BUTTON_NEXT,90,110,140,42,"Next level",true);}else{game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"All nine levels complete!");game_port_button(p,BUTTON_PAY,90,110,140,44,"Get coins",true);}return;}
 game_port_rect(p,0,0,320,28,0x003366);snprintf(line,sizeof line,"%s  Puffles %u/3  Carried %u  Moves %u",rescue_model_level(m)->name,m->rescued,m->carried,m->moves);game_port_text(p,5,8,9,0xffffff,GAME_ALIGN_LEFT,line);game_port_button(p,BUTTON_EXIT,266,2,50,20,"Exit",true);
 game_port_button(p,BUTTON_LEFT,4,196,58,40,"<",false);
 game_port_button(p,BUTTON_UP,70,190,58,22,"Up",false);
 game_port_button(p,BUTTON_DOWN,70,216,58,22,"Down",false);
 game_port_button(p,BUTTON_RIGHT,258,196,58,40,">",false);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
const GameController GAME_RESCUE_CONTROLLER={.key="rescue",.kind=GAME_SOLO,.room=RESCUE_ROOM,.rate=24,.banks={"games/rescue-snow.gpk","games/rescue-temple.gpk","games/rescue-volcanoe.gpk"},.order={0,1,2},.backgroundRGB=0x071c33,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view};
