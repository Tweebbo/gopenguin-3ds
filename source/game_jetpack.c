#include "game_controller.h"
#include "game_jetpack_model.h"
#include "game_jetpack.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JETPACK_ROOM 906
#define TILE_SLOTS 96
#define HAZARD_SLOTS 12 /* tools/export-game.mjs JETPACK_HAZARD_SLOTS */
enum {SCREEN_TITLE,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_THRUST,BUTTON_NEXT,BUTTON_RETRY,BUTTON_PAY,BUTTON_LEAVE,BUTTON_EXIT};

/* Touch-screen layout (320x240). */
#define HEAD_H 26
#define MAP_X 6
#define MAP_Y 30
#define MAP_W 308
#define MAP_H 92
#define FUEL_Y 127
#define PAD_Y 158
#define PAD_H 78
#define SIDE_W 84

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene,*backdrop; /* backdrop: original background_mc (frame 1), interFace pieces (frame 2), titleScreen (frame 3) */
 JetpackModel model;
 bool started,thrust,left,right,down,sceneReady;
 unsigned screen,loadedLevel,ticks;
 double camX,camY;
 float viewY; /* eased vertical offset of the 400x240 crop of the 760x480 stage */
 float mapX,mapY; /* eased mini-map scroll (tiles) */
 int pad;bool thrustPad; /* lower-screen flight pad held by the stylus */
 char resource[40];
} Jetpack;

JetpackModel *game_jetpack_model(void *game){return &((Jetpack *)game)->model;}

static void target(Jetpack *j,const char *path,unsigned frame,double x,double y){
 swl_target(j->scene,path,frame,NULL,false);
 swl_transform(j->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});
 swl_visible(j->scene,path,true);
}
static void hide(Jetpack *j,const char *path){swl_visible(j->scene,path,false);}

/* The original drawStage(): background_mc, the 80x80 tiles (pickups and wind), the 480x480 terrain
   pieces over them, then the pilot. Terrain pieces overhang their cell, so neighbours are placed too. */
static void sync_tiles(Jetpack *j){
 JetpackModel *m=&j->model;const JetpackLevelData *l=jetpack_model_level(m);
 double width=l->width*80.0,height=l->height*80.0;
 j->camX=m->x-380;if(j->camX<0)j->camX=0;if(j->camX>width-760)j->camX=width-760;
 j->camY=m->y-240;if(j->camY<0)j->camY=0;if(j->camY>height-480)j->camY=height-480;
 if(j->camX<0)j->camX=0;
 if(j->camY<0)j->camY=0;
 /* Keep a descending pilot on screen even below the final terrain row. */
 if(m->y-j->camY>400)j->camY=m->y-400;
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
 slot=0;
 int hx0=(int)(j->camX/JETPACK_HAZARD),hx1=(int)((j->camX+760)/JETPACK_HAZARD),hy0=(int)(j->camY/JETPACK_HAZARD),hy1=(int)((j->camY+480)/JETPACK_HAZARD);
 /* Pass 0: cells under the view; pass 1: the overhanging neighbours (left column, row above). */
 for(unsigned pass=0;pass<2;pass++)for(int y=hy0-(int)pass;y<=hy1;y++)for(int x=hx0-(int)pass;x<=hx1;x++){
  bool inner=x>=hx0&&y>=hy0;if((pass==0)!=inner)continue;
  if(x<0||y<0||(unsigned)x>=l->hazardW||(unsigned)y>=l->hazardH||slot>=HAZARD_SLOTS)continue;
  unsigned piece=l->hazards[(unsigned)y*l->hazardW+(unsigned)x];if(piece<=1)continue;
  char path[24];snprintf(path,sizeof path,"root.haz%u",slot++);
  target(j,path,piece,(double)x*JETPACK_HAZARD-j->camX,(double)y*JETPACK_HAZARD-j->camY);
 }
 for(unsigned i=slot;i<HAZARD_SLOTS;i++){char path[24];snprintf(path,sizeof path,"root.haz%u",i);hide(j,path);}
 double angle=m->rotation*3.141592653589793/180.0;float c=(float)cos(angle),s=(float)sin(angle);
 swl_transform(j->scene,"root.player",(SWLMatrix){c,s,-s,c,(float)(m->x-j->camX),(float)(m->y-j->camY)});
 swl_visible(j->scene,"root.player",true);
}

