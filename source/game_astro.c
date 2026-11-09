// Astro Barrier on the original cabinet. Model: game_astro_model.c at 24 Hz (parity with the PC model).
// Top screen: sprite 235 exactly as the model selects it (title, instructions, levels, level-done and
// restart clips, lose/win). Lower screen: the cabinet's controls and readouts at a readable size.
#include "game_controller.h"
#include "game_astro_model.h"
#include "game_astro.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASTRO_ROOM 900
enum {BUTTON_START=1,BUTTON_LEVEL10,BUTTON_LEVEL20,BUTTON_LEVEL30,BUTTON_CONTINUE,BUTTON_EXIT,BUTTON_PAY,BUTTON_LEAVE};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 AstroModel model;
 bool started,paid;
 unsigned shownFrame,shownScore,shownLevel,shownSecret;int shownLives;
 int pad;bool padFire;
 bool keyFire;
 bool aim;float aimX;
 float knobX;bool lowerPass;
} Astro;
/* Touch-screen control panel (bank root frame 2, drawn 1:1 in screen pixels). */
#define HEADER 22
#define FOOTER 218
#define STICK_X 84.f
#define STICK_Y 128.f
#define FIRE_X 236.f
#define FIRE_Y 124.f
#define FIRE_R 46.f

static const char *const INTRO_TEXT[4][2]={
 {"If you hit a blue target before hitting any other targets, you recieve an extra life.",""},
 {"Hit an orange switch...","... and any orange wall block will disappear."},
 {"A yellow barrier, when hit, turns into a green barrier.",""},
 {"A blaster shoots at the same time you do. Be careful, though... a blaster's bullets can hurt you!","You do not have to destroy a blaster to pass a level."},
};
static int intro_slot(unsigned level){return level==7?0:level==11?1:level==16?2:level==31?3:-1;}

AstroModel *game_astro_model(void *game){return &((Astro *)game)->model;}

static void target(Astro *a,const char *path,unsigned frame){swl_target(a->scene,path,frame?frame:1,NULL,false);}
static void text(Astro *a,const char *path,const char *value){game_bank_text(a->bank,path,value);}
static void place(Astro *a,const char *path,double x,double y,int rotation){
 double r=rotation*3.14159265358979/180;float c=(float)cos(r),s=(float)sin(r);
 swl_transform(a->scene,path,(SWLMatrix){c,s,-s,c,(float)x,(float)y});
}

static void sync_labels(Astro *a){
 AstroModel *m=&a->model;char value[16];
 if(a->shownSecret!=m->secretSet+1){a->shownSecret=m->secretSet+1;text(a,"root.astroBarrier.titleLabel",m->secretSet?"Secret Levels":"Astro Barrier");}
 text(a,"root.astroBarrier.levelLabel","L");text(a,"root.astroBarrier.livesLabel","Lives");
 if(a->shownScore!=m->score+1){a->shownScore=m->score+1;snprintf(value,sizeof value,"%u",m->score);text(a,"root.astroBarrier.score_txt",value);text(a,"root.astroBarrier.leveldone.score_txt",value);}
 if(a->shownLevel!=m->level){a->shownLevel=m->level;snprintf(value,sizeof value,"%u",m->level);text(a,"root.astroBarrier.level_txt",value);}
 if(a->shownLives!=m->lives){a->shownLives=m->lives;snprintf(value,sizeof value,"%d",m->lives>0?m->lives:0);text(a,"root.astroBarrier.lives_txt",value);}
}

