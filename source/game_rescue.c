#include "game_controller.h"
#include "game_rescue_model.h"
#include "game_rescue.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESCUE_ROOM 949
#define TILE_SLOTS 144
#define PART_SLOTS 64
/* Dual screen: the upper screen is the wide view (12.5 x 7.5 tiles at 0.4) with the original HUD;
   the touch screen is the same scene at twice that size around the penguin. */
#define TOP_SCALE .4f
#define TOP_W 1000.0
#define TOP_H 600.0
/* The original HUD band covers the top 24 px of the upper screen: the map starts below it. */
#define TOP_Y 24.0f
#define TOP_VIS_H ((240.0-TOP_Y)/TOP_SCALE)
#define LOW_SCALE .8f
#define LOW_TOP 24.0f
#define LOW_W 400.0
#define LOW_H 270.0
#define HUD_SCALE (1000.0f/760.0f)
#define REPEAT_SECONDS .17f
enum {SCREEN_TITLE,SCREEN_INTRO,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_NEXT=6,BUTTON_PAY,BUTTON_EXIT};
typedef struct {uint32_t key;unsigned frame;double x,y;} RescuePart;
typedef struct {
 GamePort *port;
 GameBank *themeBank,*introBank;
 SWLScene *themeScene,*introScene;
 RescueModel model;
 unsigned screen,introTicks,introFrame,loadedTheme;
 bool themeReady;
 int pad,held;float repeat;
 double camX,camY,lowX,lowY;bool camSet;
 unsigned shownLevel,shownScore,shownPuffles,shownStyle;
 char resource[40];
 uint32_t partKey[PART_SLOTS]; /* object held by each root.partN, 0 = free */
 RescuePart wanted[PART_SLOTS];unsigned wantedCount;
} Rescue;
RescueModel *game_rescue_model(void *game){return &((Rescue *)game)->model;}
static const char *STYLES[]={"skier","indy","jet"};
static const char *THEME_BANKS[]={"games/rescue-snow.gpk","games/rescue-temple.gpk","games/rescue-volcanoe.gpk"};
static const char *INTRO_NAMES[]={"introSnow","introTemple","introVolcanoe"};
static const unsigned INTRO_FRAMES[]={26,26,27};
static double clampd(double v,double lo,double hi){return v<lo?lo:v>hi?hi:v;}
static void camera_goal(const RescueModel *m,double *x,double *y,double *lx,double *ly){
 const RescueLevelData *l=rescue_model_level(m);double w=l->width*80.0,h=l->height*80.0;
 *x=clampd(m->x*80+40-TOP_W/2,0,fmax(0,w-TOP_W));*y=clampd(m->y*80+40-TOP_VIS_H/2,0,fmax(0,h-TOP_VIS_H));
 *lx=clampd(m->x*80+40-LOW_W/2,0,fmax(0,w-LOW_W));*ly=clampd(m->y*80+40-LOW_H/2,0,fmax(0,h-LOW_H));
}
/* Cameras glide toward the penguin (snap on a new level). */
static void camera_follow(Rescue *r,bool snap){
 double x,y,lx,ly;camera_goal(&r->model,&x,&y,&lx,&ly);
 if(snap||!r->camSet){r->camX=x;r->camY=y;r->lowX=lx;r->lowY=ly;r->camSet=true;return;}
 double k=.35;r->camX+=(x-r->camX)*k;r->camY+=(y-r->camY)*k;r->lowX+=(lx-r->lowX)*k;r->lowY+=(ly-r->lowY)*k;
 if(fabs(x-r->camX)<.5)r->camX=x;
 if(fabs(y-r->camY)<.5)r->camY=y;
 if(fabs(lx-r->lowX)<.5)r->lowX=lx;
 if(fabs(ly-r->lowY)<.5)r->lowY=ly;
}
static SWLMatrix game_lower(const Rescue *r){return (SWLMatrix){LOW_SCALE,0,0,LOW_SCALE,(float)(-(r->lowX-r->camX)*LOW_SCALE),LOW_TOP-(float)((r->lowY-r->camY)*LOW_SCALE)};}
static bool lower_view(void *game,SWLMatrix *view){
 Rescue *r=game;
 if(r->screen!=SCREEN_GAME){*view=game_port_art_view(60,0,640,480);return r->themeReady;}
 *view=game_lower(r);return r->themeReady;
}
static bool top_view(void *game,SWLMatrix *view){
 Rescue *r=game;if(r->screen!=SCREEN_GAME)return false;
 *view=(SWLMatrix){TOP_SCALE,0,0,TOP_SCALE,0,TOP_Y};return true;
}
/* Bank 0 root frame 2 is the HUD-less touch-screen composition (tools/export-game.mjs rescueBank). */
static unsigned lower_frame(void *game,unsigned bank){(void)game;return bank==0?2:1;}