/* Touch-screen copies of the original interFace pieces (frame 2 of the backdrop bank), in screen pixels. */
static void sync_hud(Jetpack *j){
 SWLScene *b=j->backdrop;if(!b)return;
 JetpackModel *m=&j->model;
 swl_transform(b,"root.hudbar",(SWLMatrix){320.0f/762,0,0,HEAD_H/57.5f,0,0});
 /* fuelGuage is authored upright (293 tall) and turned a quarter in interFace; its mask's _yscale is the fuel. */
 const float k=.86f;swl_transform(b,"root.fuel",(SWLMatrix){0,k,-k,0,46,FUEL_Y+3});
 float fuel=(float)(m->fuel>0?m->fuel:0)/100;if(fuel<.001f)fuel=.001f;
 swl_transform(b,"root.fuel.mask",(SWLMatrix){1,0,0,fuel,0,3.3f});
 unsigned lives=m->lives>10?10:m->lives;swl_target(b,"root.lives",lives+1,NULL,false);
 /* The original jet-pack icons are 52 px tall; at .42 they touched the top edge of the 26 px header
    and lost their nozzle tips. Keep them fully inside the bar. */
 swl_transform(b,"root.lives",(SWLMatrix){.40f,0,0,.40f,190,16});
 swl_transform(b,"root.coin",(SWLMatrix){.36f,0,0,.36f,104,13});
 swl_transform(b,"root.title",(SWLMatrix){1,0,0,1,-1.7f,-1.15f});
}
static void sync_backdrop(Jetpack *j){
 if(!j->backdrop)j->backdrop=game_bank_scene(game_port_bank(j->port,1));
 if(j->backdrop){swl_target(j->backdrop,"root.bg",j->model.level+1,NULL,false);sync_hud(j);}
}
static void hide_scene(Jetpack *j){
 for(unsigned i=0;i<TILE_SLOTS;i++){char path[24];snprintf(path,sizeof path,"root.tile%u",i);hide(j,path);}
 for(unsigned i=0;i<HAZARD_SLOTS;i++){char path[24];snprintf(path,sizeof path,"root.haz%u",i);hide(j,path);}
 hide(j,"root.player");
}
static void attach_scene(Jetpack *j){
 j->bank=game_port_bank(j->port,0);j->scene=game_bank_scene(j->bank);j->sceneReady=j->scene!=NULL;
 if(j->sceneReady){hide_scene(j);sync_tiles(j);game_port_snap(j->port);}
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
static void center_map(Jetpack *j);
static void load_level(Jetpack *j){request_level(j,j->model.level);sync_backdrop(j);center_map(j);if(j->sceneReady){sync_tiles(j);game_port_snap(j->port);}}
static void start(Jetpack *j){j->started=true;j->screen=SCREEN_GAME;jetpack_model_start(&j->model);load_level(j);}

static void *create(GamePort *port){Jetpack *j=calloc(1,sizeof *j);if(!j)return NULL;j->port=port;jetpack_model_init(&j->model);return j;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 /* The menu shows level 1's launch pad on the top screen (the pilot waiting) under the title art below. */
 Jetpack *j=game;j->screen=SCREEN_TITLE;jetpack_model_start(&j->model);request_level(j,0);sync_backdrop(j);center_map(j);if(j->sceneReady){sync_tiles(j);game_port_snap(j->port);}return true;
}

/* Mini-map scale (pixels per 80x80 tile): the whole level when it fits, otherwise a scrolling window. */
/* Fill the panel along the level's short side (at most 5 px per tile) and scroll along the long one. */
static float map_scale(const JetpackLevelData *l){float s=fmaxf((float)MAP_W/l->width,(float)MAP_H/l->height);return s>5?5:s;}
static void map_window(const Jetpack *j,float *x,float *y){
 const JetpackLevelData *l=jetpack_model_level(&j->model);float s=map_scale(l),w=MAP_W/s,h=MAP_H/s;
 float cx=(float)(j->model.x/80)-w/2,cy=(float)(j->model.y/80)-h/2;
 cx=fmaxf(0,fminf(cx,(float)l->width-w));cy=fmaxf(0,fminf(cy,(float)l->height-h));
 *x=l->width<=w?0:cx;*y=l->height<=h?0:cy;
}
static void center_map(Jetpack *j){map_window(j,&j->mapX,&j->mapY);}

static void tick(void *game){
 Jetpack *j=game;
 if(!j->sceneReady){GameResource state=game_port_require_bank(j->port,0);if(state==GAME_RESOURCE_READY)attach_scene(j);else return;}
 swl_tick_deferred(j->scene);j->ticks++;
 if(j->screen!=SCREEN_GAME||game_port_session(j->port)!=GAME_SESSION_PLAYING)return;
 jetpack_model_step(&j->model,j->thrust,j->left,j->right,j->down);
 sync_tiles(j);if(j->model.level!=j->loadedLevel||!j->backdrop)sync_backdrop(j);else sync_hud(j);
 float wx,wy;map_window(j,&wx,&wy);j->mapX+=(wx-j->mapX)*.2f;j->mapY+=(wy-j->mapY)*.2f;
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
  if(m->phase==JETPACK_DONE){if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_A)){jetpack_model_next(m);if(m->phase!=JETPACK_WON){j->screen=SCREEN_GAME;request_level(j,m->level);sync_backdrop(j);center_map(j);}}}
  else if(m->phase==JETPACK_OVER){if(clicked==BUTTON_RETRY||(input->down&GAME_KEY_A)){j->screen=SCREEN_GAME;m->phase=JETPACK_PLAYING;jetpack_model_reset(m);request_level(j,m->level);center_map(j);}}
  else{if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(j->port,jetpack_model_total(m));else if(clicked==BUTTON_LEAVE)game_port_leave(j->port);}
  return;
 }
 if(clicked==BUTTON_EXIT){game_port_finish(j->port,jetpack_model_total(&j->model));return;}
 j->thrust=(input->held&(GAME_KEY_A|GAME_KEY_B|GAME_KEY_UP))!=0||input->stickY>.5f||j->thrustPad;
 j->left=(input->held&GAME_KEY_LEFT)||input->stickX<-.5f;
 j->right=(input->held&GAME_KEY_RIGHT)||input->stickX>.5f;
 j->down=(input->held&GAME_KEY_DOWN)||input->stickY<-.5f;
 if(j->pad<0)j->left=true;
 if(j->pad>0)j->right=true;
}