static void sync(Astro *a){
 AstroModel *m=&a->model;
 unsigned display=astro_model_display_frame(m);
 if(display!=a->shownFrame){a->shownFrame=display;target(a,"root.astroBarrier",display);a->shownScore=0;a->shownLevel=0;a->shownLives=-99;a->shownSecret=0;game_port_snap(a->port);}
 sync_labels(a);
 if(m->phase==ASTRO_TITLE||m->phase==ASTRO_OVER||m->phase==ASTRO_WON){
  if(m->phase!=ASTRO_TITLE){text(a,"root.astroBarrier.finalScoreLabel","Final Score:");text(a,"root.astroBarrier.gameOverLabel",m->phase==ASTRO_WON?"YOU WIN!":"GAME OVER");}
  return;
 }
 target(a,"root.astroBarrier.gun",m->gunFrame);place(a,"root.astroBarrier.gun",m->gunX,ASTRO_GUN_Y,0);
 swl_visible(a->scene,"root.astroBarrier.bullet",m->bullet.active);target(a,"root.astroBarrier.bullet",m->bullet.active?2:1);
 place(a,"root.astroBarrier.bullet",m->bullet.x,m->bullet.y,0);
 swl_visible(a->scene,"root.astroBarrier.turretbullet",m->turretBullet.active);
 place(a,"root.astroBarrier.turretbullet",m->turretBullet.x,m->turretBullet.y,m->turretBullet.rotation);
 unsigned shots=m->shotsFrame,frames=swl_frames(a->scene,"root.astroBarrier.shots");if(frames&&shots>frames)shots=frames;
 target(a,"root.astroBarrier.shots",shots);
 swl_visible(a->scene,"root.astroBarrier.leveldone",m->phase==ASTRO_DONE);
 if(m->phase==ASTRO_DONE){target(a,"root.astroBarrier.leveldone",m->overlay+1);swl_visible(a->scene,"root.astroBarrier.leveldone.score_txt",false);}
 swl_visible(a->scene,"root.astroBarrier.levelrestart",m->phase==ASTRO_RESTART);
 if(m->phase==ASTRO_RESTART){
  target(a,"root.astroBarrier.levelrestart",m->overlay+1);
  char warning[64];snprintf(warning,sizeof warning,"%s",m->overlayText);for(char *c=warning;*c;c++)if(*c=='\r')*c='\n';
  text(a,"root.astroBarrier.levelrestart.warningText",warning);
 }
 swl_visible(a->scene,"root.astroBarrier.levelMember",false);
 if(m->phase==ASTRO_INTRO||m->phase==ASTRO_SECRET){
  target(a,"root.astroBarrier.enterToCont",m->introTicks+1<13?m->introTicks+1:13);
  target(a,"root.astroBarrier.secret",m->planeFrame?m->planeFrame:1);
  int slot=intro_slot(m->level);
  if(slot>=0){text(a,"root.astroBarrier.instructionText",INTRO_TEXT[slot][0]);text(a,"root.astroBarrier.instructionText1",INTRO_TEXT[slot][0]);text(a,"root.astroBarrier.instructionText2",INTRO_TEXT[slot][1]);}
 }
 char path[64],wall[96];
 for(unsigned i=0;i<m->objectCount;i++){
  const AstroObject *o=&m->objects[i];const AstroObjectData *d=o->data;
  snprintf(path,sizeof path,"root.astroBarrier.%s",d->name);
  if(d->kind==ASTRO_TURRET)place(a,path,o->matrix[4],o->matrix[5],0);
  if(d->clip>=0){target(a,path,o->clipFrame);snprintf(wall,sizeof wall,"%s.wall",path);target(a,wall,o->wallFrame);}
  else if(d->partCount){
   for(unsigned p=0;p<d->partCount;p++){
    const AstroPartData *pd=&ASTRO_PART_DATA[d->partFirst+p];
    snprintf(wall,sizeof wall,"%s.%s",path,pd->name);target(a,wall,o->parts[p].clipFrame);
    char inner[112];snprintf(inner,sizeof inner,"%s.wall",wall);target(a,inner,o->parts[p].hit?2:1);
   }
  }else if(d->kind==ASTRO_SWITCH||d->kind==ASTRO_TURRET||d->kind==ASTRO_ORANGE)target(a,path,o->frame);
  else if(d->kind==ASTRO_BONUS)target(a,path,o->hit?2:1);
 }
}

static void drain(Astro *a){
 for(unsigned i=0;i<a->model.soundCount;i++)game_port_sound(a->port,a->model.sounds[i],100,false);
 a->model.soundCount=0;
 for(unsigned i=0;i<a->model.stampCount;i++){
  char value[8];snprintf(value,sizeof value,"%u",a->model.stamps[i]);
  const char *args[]={value};game_port_send(a->port,'s',"st#sse",1,args);
 }
 a->model.stampCount=0;
}

static void *create(GamePort *port){Astro *a=calloc(1,sizeof *a);if(!a)return NULL;a->port=port;a->knobX=STICK_X;astro_model_init(&a->model);return a;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Astro *a=game;a->bank=game_port_bank(a->port,0);a->scene=game_bank_scene(a->bank);if(!a->scene)return false;
 a->shownFrame=0;sync(a);return true;
}
static void begin(Astro *a,unsigned level){a->started=true;astro_model_start(&a->model,level);sync(a);drain(a);}