static void target(Rescue *r,const char *path,unsigned frame,double x,double y){if(!r->themeScene)return;swl_target(r->themeScene,path,frame,NULL,false);swl_transform(r->themeScene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});swl_visible(r->themeScene,path,true);}
static void hide(Rescue *r,const char *path){if(r->themeScene)swl_visible(r->themeScene,path,false);}
/* Parts are collected first, then each object keeps the root.partN it had while it stays visible
 * (like the PC's per-object instances). Handing out slots in scan order every tick made a slot switch
 * objects and frames, restarting nested clips: the ice block under the penguin flickered. */
static void place_part(Rescue *r,uint32_t key,unsigned frame,double x,double y){
 if(r->wantedCount>=PART_SLOTS)return;
 r->wanted[r->wantedCount++]=(RescuePart){key,frame,x,y};
}
static void flush_parts(Rescue *r){
 char path[32];bool kept[PART_SLOTS]={false},placed[PART_SLOTS]={false};
 for(unsigned w=0;w<r->wantedCount;w++)for(unsigned s=0;s<PART_SLOTS;s++)if(r->partKey[s]==r->wanted[w].key&&!kept[s]){kept[s]=placed[w]=true;break;}
 for(unsigned s=0;s<PART_SLOTS;s++)if(!kept[s])r->partKey[s]=0;
 for(unsigned w=0;w<r->wantedCount;w++){
  unsigned s=0;
  if(placed[w]){while(r->partKey[s]!=r->wanted[w].key)s++;}
  else{while(s<PART_SLOTS&&r->partKey[s])s++;if(s==PART_SLOTS)continue;r->partKey[s]=r->wanted[w].key;}
  snprintf(path,sizeof path,"root.part%u",s);target(r,path,r->wanted[w].frame,r->wanted[w].x,r->wanted[w].y);
 }
 for(unsigned s=0;s<PART_SLOTS;s++)if(!r->partKey[s]){snprintf(path,sizeof path,"root.part%u",s);hide(r,path);}
}
static void sync_hud(Rescue *r){
 SWLScene *sc=r->themeScene;RescueModel *m=&r->model;bool on=r->screen==SCREEN_GAME;
 swl_visible(sc,"root.hud",on);if(!on)return;
 unsigned theme=rescue_model_level(m)->theme;
 swl_transform(sc,"root.hud",(SWLMatrix){HUD_SCALE,0,0,HUD_SCALE,0,-TOP_Y/TOP_SCALE});
 swl_target(sc,"root.hud.borderart4",theme+1,NULL,false);
 swl_visible(sc,"root.hud.lives",false);
 unsigned shown=m->rescued+m->carried;if(shown>3)shown=3;
 swl_target(sc,"root.hud.puffles.holders",4,NULL,false);
 swl_target(sc,"root.hud.puffles.puffles",shown+1,NULL,false);
 char value[16];
 if(r->shownLevel!=m->level+1){r->shownLevel=m->level+1;snprintf(value,sizeof value,"%02u",m->level+1);game_bank_text(r->themeBank,"root.hud.stage_txt",value);}
 if(r->shownScore!=m->score+1){r->shownScore=m->score+1;snprintf(value,sizeof value,"%u",m->score);game_bank_text(r->themeBank,"root.hud.score_txt",value);}
}
static void sync(Rescue *r){
 RescueModel *m=&r->model;if(!r->themeReady||!r->themeScene)return;
 const RescueLevelData *l=rescue_model_level(m);unsigned theme=l->theme;SWLScene *sc=r->themeScene;
 swl_target(sc,"root.bg",theme+1,NULL,false);
 /* Select the player style once per theme: re-selecting it every tick restarted its nested clips (flicker). */
 if(r->shownStyle!=theme+1){r->shownStyle=theme+1;swl_goto(sc,"root.player",1,STYLES[theme],false);}
 bool wide=r->screen==SCREEN_GAME;
 swl_transform(sc,"root.bg",wide?(SWLMatrix){HUD_SCALE,0,0,HUD_SCALE,0,-TOP_Y/TOP_SCALE}:(SWLMatrix){1,0,0,1,0,0});
 double viewW=wide?TOP_W:760,viewH=wide?TOP_H:480;
 double camX,camY;
 if(wide){camX=r->camX;camY=r->camY;}
 else{double width=l->width*80.0,height=l->height*80.0;camX=clampd(m->x*80+40-380,0,fmax(0,width-760));camY=clampd(m->y*80+40-240,0,fmax(0,height-480));}
 char path[32];bool used[TILE_SLOTS]={false};r->wantedCount=0;
 for(unsigned y=(unsigned)(camY/80);y<l->height&&y*80<camY+viewH;y++)for(unsigned x=(unsigned)(camX/80);x<l->width&&x*80<camX+viewW;x++){
  unsigned at=y*l->width+x,tile=m->map[at];
  /* One stable slot per map cell (like the PC's t<x>-<y> instances): the visible window is at most
   * 14 x 9 cells, so (row mod 9, column mod 16) never collides. Reassigning slots in scan order made a
   * slot change frame whenever the camera moved, restarting its nested clips (tiles flickered). */
  if(tile!=1){unsigned cell=(y%9)*16+(x%16);used[cell]=true;snprintf(path,sizeof path,"root.tile%u",cell);target(r,path,tile,(double)x*80-camX,(double)y*80-camY);}
  unsigned part=m->parts[at],part2=m->parts2[at];
  if(part==3&&!m->broken[at])place_part(r,0x1000000u|at*4u,3,(double)x*80-camX,(double)y*80-camY);
  if(part>=30&&part<=38)place_part(r,0x1000001u|at*4u,part,(double)x*80-camX,(double)y*80-camY);
  if(part2>=30&&part2<=38)place_part(r,0x1000002u|at*4u,part2,(double)x*80-camX,(double)y*80-camY);
 }
 for(unsigned i=0;i<TILE_SLOTS;i++)if(!used[i]){snprintf(path,sizeof path,"root.tile%u",i);hide(r,path);}
 for(unsigned i=0;i<m->platformCount;i++){unsigned x=0,y=0,frame=0;if(rescue_model_platform_cell(m,i,&x,&y,&frame))place_part(r,0x2000000u|i,frame,(double)x*80-camX,(double)y*80-camY);}
 unsigned snowSlot=0,batSlot=0,secretSlot=0;
 for(unsigned i=0;i<m->turretCount;i++)place_part(r,0x3000000u|i,25,(double)m->turrets[i].x*80-camX,(double)m->turrets[i].y*80-camY);
 for(unsigned i=0;i<m->shotCount;i++){if(snowSlot<4){snprintf(path,sizeof path,"root.snowball%u",snowSlot++);target(r,path,1,(double)m->shots[i].x*80-camX,(double)m->shots[i].y*80-camY);}}
 for(unsigned i=0;i<m->hazardCount;i++){unsigned x=0,y=0;if(!rescue_model_hazard_cell(m,i,&x,&y))continue;
  if(m->hazards[i].kind==0)place_part(r,0x4000000u|i,2,(double)x*80-camX,(double)y*80-camY);
  else if(m->hazards[i].kind==1&&snowSlot<4){snprintf(path,sizeof path,"root.snowball%u",snowSlot++);target(r,path,1,(double)x*80-camX,(double)y*80-camY);}
  else if(m->hazards[i].kind==2&&batSlot<4){snprintf(path,sizeof path,"root.bat%u",batSlot++);target(r,path,1,(double)x*80-camX,(double)y*80-camY);}
  else if(m->hazards[i].kind==3)target(r,"root.kraken",1,(double)x*80-camX,(double)y*80-camY);
  else if(m->hazards[i].kind==4&&secretSlot<2){snprintf(path,sizeof path,"root.secret%u",secretSlot++);target(r,path,1,(double)x*80-camX,(double)y*80-camY);}
 }
 for(unsigned i=snowSlot;i<4;i++){snprintf(path,sizeof path,"root.snowball%u",i);hide(r,path);}
 for(unsigned i=batSlot;i<4;i++){snprintf(path,sizeof path,"root.bat%u",i);hide(r,path);}
 for(unsigned i=secretSlot;i<2;i++){snprintf(path,sizeof path,"root.secret%u",i);hide(r,path);}
 for(unsigned i=0;i<3;i++)if(!m->collected[i])place_part(r,0x5000000u|i,40,(double)l->puffleX[i]*80-camX,(double)l->puffleY[i]*80-camY);
 flush_parts(r);
 swl_transform(sc,"root.player",(SWLMatrix){1,0,0,1,(float)(m->x*80-camX),(float)(m->y*80-camY)});
 sync_hud(r);
 game_port_snap(r->port);
}
static void sync_intro(Rescue *r){
 if(!r->introScene)return;
 unsigned theme=rescue_model_level(&r->model)->theme;
 for(unsigned i=0;i<3;i++){char path[32];snprintf(path,sizeof path,"root.%s",INTRO_NAMES[i]);swl_visible(r->introScene,path,i==theme);}
 if(r->introFrame<1)r->introFrame=1;
 unsigned frames=INTRO_FRAMES[theme];r->introFrame=r->introFrame>frames?frames:r->introFrame;
 char path[32];snprintf(path,sizeof path,"root.%s",INTRO_NAMES[theme]);swl_target(r->introScene,path,r->introFrame,NULL,false);
 game_port_snap(r->port);
}
static void attach_theme(Rescue *r){r->shownStyle=0;memset(r->partKey,0,sizeof r->partKey);r->themeBank=game_port_bank(r->port,0);r->themeScene=game_bank_scene(r->themeBank);r->themeReady=r->themeScene!=NULL;if(r->themeReady)game_port_snap(r->port);}
static void request_theme(Rescue *r,unsigned theme){
 if(r->themeReady&&r->loadedTheme==theme)return;
 if(r->themeBank)game_port_release_bank(r->port,0);
 r->themeBank=NULL;r->themeScene=NULL;r->themeReady=false;r->loadedTheme=theme;
 snprintf(r->resource,sizeof r->resource,"%s",THEME_BANKS[theme]);
 GameResource state=game_port_require_bank(r->port,0);
 if(state==GAME_RESOURCE_READY)attach_theme(r);
}
static void begin_intro(Rescue *r){unsigned theme=rescue_model_level(&r->model)->theme;r->screen=SCREEN_INTRO;r->introTicks=INTRO_FRAMES[theme]*2;r->introFrame=1;}
static void start(Rescue *r){rescue_model_start(&r->model);begin_intro(r);request_theme(r,rescue_model_level(&r->model)->theme);sync_intro(r);}
static void next(Rescue *r){unsigned theme=rescue_model_level(&r->model)->theme;rescue_model_next(&r->model);if(r->model.phase!=RESCUE_WON){unsigned nextTheme=rescue_model_level(&r->model)->theme;request_theme(r,nextTheme);if(nextTheme!=theme){begin_intro(r);sync_intro(r);}else{r->screen=SCREEN_GAME;sync(r);}}}
static void *create(GamePort *port){Rescue *r=calloc(1,sizeof *r);if(!r)return NULL;r->port=port;rescue_model_init(&r->model);return r;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Rescue *r=game;r->introBank=game_port_bank(r->port,1);r->introScene=game_bank_scene(r->introBank);if(!r->introScene)return false;
 r->screen=SCREEN_TITLE;request_theme(r,0);return true;
}
static void play_effects(Rescue *r){unsigned sounds=rescue_model_take_sounds(&r->model);if(sounds&RESCUE_SOUND_PUFFLE)game_port_sound(r->port,"puffle",55,false);if(sounds&RESCUE_SOUND_HOME)game_port_sound(r->port,"rescue",55,false);if(sounds&RESCUE_SOUND_WIPEOUT)game_port_sound(r->port,"wipeout",55,false);}
static void enter_game(Rescue *r){r->screen=SCREEN_GAME;r->camSet=false;r->shownLevel=r->shownScore=0;camera_follow(r,true);sync(r);}
static void tick(void *game){
 Rescue *r=game;
 if(!r->themeReady){GameResource state=game_port_require_bank(r->port,0);if(state==GAME_RESOURCE_READY)attach_theme(r);else return;}
 /* In play the PC draws tiles, parts and the penguin as still frames; advancing their nested clips here
  * looped authored sub-animations (an ice block blinking under the penguin). */
 if(r->themeScene&&r->screen!=SCREEN_GAME)swl_tick_deferred(r->themeScene);
 if(r->introScene)swl_tick_deferred(r->introScene);
 if(r->screen==SCREEN_INTRO){if(!r->introTicks){enter_game(r);return;}if(r->introTicks%2==0&&r->introFrame<INTRO_FRAMES[rescue_model_level(&r->model)->theme])r->introFrame++;r->introTicks--;sync_intro(r);return;}
 if(r->screen==SCREEN_GAME&&game_port_session(r->port)==GAME_SESSION_PLAYING){rescue_model_tick(&r->model,1000.0/24.0);play_effects(r);camera_follow(r,false);sync(r);}
}
/* Direction codes: -1 left, 1 right, -2 up, 2 down. */
static void step(Rescue *r,int dir){
 RescueModel *m=&r->model;int dx=dir==-1?-1:dir==1?1:0,dy=dir==-2?-1:dir==2?1:0;
 rescue_model_step(m,dx,dy);play_effects(r);
 if(m->phase==RESCUE_DONE||m->phase==RESCUE_WON){r->screen=SCREEN_RESULTS;r->held=0;r->pad=0;}
 sync(r);
}
static void frame(void *game,float seconds,const GameInput *input){
 Rescue *r=game;unsigned clicked=game_port_clicked(r->port);RescueModel *m=&r->model;
 if(r->screen==SCREEN_INTRO){if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))r->introTicks=0;return;}
 if(r->screen==SCREEN_TITLE){if(clicked==BUTTON_EXIT){game_port_leave(r->port);return;}if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))start(r);return;}
 if(r->screen==SCREEN_RESULTS){if(m->phase==RESCUE_DONE){if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_A)){next(r);if(r->screen==SCREEN_GAME)enter_game(r);}}else if(m->phase==RESCUE_WON){if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(r->port,rescue_model_total(m));}return;}
 if(clicked==BUTTON_EXIT){game_port_finish(r->port,rescue_model_total(m));return;}
 /* D-pad / Circle Pad: one tile per press, repeating while held (the PC port's 170 ms). */
 int key=0;
 if(input->down&GAME_KEY_LEFT)key=-1;else if(input->down&GAME_KEY_RIGHT)key=1;else if(input->down&GAME_KEY_UP)key=-2;else if(input->down&GAME_KEY_DOWN)key=2;
 int hold=(input->held&GAME_KEY_LEFT)||input->stickX<-.6f?-1:(input->held&GAME_KEY_RIGHT)||input->stickX>.6f?1:(input->held&GAME_KEY_UP)||input->stickY>.6f?-2:(input->held&GAME_KEY_DOWN)||input->stickY<-.6f?2:0;
 if(r->pad)hold=r->pad;
 if(key){r->held=key;r->repeat=0;step(r,key);return;}
 if(!hold){r->held=0;r->repeat=0;return;}
 if(hold!=r->held){r->held=hold;r->repeat=0;step(r,hold);return;}
 r->repeat+=seconds;if(r->repeat>=REPEAT_SECONDS){r->repeat-=REPEAT_SECONDS;step(r,hold);}
}
/* Edge arrows on the touch screen: centre and hit box. */
typedef struct {int dir;float x,y,w,h;} Arrow;
static const Arrow ARROWS[4]={{-2,160,38,96,30},{2,160,226,96,28},{-1,16,132,32,96},{1,304,132,32,96}};
static int touch_dir(Rescue *r,float x,float y){
 for(unsigned i=0;i<4;i++){const Arrow *a=&ARROWS[i];if(fabsf(x-a->x)<=a->w/2&&fabsf(y-a->y)<=a->h/2)return a->dir;}
 SWLMatrix view=game_lower(r);RescueModel *m=&r->model;
 float px=(float)(m->x*80+40-r->camX),py=(float)(m->y*80+40-r->camY);
 float dx=(x-view.x)/view.a-px,dy=(y-view.y)/view.d-py;
 if(fmaxf(fabsf(dx),fabsf(dy))<40)return 0;
 return fabsf(dx)>fabsf(dy)?(dx<0?-1:1):(dy<0?-2:2);
}
static void touch(void *game,GameTouch phase,float x,float y){
 Rescue *r=game;if(r->screen!=SCREEN_GAME)return;
 if(phase==GAME_TOUCH_UP||x<0||x>=320||y<LOW_TOP||y>=240){r->pad=0;return;}
 int dir=touch_dir(r,x,y);
 /* A tap steps at once; holding repeats from frame() like the D-pad. */
 if(phase==GAME_TOUCH_DOWN){r->pad=dir;r->held=dir;r->repeat=0;if(dir)step(r,dir);}
 else if(dir&&dir!=r->pad){r->pad=dir;r->held=dir;r->repeat=0;step(r,dir);}
}
static void close_game(void *game){Rescue *r=game;game_port_finish(r->port,rescue_model_total(&r->model));}
/* Filled triangle from 1px rows (drawn chrome; the bank has no arrow art). */
static void triangle(GamePort *p,float cx,float cy,int dir,float half,float length,uint32_t rgb){
 for(int i=0;i<(int)length;i++){
  float w=half*(float)(i+1)/length;
  if(dir==-2)game_port_rect(p,cx-w,cy-length/2+(float)i,w*2,1,rgb);
  else if(dir==2)game_port_rect(p,cx-w,cy+length/2-(float)i-1,w*2,1,rgb);
  else if(dir==-1)game_port_rect(p,cx-length/2+(float)i,cy-w,1,w*2,rgb);
  else game_port_rect(p,cx+length/2-(float)i-1,cy-w,1,w*2,rgb);
 }
}
static void draw_arrow(Rescue *r,const Arrow *a){
 GamePort *p=r->port;RescueModel *m=&r->model;
 int dx=a->dir==-1?-1:a->dir==1?1:0,dy=a->dir==-2?-1:a->dir==2?1:0;
 bool open=rescue_model_passable(m,(unsigned)((int)m->x+dx),(unsigned)((int)m->y+dy));
 bool on=r->held==a->dir&&(r->pad==a->dir||r->repeat>0);
 triangle(p,a->x+1,a->y+2,a->dir,15,16,0x06203a);
 triangle(p,a->x,a->y,a->dir,15,16,0xffffff);
 triangle(p,a->x,a->y,a->dir,11,12,on?0xffd23f:open?0x3aa0ff:0x7d8fa3);
}
static void draw_bottom(void *game){
 Rescue *r=game;GamePort *p=r->port;RescueModel *m=&r->model;char line[80];
 if(r->screen==SCREEN_INTRO){game_port_title_band(p);snprintf(line,sizeof line,"%s",rescue_model_level(m)->name);game_port_text(p,160,70,14,0xffffff,GAME_ALIGN_CENTER,line);game_port_text(p,160,100,10,0xd6ecff,GAME_ALIGN_CENTER,"Collect three puffles and bring them home.");game_port_text(p,160,140,9,0xffffff,GAME_ALIGN_CENTER,"Tap Play or press A to start.");game_port_button(p,BUTTON_PLAY,90,180,140,40,"Play",true);return;}
 if(r->screen==SCREEN_TITLE){game_port_title_band(p);game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);game_port_button(p,BUTTON_EXIT,196,197,84,36,"Exit",true);return;}
 if(r->screen==SCREEN_RESULTS){snprintf(line,sizeof line,"Score %u",m->score);game_port_text(p,160,40,16,0xffffff,GAME_ALIGN_CENTER,line);if(m->phase==RESCUE_DONE){game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Level complete!");game_port_button(p,BUTTON_NEXT,90,110,140,42,"Next level",true);}else{game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"All nine levels complete!");game_port_button(p,BUTTON_PAY,90,110,140,44,"Get coins",true);}return;}
 /* Thin header: level and what the upper HUD does not show (carried puffles, moves). */
 game_port_rect(p,0,0,320,LOW_TOP,0x0b2a4a);game_port_rect(p,0,LOW_TOP-1,320,1,0x4fc0ff);
 snprintf(line,sizeof line,"%s",rescue_model_level(m)->name);game_port_text(p,6,6,10,0xffffff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"Carrying %u   Moves %u",m->carried,m->moves);game_port_text(p,96,7,9,0xd6ecff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,266,2,50,20,"Exit",true);
 for(unsigned i=0;i<4;i++)draw_arrow(r,&ARROWS[i]);
}
static void draw_top(void *game){
 Rescue *r=game;if(r->screen!=SCREEN_GAME||!r->themeScene||swl_frames(r->themeScene,"root")>=2)return;
 GamePort *p=r->port;const RescueModel *m=&r->model;char line[48];
 game_port_rect(p,0,0,400,TOP_Y,0x0b2a4a);game_port_rect(p,0,TOP_Y-1,400,1,0x4fc0ff);
 snprintf(line,sizeof line,"Puffles %u/3",m->rescued+m->carried>3?3:m->rescued+m->carried);
 game_port_text(p,12,6,10,0xd6ecff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"Level %02u",m->level+1);
 game_port_text(p,200,6,10,0xffffff,GAME_ALIGN_CENTER,line);
 snprintf(line,sizeof line,"Score %u",m->score);
 game_port_text(p,388,6,10,0xffe08a,GAME_ALIGN_RIGHT,line);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Rescue *r=game;return bank==0?r->resource:bank==1?"games/rescue-intro.gpk":NULL;}
#endif
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Rescue *r=game;return bank==0?r->themeReady:(bank==1&&r->screen==SCREEN_INTRO);}
const GameController GAME_RESCUE_CONTROLLER={.key="rescue",.kind=GAME_SOLO,.room=RESCUE_ROOM,.rate=24,.banks={"games/rescue-snow.gpk","games/rescue-intro.gpk",NULL},.lazy=1,.order={0,1,2},.nightPanel=true,.backgroundRGB=0x071c33,
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.draw_top=draw_top,.bank_view=bank_view,.top_view=top_view,.lower_view=lower_view,.lower_frame=lower_frame};