/* One flight pad across the bottom: holding anywhere thrusts, the outer thirds also tilt (one stylus
   point cannot hold two buttons). The mini-map and gauge rows are read-only. */
static void touch(void *game,GameTouch phase,float x,float y){
 Jetpack *j=game;
 if(phase==GAME_TOUCH_UP||j->screen!=SCREEN_GAME||x<0||x>=320||y<PAD_Y-4||y>=240){j->pad=0;j->thrustPad=false;return;}
 j->pad=x<SIDE_W?-1:x>=320-SIDE_W?1:0;j->thrustPad=true;
}

static void close_game(void *game){Jetpack *j=game;if(!j->started){game_port_leave(j->port);return;}game_port_finish(j->port,jetpack_model_total(&j->model));}

static void frame_rect(GamePort *p,float x,float y,float w,float h,uint32_t rgb){
 game_port_rect(p,x,y,w,1,rgb);game_port_rect(p,x,y+h-1,w,1,rgb);game_port_rect(p,x,y,1,h,rgb);game_port_rect(p,x+w-1,y,1,h,rgb);
}
/* Clip a rectangle to [x0,x0+w)x[y0,y0+h); false when nothing is left. */
static bool clip(float *x,float *y,float *w,float *h,float x0,float y0,float cw,float ch){
 if(*x<x0){*w-=x0-*x;*x=x0;}
 if(*y<y0){*h-=y0-*y;*y=y0;}
 if(*x+*w>x0+cw)*w=x0+cw-*x;
 if(*y+*h>y0+ch)*h=y0+ch-*y;
 return *w>0&&*h>0;
}
static bool solid_tile(unsigned t){return t>=15&&t!=28&&t!=29;}
static void draw_map(Jetpack *j){
 GamePort *p=j->port;JetpackModel *m=&j->model;const JetpackLevelData *l=jetpack_model_level(m);
 float s=map_scale(l),ox=j->mapX,oy=j->mapY,vw=MAP_W/s,vh=MAP_H/s;
 float drawnW=fminf((float)l->width,vw)*s,drawnH=fminf((float)l->height,vh)*s;
 float x0=MAP_X+(MAP_W-drawnW)/2,y0=MAP_Y+(MAP_H-drawnH)/2;
 game_port_rect(p,MAP_X-2,MAP_Y-2,MAP_W+4,MAP_H+4,0x0b2c4c);
 game_port_rect(p,x0,y0,drawnW,drawnH,0x0d4f7a);
 /* Terrain: the level's 480x480 pieces as soft blocks, then the solid collision tiles. */
 const float hs=6*s;
 for(unsigned hy=0;hy<l->hazardH;hy++)for(unsigned hx=0;hx<l->hazardW;hx++){
  if(l->hazards[hy*l->hazardW+hx]<=1)continue;
  float rx=x0+(hx*6-ox)*s,ry=y0+(hy*6-oy)*s,rw=hs,rh=hs;
  if(clip(&rx,&ry,&rw,&rh,x0,y0,drawnW,drawnH))game_port_rect(p,rx,ry,rw,rh,0x14608f);
 }
 unsigned ty0=(unsigned)fmaxf(0,oy),ty1=(unsigned)fminf((float)l->height,oy+vh+1),tx0=(unsigned)fmaxf(0,ox),tx1=(unsigned)fminf((float)l->width,ox+vw+1);
 float dot=s<3?2:s*.7f;
 for(unsigned ty=ty0;ty<ty1;ty++){
  float ry=y0+(ty-oy)*s;if(ry<y0||ry+s>y0+drawnH+.5f)continue;
  for(unsigned tx=tx0;tx<tx1;){
   unsigned t=m->map[ty*m->width+tx];
   if(solid_tile(t)){unsigned e=tx;while(e<tx1&&solid_tile(m->map[ty*m->width+e]))e++;
    float rx=x0+(tx-ox)*s,rw=(e-tx)*s,yy=ry,rh=s<2?2:s;
    if(clip(&rx,&yy,&rw,&rh,x0,y0,drawnW,drawnH))game_port_rect(p,rx,yy,rw,rh,0xc9e6f5);
    tx=e;continue;}
   float cx=x0+(tx-ox+.5f)*s,cy=ry+s*.5f;
   if(cx>=x0+1&&cx<=x0+drawnW-1){
    uint32_t rgb=0;float d=dot;
    if(t==3||t==10||t==11)rgb=0xffd23c;else if(t==4){rgb=0xffe680;d=dot+1;}else if(t==2)rgb=0xff5a4f;else if(t==5)rgb=0x7dff7a;
    else if(t==28||t==29){bool blink=(j->ticks/8)%2;rgb=blink?0x41e84f:0xffffff;d=dot+3;}
    if(rgb)game_port_rect(p,cx-d/2,cy-d/2,d,d,rgb);
   }
   tx++;
  }
 }
 /* What the top screen shows, then the pilot. */
 float vx=x0+(float)(j->camX/80-ox)*s,vy=y0+(float)((j->camY-j->viewY*760/400)/80-oy)*s,vwid=760.0f/80*s,vhei=456.0f/80*s;
 if(clip(&vx,&vy,&vwid,&vhei,x0,y0,drawnW,drawnH)&&vwid>2&&vhei>2)frame_rect(p,vx,vy,vwid,vhei,0x9fd8ff);
 float px=x0+(float)(m->x/80-ox)*s,py=y0+(float)(m->y/80-oy)*s;
 if(px>=x0&&px<=x0+drawnW&&py>=y0&&py<=y0+drawnH){game_port_rect(p,px-3,py-3,6,6,0x062646);game_port_rect(p,px-2,py-2,4,4,0xff8a1f);}
 frame_rect(p,MAP_X-2,MAP_Y-2,MAP_W+4,MAP_H+4,0x4fc0ff);
}