static void tick(void *game){
 Astro *a=game;if(!a->scene)return;
 swl_tick_deferred(a->scene);
 if(game_port_session(a->port)!=GAME_SESSION_PLAYING)return;
 astro_model_trigger(&a->model,a->keyFire||a->padFire);
 astro_model_step(&a->model);
 drain(a);sync(a);
}

static void frame(void *game,float seconds,const GameInput *input){
 Astro *a=game;(void)seconds;if(!a->scene)return;
 AstroModel *m=&a->model;unsigned clicked=game_port_clicked(a->port);bool live=game_port_session(a->port)==GAME_SESSION_PLAYING;
 if(m->phase==ASTRO_TITLE){
  if(clicked==BUTTON_EXIT){game_port_leave(a->port);return;}
  if(!live)return;
  if(clicked==BUTTON_START||(input->down&(GAME_KEY_A|GAME_KEY_START)))begin(a,1);
  else if(clicked==BUTTON_LEVEL10)begin(a,10);
  else if(clicked==BUTTON_LEVEL20)begin(a,20);
  else if(clicked==BUTTON_LEVEL30)begin(a,30);
  return;
 }
 if(m->phase==ASTRO_OVER||m->phase==ASTRO_WON){
  if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(a->port,astro_model_score(m));
  else if(clicked==BUTTON_LEAVE)game_port_leave(a->port);
  return;
 }
 if(clicked==BUTTON_EXIT){game_port_finish(a->port,astro_model_score(m));return;}
 if(clicked==BUTTON_CONTINUE||(input->down&(GAME_KEY_START|GAME_KEY_X))){astro_model_confirm(m);sync(a);}
 a->keyFire=(input->held&(GAME_KEY_A|GAME_KEY_B))!=0;
 astro_model_release(m,0);
 if((input->held&GAME_KEY_LEFT)||input->stickX<-.5f||a->pad<0)astro_model_press(m,-1);
 else if((input->held&GAME_KEY_RIGHT)||input->stickX>.5f||a->pad>0)astro_model_press(m,1);
 else if(a->aim&&fabs(m->gunX-a->aimX)>4)astro_model_press(m,a->aimX<m->gunX?-1:1);
}

static void touch(void *game,GameTouch phase,float x,float y){
 Astro *a=game;a->aim=false;
 if(phase==GAME_TOUCH_UP||x<0||x>=320||y<HEADER||y>=FOOTER){a->pad=0;a->padFire=false;return;}
 /* Left half: the joystick (push it left or right of its centre). Right half: the fire button. */
 if(x<160){float dx=x-STICK_X;a->pad=dx<-10?-1:dx>10?1:0;a->padFire=false;}
 else{a->pad=0;a->padFire=true;}
}

static void close_game(void *game){
 Astro *a=game;
 if(!a->started||a->model.phase==ASTRO_TITLE){game_port_leave(a->port);return;}
 game_port_finish(a->port,astro_model_score(&a->model));
}

static void disc(GamePort *p,float cx,float cy,float rx,float ry,uint32_t rgb){
 for(int i=-(int)ry;i<=(int)ry;i++){float f=(float)i/ry,w=rx*sqrtf(fmaxf(0,1-f*f));game_port_rect(p,cx-w,cy+(float)i,w*2,1,rgb);}
}
static void draw_bottom(void *game){
 Astro *a=game;GamePort *p=a->port;AstroModel *m=&a->model;char line[96];
 game_port_rect(p,0,0,320,HEADER,0x0b2a4a);game_port_rect(p,0,HEADER-1,320,1,0x4fc0ff);
 game_port_text(p,6,5,10,0xffffff,GAME_ALIGN_LEFT,m->secretSet?"Astro Barrier - Secret Levels":"Astro Barrier");
 game_port_button(p,BUTTON_EXIT,266,1,50,19,"Exit",true);
 if(m->phase==ASTRO_TITLE){
  game_port_rect(p,16,32,288,58,0x0b2a4a);
  game_port_text(p,160,38,10,0xffffff,GAME_ALIGN_CENTER,"Shoot all the moving targets.");
  game_port_text(p,160,54,10,0xd6ecff,GAME_ALIGN_CENTER,"Left and Right move your ship. Hold A to shoot.");
  game_port_text(p,160,70,10,0xffe08a,GAME_ALIGN_CENTER,"Careful! You have limited bullets.");
  bool live=game_port_session(p)==GAME_SESSION_PLAYING;
  game_port_button(p,BUTTON_START,80,100,160,44,"Start  (A)",live);
  game_port_rect(p,100,152,120,15,0x0b2a4a);game_port_text(p,160,154,9,0xffe08a,GAME_ALIGN_CENTER,"Expert start");
  game_port_button(p,BUTTON_LEVEL10,16,172,92,36,"Level 10",live);
  game_port_button(p,BUTTON_LEVEL20,114,172,92,36,"Level 20",live);
  game_port_button(p,BUTTON_LEVEL30,212,172,92,36,"Level 30",live);
  return;
 }
 if(m->phase==ASTRO_OVER||m->phase==ASTRO_WON){
  game_port_rect(p,40,36,240,74,0x0b2a4a);
  game_port_text(p,160,42,16,m->phase==ASTRO_WON?0x9fe870:0xffffff,GAME_ALIGN_CENTER,m->phase==ASTRO_WON?"YOU WIN!":"GAME OVER");
  bool online=game_port_descriptor(p)->online;
  if(online)snprintf(line,sizeof line,"You earned %u coins.",m->score/10);else snprintf(line,sizeof line,"Offline preview: no coins are paid.");
  game_port_text(p,160,74,11,0xd6ecff,GAME_ALIGN_CENTER,line);
  bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
  game_port_button(p,BUTTON_PAY,90,124,140,44,waiting?"Paying...":online?"Get coins":"Exit",!waiting);
  if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,176,140,30,"Leave without waiting",true);
  return;
 }
 if(m->phase==ASTRO_INTRO||m->phase==ASTRO_SECRET){
  int slot=intro_slot(m->level);
  if(slot>=0){game_port_rect(p,8,30,304,84,0x0b2a4a);game_port_text(p,16,36,10,0xffffff,GAME_ALIGN_LEFT,INTRO_TEXT[slot][0]);if(*INTRO_TEXT[slot][1])game_port_text(p,16,74,10,0xd6ecff,GAME_ALIGN_LEFT,INTRO_TEXT[slot][1]);}
  game_port_button(p,BUTTON_CONTINUE,110,170,100,36,"Continue",m->introTicks>ASTRO_PROMPT_READY);
  return;
 }
 /* Fire button: a drawn arcade button (the original cabinet only has the joystick knob). */
 bool firing=a->padFire||a->keyFire;float dy=firing?3:0;
 disc(p,FIRE_X,FIRE_Y+6,FIRE_R+4,FIRE_R*.62f+4,0x2a0606);
 disc(p,FIRE_X,FIRE_Y+4,FIRE_R,FIRE_R*.62f,0x7a1010);
 disc(p,FIRE_X,FIRE_Y+dy-2,FIRE_R-6,FIRE_R*.55f,firing?0xb51c1c:0xe8302c);
 if(!firing)disc(p,FIRE_X-12,FIRE_Y-12,14,6,0xff8a7a);
 game_port_text(p,FIRE_X,FIRE_Y+dy-10,14,0xffffff,GAME_ALIGN_CENTER,"FIRE");
 game_port_text(p,FIRE_X,FIRE_Y+44,9,0x0b2a4a,GAME_ALIGN_CENTER,"or hold A");
 /* Direction marks either side of the joystick socket. */
 for(int s=-1;s<=1;s+=2){
  float x=STICK_X+(float)s*66,y=STICK_Y+62;bool on=s<0?(a->pad<0||m->left):(a->pad>0||m->right);
  for(int i=0;i<12;i++){float w=(float)(i+1)*.75f;game_port_rect(p,s<0?x-6+(float)i:x+6-(float)i-1,y-w,1,w*2,on?0xffd23f:0x0b2a4a);}
 }
 game_port_rect(p,0,FOOTER,320,240-FOOTER,0x0b2a4a);game_port_rect(p,0,FOOTER,320,1,0x4fc0ff);
 game_port_text(p,160,FOOTER+6,9,0xd6ecff,GAME_ALIGN_CENTER,"Stick or D-pad: move    FIRE or A: shoot");
}