static void draw_bottom(void *game){
 Jetpack *j=game;GamePort *p=j->port;char line[96];
 if(j->screen==SCREEN_TITLE){
  game_port_title_band(p);
  game_port_button(p,BUTTON_PLAY,40,197,150,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);
  game_port_button(p,BUTTON_EXIT,206,197,74,36,"Exit",true);return;
 }
 JetpackModel *m=&j->model;
 if(j->screen==SCREEN_RESULTS){
  game_port_rect(p,0,0,320,240,0x0b2e52);game_port_rect(p,0,0,320,30,0x062646);game_port_rect(p,0,30,320,1,0x4fc0ff);
  const char *title=m->phase==JETPACK_DONE?"Level complete!":m->phase==JETPACK_OVER?"Game over":"Jet Pack Adventure complete!";
  game_port_text(p,160,8,13,0xffffff,GAME_ALIGN_CENTER,title);
  if(m->phase==JETPACK_DONE){snprintf(line,sizeof line,"Level %u landed with %u coins and %u%% fuel",m->level+1,m->coins,(unsigned)(m->fuel>0?m->fuel:0));game_port_text(p,160,56,11,0xd6ecff,GAME_ALIGN_CENTER,line);}
  snprintf(line,sizeof line,"Score %u",jetpack_model_total(m));game_port_text(p,160,80,18,0xffd23c,GAME_ALIGN_CENTER,line);
  if(m->phase==JETPACK_DONE){game_port_button(p,BUTTON_NEXT,80,130,160,44,"Next level",true);game_port_text(p,160,184,10,0xa9d4f5,GAME_ALIGN_CENTER,"or press A");}
  else if(m->phase==JETPACK_OVER){game_port_text(p,160,110,11,0xd6ecff,GAME_ALIGN_CENTER,"Out of jet packs.");game_port_button(p,BUTTON_RETRY,80,130,160,44,"Retry level",true);game_port_text(p,160,184,10,0xa9d4f5,GAME_ALIGN_CENTER,"or press A");}
  else{bool online=game_port_descriptor(p)->online;snprintf(line,sizeof line,online?"You earned %u coins.":"Offline preview: no coins.",jetpack_model_total(m));game_port_text(p,160,110,12,0xffffff,GAME_ALIGN_CENTER,line);
   game_port_button(p,BUTTON_PAY,80,130,160,44,online?"Get coins":"Exit",true);}
  return;
 }
 /* Header over the original interFace bar: level, coins (original coin), jet packs left (lives_mc). */
 snprintf(line,sizeof line,"Level %u/%u",m->level+1,JETPACK_LEVELS);
 game_port_text(p,8,7,11,0xffffff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"%u",m->coins);game_port_text(p,113,7,11,0xffd23c,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,262,3,54,20,"Exit",true);
 draw_map(j);
 /* Fuel row: original fuelGuage art at 46..298. */
 game_port_text(p,8,FUEL_Y+5,10,m->fuel<=25&&(j->ticks/6)%2?0xff5a4f:0xffffff,GAME_ALIGN_LEFT,"Fuel");
 if(m->fuel<=25&&m->fuel>0)game_port_text(p,306,FUEL_Y+5,10,0xff5a4f,GAME_ALIGN_RIGHT,"!");
 /* Flight pad: hold anywhere to thrust; the outer thirds tilt as well. */
 bool l=j->pad<0||j->left,r=j->pad>0||j->right;
 game_port_pad(p,4,PAD_Y,SIDE_W-8,PAD_H,"<",l);
 game_port_pad(p,SIDE_W,PAD_Y,320-2*SIDE_W,PAD_H,"Thrust",j->thrust);
 game_port_pad(p,320-SIDE_W+4,PAD_Y,SIDE_W-8,PAD_H,">",r);
 game_port_text(p,SIDE_W/2,PAD_Y+PAD_H-20,10,0x0b3d6e,GAME_ALIGN_CENTER,"Tilt");
 game_port_text(p,320-SIDE_W/2,PAD_Y+PAD_H-20,10,0x0b3d6e,GAME_ALIGN_CENTER,"Tilt");
 game_port_text(p,160,PAD_Y+PAD_H-20,10,0x0b3d6e,GAME_ALIGN_CENTER,"Hold, or press A");
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Jetpack *j=game;return bank==0?j->resource:bank==1?"games/jetpackbg.gpk":NULL;}
#endif
/* The top screen shows 760x456 of the 760x480 stage; keep the penguin inside the crop instead of losing the bottom rows. */
static bool top_view(void *game,SWLMatrix *view){
 Jetpack *j=game;const float s=400.0f/760,lowest=240-480*s;
 float want=j->screen!=SCREEN_RESULTS?150-(float)(j->model.y-j->camY)*s:lowest;want=want>0?0:want<lowest?lowest:want;
 j->viewY+=(want-j->viewY)*.15f;*view=(SWLMatrix){s,0,0,s,0,j->viewY};return true;
}
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Jetpack *j=game;return bank==1?j->backdrop!=NULL:bank==0&&j->sceneReady;}
/* Touch screen: only the backdrop bank, at its interFace frame in play and its titleScreen frame on the menu. */
static bool lower_view(void *game,SWLMatrix *view){
 Jetpack *j=game;*view=(SWLMatrix){1,0,0,1,0,0};
 return j->backdrop!=NULL&&(j->screen==SCREEN_TITLE||(j->screen==SCREEN_GAME&&j->sceneReady));
}
static unsigned lower_frame(void *game,unsigned bank){Jetpack *j=game;return bank!=1?0:j->screen==SCREEN_TITLE?3:2;}
static bool lower_bank_view(void *game,unsigned bank,SWLMatrix *view){
 Jetpack *j=game;if(bank!=1||!j->backdrop)return false;
 const float s=320.0f/760; /* title art across the width, above the button band */
 *view=j->screen==SCREEN_TITLE?(SWLMatrix){s,0,0,s,0,0}:(SWLMatrix){1,0,0,1,0,0};return true;
}

const GameController GAME_JETPACK_CONTROLLER={
 .key="jetpack",.kind=GAME_SOLO,.room=JETPACK_ROOM,.rate=24,
 .banks={"games/jetpack.gpk","games/jetpackbg.gpk"},.lazy=1,.order={1,0,2},.nightPanel=true,.backgroundRGB=0x016691,
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.top_view=top_view,.lower_view=lower_view,
 .lower_bank_view=lower_bank_view,.lower_frame=lower_frame,
};