static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
/* Upper screen: the whole cabinet monitor and its bezel, as the original stage frames it. */
static bool top_view(void *game,SWLMatrix *view){
 Astro *a=game;a->lowerPass=false;
 if(a->scene&&swl_frames(a->scene,"root")<2){
  swl_transform(a->scene,"root#1",(SWLMatrix){1,0,0,1,-9,-9});
  swl_visible(a->scene,"root.astroBarrier",true);
  swl_visible(a->scene,"root#121",true);
  swl_visible(a->scene,"root#122",true);
  swl_visible(a->scene,"root#130",true);
  swl_transform(a->scene,"root#130",(SWLMatrix){1,0,0,1,136,364});
  game_port_snap(a->port);
 }
 *view=(SWLMatrix){.552f,0,0,.552f,200-388*.552f,-6};return true;
}
static bool lower_view(void *game,SWLMatrix *view){
 Astro *a=game;a->lowerPass=true;
 if(a->scene&&swl_frames(a->scene,"root")<2){
  bool panel=!(a->model.phase==ASTRO_TITLE||a->model.phase==ASTRO_OVER||a->model.phase==ASTRO_WON);
  swl_transform(a->scene,"root#1",(SWLMatrix){1.65f,0,0,.5f,-9.f,-8.5f});
  swl_visible(a->scene,"root.astroBarrier",false);
  swl_visible(a->scene,"root#121",false);
  swl_visible(a->scene,"root#122",false);
  swl_visible(a->scene,"root#130",panel);
  swl_transform(a->scene,"root#130",(SWLMatrix){.8f,0,0,.8f,a->knobX-500.85f*.8f,STICK_Y-50.4f*.8f});
 }
 *view=(SWLMatrix){1,0,0,1,0,0};return true;
}
static unsigned lower_frame(void *game,unsigned bank){(void)game;return bank?0:2;}
/* Joystick socket and shaft under the original knob art (root depth 3 of the panel frame). */
static void overlay(void *game,unsigned bank,float depth,const GameCanvas *canvas){
 Astro *a=game;if(!a->lowerPass||bank||depth!=3)return;
 if(a->model.phase==ASTRO_TITLE||a->model.phase==ASTRO_OVER||a->model.phase==ASTRO_WON)return;
 float bx=STICK_X,by=STICK_Y+64;
 for(int i=-14;i<=14;i++){float f=(float)i/14,w=56*sqrtf(fmaxf(0,1-f*f));canvas->line(canvas->user,bx-w,by+(float)i,bx+w,by+(float)i,1.2f,i<-6?0x3a4a5a:0x1c2630);}
 for(int i=-9;i<=9;i++){float f=(float)i/9,w=30*sqrtf(fmaxf(0,1-f*f));canvas->line(canvas->user,bx-w,by+(float)i-2,bx+w,by+(float)i-2,1.2f,0x05090d);}
 canvas->line(canvas->user,bx,by-2,a->knobX,STICK_Y+4,9,0x2b2f36);
 canvas->line(canvas->user,bx-2,by-2,a->knobX-2,STICK_Y+4,3,0x6c7480);
}
static void draw_top(void *game){
 Astro *a=game;
 /* Knob follows the stick, then the shown frame is set for the touch-screen pass. */
 float goal=STICK_X+(a->model.left?-26.f:a->model.right?26.f:0);a->knobX+=(goal-a->knobX)*.5f;
 bool panel=!(a->model.phase==ASTRO_TITLE||a->model.phase==ASTRO_OVER||a->model.phase==ASTRO_WON);
 swl_visible(a->scene,"root.knob",panel);
 swl_transform(a->scene,"root.knob",(SWLMatrix){.8f,0,0,.8f,a->knobX,STICK_Y});
 if(a->model.phase!=ASTRO_DONE)return;
 /* The shipped atlas contains static placeholder digits for this field. */
 char score[16];snprintf(score,sizeof score,"%u",a->model.score);
 game_port_text(a->port,199,100,12,0xffff00,GAME_ALIGN_CENTER,score);
}

const GameController GAME_ASTRO_CONTROLLER={
 .key="astro",.kind=GAME_SOLO,.room=ASTRO_ROOM,.rate=24,
 .banks={"games/astro.gpk"},.order={0,1,2},.nightPanel=true,.backgroundRGB=0x006600,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.overlay=overlay,.top_view=top_view,.lower_view=lower_view,.lower_frame=lower_frame,.draw_top=draw_top,
};
