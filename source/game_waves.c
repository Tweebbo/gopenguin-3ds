// Catchin' Waves controller (web: src/games/waves.ts), dual screen.
// Upper screen: the original gameplay composition (sky, the 58x wave face, parallax foam, crest
// curl, breaking wave, surfer, shark, screeninterface HUD), the original title sign over an
// attract ride on the menu, and the original end screens with their score fields.
// Lower screen (root frame 2 of the bank): the original mode panels as touch targets, then the
// steering pad (the HUD's red arrows on the wave-face texture) with Jump and a status strip.
#include "game_controller.h"
#include "game_waves_model.h"
#include "game_waves.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAVES_ROOM 912
/* Emulator review builds only (make STABILITY_DIAGNOSTICS='0 -DWAVES_REVIEW=n'): 1 keeps the title
 * menu up, 2 ends the first ride after 3 s (result screen), 3 starts Survival (shark). */
#ifndef WAVES_REVIEW
#define WAVES_REVIEW 0
#endif
enum {SCREEN_MENU,SCREEN_GAME,SCREEN_RESULT};
enum {BUTTON_PAY=1,BUTTON_AGAIN,BUTTON_EXIT,BUTTON_START,BUTTON_MODE_BASE=20};

/* Lower-screen layout (320x240). */
#define HEADER_H 26.0f
#define MENU_SCALE (320.0f/760.0f)
#define MENU_STAGE_Y 104.0f
#define MENU_BOTTOM 161.0f
#define PAD_SCALE .5f
#define PAD_CX 84.0f
#define PAD_CY 142.0f
#define PAD_STEP 37.0f
#define STRIP_H 24.0f
#define JUMP_X 194.0f
#define JUMP_Y 88.0f
#define JUMP_W 118.0f
#define JUMP_H 96.0f
#define FACE_RATE_DEG 16.0
/* Menu panel edges: 3..80, 81..158, 159..236, 237..318 (the original panel art). */
#define PA_1 81.0f
#define PA_2 159.0f
#define PA_3 237.0f

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 WavesModel model,demo;
 unsigned screen,selected,demoTicks;
 bool started,left,right,up,down,jumpTouch,jumpHeld;
 signed char pad,vpad;
 double surfX[3],bob;int bobDir;
 const char *playerLabel;
 unsigned endFrame;
} Waves;

WavesModel *game_waves_model(void *game){return &((Waves *)game)->model;}
/* Panel order of the original title (left to right). */
static const WavesMode PANEL_MODES[4]={WAVES_LESSON,WAVES_FREESTYLE,WAVES_COMPETITION,WAVES_SURVIVAL};
static const char *MODE_NAMES[]={"Freestyle","Survival","Competition","Surf Lesson"};
static const char *MODE_HELP[][2]={
 {"Surf for fun and practise your tricks.","Three waves, no pressure."},
 {"Go as far as you can on the longest wave.","A shark is hunting you: keep your speed."},
 {"Three waves in front of the judges.","Score 1000 trick points to win."},
 {"Learn to ride: climb to the lip and jump,","then spin and land on your board."},
};
static const char *SOUND_NAMES[]={"splash4","achord1","splash4","drip","dchord2","splash4"};
static const char *SOUND_KEYS[]={"jump","trick","wipeout","wave","win","lose"};
static const char *SURFACES[3]={"root.surface2","root.surface3","root.surface4"};
static const char *ARROWS[4]={"root.arrowup","root.arrowright","root.arrowdown","root.arrowleft"};

static const char *sound_name(const char *key){for(unsigned i=0;i<6;i++)if(!strcmp(key,SOUND_KEYS[i]))return SOUND_NAMES[i];return NULL;}
static void text(Waves *w,const char *path,unsigned value){char v[16];snprintf(v,sizeof v,"%u",value);game_bank_text(w->bank,path,v);}
static void show(Waves *w,const char *path,bool on){swl_visible(w->scene,path,on);}

static bool lower_view(void *game,SWLMatrix *view){
 Waves *w=game;
 if(w->screen==SCREEN_MENU){*view=(SWLMatrix){MENU_SCALE,0,0,MENU_SCALE,0,HEADER_H-MENU_STAGE_Y*MENU_SCALE};return true;}
 /* Pad art is laid out around stage (1300,240) in root frame 2. */
 *view=(SWLMatrix){PAD_SCALE,0,0,PAD_SCALE,PAD_CX-1300*PAD_SCALE,PAD_CY-240*PAD_SCALE};return true;
}
static unsigned lower_frame(void *game,unsigned bank){(void)game;return bank?0:2;}

static const WavesModel *shown(const Waves *w){return w->screen==SCREEN_MENU?&w->demo:&w->model;}
static void set_label(Waves *w,const char *label){if(w->playerLabel!=label){swl_goto(w->scene,"root.player",1,label,true);w->playerLabel=label;}}

/** Upper scene from the shown model: the camera follows the surfer up and down the face. */
static void sync_art(Waves *w){
 const WavesModel *m=shown(w);SWLScene *s=w->scene;
 /* Crest line on screen: the sky shows above the lip; deep on the face the camera drops with the surfer. */
 double crest=215.0-.6*m->y;if(crest>230)crest=230;if(crest<0)crest=0;
 float cy=(float)crest;
 swl_transform(s,"root.water",(SWLMatrix){58.4f,0,0,58.4f,380,cy});
 for(unsigned i=0;i<3;i++)swl_transform(s,SURFACES[i],(SWLMatrix){1,0,0,1,(float)w->surfX[i],cy+502+(float)w->bob*(float)(i+2)*.5f});
 float waveX=(float)m->curl-1200;
 swl_transform(s,"root.wave",(SWLMatrix){1,0,0,1,waveX,cy});
 swl_transform(s,"root.curl",(SWLMatrix){1,0,0,1,waveX+1510,cy});
 /* Original perspective: _xscale = (player._y - map.water._y + 200) / 4. */
 double scale=(m->y+200.0)/400.0;if(scale<1)scale=1;if(scale>2)scale=2;
 double angle=m->air?m->spin+m->flip:(m->wipe>0?0:-m->ride/240.0*FACE_RATE_DEG);
 angle*=3.141592653589793/180.0;float c=(float)(cos(angle)*scale),sn=(float)(sin(angle)*scale);
 swl_transform(s,"root.player",(SWLMatrix){c,sn,-sn,c,(float)m->x,cy+(float)m->y});
 const char *label=m->wipe>0?(m->sharkHit?"sharkattacked":"wipeout"):m->air?(fabs(m->spin)+fabs(m->flip)<30?"wavejump":"normalforward"):m->ride<-90?"normalback":"normalforward";
 set_label(w,label);
 bool shark=m->mode==WAVES_SURVIVAL&&m->sharkX>-260;
 show(w,"root.shark",shark);
 if(shark)swl_transform(s,"root.shark",(SWLMatrix){.5f,0,0,.5f,(float)m->sharkX,cy+(float)(m->y<60?60:m->y)+34});
 if(w->screen==SCREEN_GAME){
  text(w,"root.hud.displayscore.stylePoints_tf",(unsigned)m->score);
  swl_target(s,"root.hud.livesRemaining",m->lives+1,NULL,false);
 }
}
static void scenery(Waves *w,bool game){
 static const char *const art[]={"root.bg","root.water","root.surface2","root.surface3","root.surface4","root.curl","root.player","root.wave"};
 for(unsigned i=0;i<sizeof art/sizeof *art;i++)show(w,art[i],game);
 if(!game)show(w,"root.shark",false);
}
static void layout(Waves *w){
 show(w,"root.sign",w->screen==SCREEN_MENU);
 show(w,"root.hud",w->screen==SCREEN_GAME);
 show(w,"root.ends",w->screen==SCREEN_RESULT);
 scenery(w,w->screen!=SCREEN_RESULT);
 show(w,"root.menu",w->screen==SCREEN_MENU);
 show(w,"root.padwater",w->screen!=SCREEN_MENU);
 for(unsigned i=0;i<4;i++)show(w,ARROWS[i],w->screen==SCREEN_GAME);
 game_port_snap(w->port);
}

static void stop_audio(Waves *w){game_port_stop_sounds(w->port);}
static void start(Waves *w,WavesMode mode){
 stop_audio(w);waves_model_init(&w->model,mode);w->screen=SCREEN_GAME;w->started=true;
 w->left=w->right=w->up=w->down=w->jumpTouch=w->jumpHeld=false;w->pad=w->vpad=0;
 swl_target(w->scene,"root.hud.distancebar",1,NULL,false);
 game_port_sound(w->port,"backgroundLoop",55,true);layout(w);sync_art(w);
}
static void menu(Waves *w){
 stop_audio(w);w->screen=SCREEN_MENU;waves_model_init(&w->demo,WAVES_FREESTYLE);w->demoTicks=0;layout(w);sync_art(w);
}
/** The original end screens: endnormal, endsurvivalgood/bad, endcompetitionthird/second/first. */
static void results(Waves *w){
 const WavesModel *m=&w->model;unsigned coins=waves_model_coins(m),frame;
 if(m->mode==WAVES_SURVIVAL)frame=m->success?2:3;
 else if(m->mode==WAVES_COMPETITION)frame=m->trickScore>=1000?6:m->trickScore>=500?5:4;
 else frame=1;
 w->endFrame=frame;w->screen=SCREEN_RESULT;
 swl_target(w->scene,"root.ends",frame,NULL,false);
 /* The player's coin total is not known here: TOTAL COINS stays hidden with the bonus rows. */
 static const char *const extra[]={"ends.labelPrize","ends.prizeBonus_tf","ends.labelDistance","ends.cutscenepuffle","ends.survivalDisplay","ends.labelTotalCoins","ends.coinsTotal_tf"};
 for(unsigned i=0;i<sizeof extra/sizeof *extra;i++){char path[48];snprintf(path,sizeof path,"root.%s",extra[i]);show(w,path,false);}
 text(w,frame==2||frame==3?"root.ends.survivalScore_tf":"root.ends.stylePoints_tf",(unsigned)m->score);
 text(w,"root.ends.coinsEarned_tf",coins);
 layout(w);
}

static void *create(GamePort *port){Waves *w=calloc(1,sizeof *w);if(!w)return NULL;w->port=port;waves_model_init(&w->model,WAVES_FREESTYLE);waves_model_init(&w->demo,WAVES_FREESTYLE);w->selected=1;w->bobDir=-1;return w;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Waves *w=game;w->bank=game_port_bank(w->port,0);w->scene=game_bank_scene(w->bank);if(!w->scene)return false;
 SWLScene *s=w->scene;
 swl_target(s,"root.water",1,NULL,false);
 for(unsigned i=0;i<3;i++){swl_target(s,SURFACES[i],1,NULL,false);w->surfX[i]=-400;}
 swl_target(s,"root.shark",1,"normal",false);
 /* screeninterface: SCORE + BOARDS only; the pointer helper, tips and judge panel need a mouse. */
 static const char *const off[]={"root.hud.gouparrow","root.hud.distancebar","root.hud.speedhelper","root.hud.tips","root.hud.judgescores"};
 for(unsigned i=0;i<sizeof off/sizeof *off;i++)swl_target(s,off[i],1,NULL,false);
 swl_target(s,"root.hud.displayscore",1,NULL,false);
 menu(w);return true;
}

/* Attract ride behind the title: climb, launch, one full spin, drop, repeat. */
static void demo_tick(Waves *w){
 WavesModel *m=&w->demo;w->demoTicks++;
 if(m->phase!=WAVES_PLAYING||m->lives<2||m->wave>1){waves_model_init(m,WAVES_FREESTYLE);}
 bool up=false,down=false,right=false;
 if(m->air)right=m->spin<350;
 else{unsigned t=w->demoTicks%120;up=t<60;down=t>=60&&t<100;if(m->curl>m->x-220)right=true,down=true,up=false;}
 m->time=0;waves_model_update(m,1000.0/24.0,up,down,false,right);m->soundCount=0;
}
static void scroll(Waves *w,const WavesModel *m){
 static const double rate[3]={1.5,2,3};
 for(unsigned i=0;i<3;i++){w->surfX[i]-=m->speed/24.0*rate[i]*.12;if(w->surfX[i]<-800)w->surfX[i]+=800;else if(w->surfX[i]>0)w->surfX[i]-=800;}
 /* wavesrising(): the foam lines bob 10 px. */
 w->bob+=w->bobDir*.5;if(w->bob<-10){w->bob=-10;w->bobDir=1;}else if(w->bob>0){w->bob=0;w->bobDir=-1;}
}
static void tick(void *game){
 Waves *w=game;if(!w->scene)return;swl_tick_deferred(w->scene);
 if(w->screen==SCREEN_MENU){demo_tick(w);scroll(w,&w->demo);sync_art(w);return;}
 if(w->screen!=SCREEN_GAME||game_port_session(w->port)!=GAME_SESSION_PLAYING)return;
 WavesModel *m=&w->model;waves_model_update(m,1000.0/24.0,w->up,w->down,w->left,w->right);
 if(WAVES_REVIEW==2&&m->time>3000&&m->phase==WAVES_PLAYING){m->score+=640;m->trickScore=230;m->maxCombo=2;waves_model_finish(m,true);}
 for(unsigned i=0;i<m->soundCount;i++){const char *name=sound_name(m->sounds[i]);if(name)game_port_sound(w->port,name,70,false);}
 m->soundCount=0;
 scroll(w,m);sync_art(w);
 static const SWLColor normal={{1,1,1,1},{0,0,0,0}},lit={{1,1,1,1},{70,70,20,0}};
 bool held[4]={w->up,w->right,w->down,w->left};
 for(unsigned i=0;i<4;i++)swl_color(w->scene,ARROWS[i],held[i]?lit:normal);
 if(m->phase==WAVES_RESULT){stop_audio(w);results(w);game_port_sound(w->port,m->success?"dchord2":"splash4",70,false);}
}

static void frame(void *game,float seconds,const GameInput *input){
 Waves *w=game;(void)seconds;unsigned clicked=game_port_clicked(w->port);
 if(!w->scene)return;
 if(w->screen==SCREEN_MENU){
  if(clicked==BUTTON_EXIT){game_port_leave(w->port);return;}
  if(clicked>=BUTTON_MODE_BASE&&clicked<BUTTON_MODE_BASE+4){w->selected=clicked-BUTTON_MODE_BASE;start(w,PANEL_MODES[w->selected]);return;}
  if(input->down&GAME_KEY_LEFT&&w->selected>0)w->selected--;
  if(input->down&GAME_KEY_RIGHT&&w->selected<3)w->selected++;
  if(clicked==BUTTON_START||(input->down&GAME_KEY_A))start(w,PANEL_MODES[w->selected]);
  return;
 }
 if(w->screen==SCREEN_RESULT){
  if(clicked==BUTTON_AGAIN||(input->down&GAME_KEY_A)){menu(w);return;}
  if(clicked==BUTTON_PAY||(input->down&GAME_KEY_B)){stop_audio(w);game_port_finish(w->port,waves_model_coins(&w->model));}
  return;
 }
 if(clicked==BUTTON_EXIT){stop_audio(w);game_port_finish(w->port,waves_model_coins(&w->model));return;}
 w->left=(input->held&GAME_KEY_LEFT)||input->stickX<-.5f||w->pad<0;
 w->right=(input->held&GAME_KEY_RIGHT)||input->stickX>.5f||w->pad>0;
 w->up=(input->held&GAME_KEY_UP)||input->stickY>.5f||w->vpad<0;
 w->down=(input->held&GAME_KEY_DOWN)||input->stickY<-.5f||w->vpad>0;
 bool jump=(input->down&(GAME_KEY_A|GAME_KEY_B))||w->jumpTouch;w->jumpTouch=false;
 if(jump)waves_model_jump(&w->model);
}
/** Menu panel under the stylus: the four original panels split the width. */
static int panel_at(float x,float y){
 if(y<HEADER_H+12||y>=MENU_BOTTOM)return -1;
 return x<PA_1?0:x<PA_2?1:x<PA_3?2:3;
}
static void touch(void *game,GameTouch phase,float x,float y){
 Waves *w=game;
 if(w->screen==SCREEN_MENU){
  if(WAVES_REVIEW==1)return;
  int panel=panel_at(x,y);if(WAVES_REVIEW==3&&panel>=0)panel=3;if(panel>=0&&phase==GAME_TOUCH_UP){w->selected=(unsigned)panel;start(w,PANEL_MODES[panel]);}
  else if(panel>=0)w->selected=(unsigned)panel;
  return;
 }
 if(w->screen!=SCREEN_GAME)return;
 w->pad=w->vpad=0;w->jumpHeld=false;
 if(phase==GAME_TOUCH_UP||y<HEADER_H+STRIP_H)return;
 if(x>=JUMP_X-6&&y>=JUMP_Y-10){w->jumpHeld=true;if(phase==GAME_TOUCH_DOWN)w->jumpTouch=true;return;}
 float dx=x-PAD_CX,dy=y-PAD_CY;if(x>=JUMP_X-6||dx*dx+dy*dy>95*95)return;
 w->pad=dx<-14?-1:dx>14?1:0;w->vpad=dy<-14?-1:dy>14?1:0;
}
static void close_game(void *game){Waves *w=game;stop_audio(w);if(w->screen==SCREEN_MENU)game_port_leave(w->port);else game_port_finish(w->port,waves_model_coins(&w->model));}

static void header(GamePort *p,const char *title,bool exit){
 game_port_rect(p,0,0,320,HEADER_H,0x0a3d66);game_port_rect(p,0,HEADER_H-1,320,1,0x4fc0ff);
 game_port_text(p,8,7,11,0xffffff,GAME_ALIGN_LEFT,title);
 if(exit)game_port_button(p,BUTTON_EXIT,256,3,60,20,"Exit",true);
}
static void frame_rect(GamePort *p,float x,float y,float w,float h,float t,uint32_t rgb){
 game_port_rect(p,x,y,w,t,rgb);game_port_rect(p,x,y+h-t,w,t,rgb);game_port_rect(p,x,y,t,h,rgb);game_port_rect(p,x+w-t,y,t,h,rgb);
}
static void draw_menu(Waves *w){
 GamePort *p=w->port;bool online=game_port_session(p)==GAME_SESSION_PLAYING;
 header(p,"Catchin' Waves - choose a mode",true);
 static const float px[4]={3,81,159,237},pw[4]={77,77,77,81};
 static const char *const short_names[4]={"Lesson","Freestyle","Competition","Survival"};
 for(unsigned i=0;i<4;i++){
  game_port_rect(p,px[i]+4,HEADER_H+15,pw[i]-8,16,0x0a3d66);
  game_port_text(p,px[i]+pw[i]/2,HEADER_H+18,9,0xffe27a,GAME_ALIGN_CENTER,short_names[i]);
  if(online)game_port_hotspot(p,BUTTON_MODE_BASE+i,px[i],HEADER_H+12,pw[i],MENU_BOTTOM-HEADER_H-12,true);
 }
 frame_rect(p,px[w->selected],HEADER_H+12,pw[w->selected],MENU_BOTTOM-HEADER_H-12,3,0xffd23f);
 game_port_rect(p,0,MENU_BOTTOM,320,240-MENU_BOTTOM,0x062646);game_port_rect(p,0,MENU_BOTTOM,320,1,0x4fc0ff);
 WavesMode mode=PANEL_MODES[w->selected];
 game_port_text(p,10,MENU_BOTTOM+6,12,0xffe27a,GAME_ALIGN_LEFT,MODE_NAMES[mode]);
 game_port_text(p,10,MENU_BOTTOM+23,10,0xffffff,GAME_ALIGN_LEFT,MODE_HELP[mode][0]);
 game_port_text(p,10,MENU_BOTTOM+37,10,0xffffff,GAME_ALIGN_LEFT,MODE_HELP[mode][1]);
 game_port_text(p,10,MENU_BOTTOM+58,9,0x9fd4ff,GAME_ALIGN_LEFT,"Tap a panel, or D-Pad + A");
 game_port_button(p,BUTTON_START,196,MENU_BOTTOM+48,116,28,"Play (A)",online);
}
static void draw_game(Waves *w){
 GamePort *p=w->port;const WavesModel *m=&w->model;char line[96];
 snprintf(line,sizeof line,"%s  -  Wave %u of 3",MODE_NAMES[m->mode],m->wave);
 header(p,line,true);
 /* Status strip: wave progress and the mode's goal (score and boards are on the upper HUD). */
 float y=HEADER_H;game_port_rect(p,0,y,320,STRIP_H,0x062646);game_port_rect(p,0,y+STRIP_H-1,320,1,0x1d5f93);
 game_port_rect(p,8,y+8,104,8,0x0b3d6e);game_port_rect(p,8,y+8,(float)(104*waves_model_progress(m)),8,0x39d353);frame_rect(p,8,y+8,104,8,1,0xffffff);
 if(m->mode==WAVES_COMPETITION)snprintf(line,sizeof line,"Tricks %u / 1000",(unsigned)m->trickScore);
 else if(m->mode==WAVES_SURVIVAL)snprintf(line,sizeof line,"Distance %u m",(unsigned)(m->distance/10));
 else if(m->mode==WAVES_LESSON)snprintf(line,sizeof line,"Land one spin to pass");
 else snprintf(line,sizeof line,"Tricks %u  Combo x%u",(unsigned)m->trickScore,m->combo);
 game_port_text(p,120,y+6,10,0xffffff,GAME_ALIGN_LEFT,line);
 const char *alert=m->wipe>0?(m->sharkHit?"Shark attack!":"Wipeout!"):m->trickShow>0?NULL:m->curl>m->x-200&&!m->air?"The wave is catching you!":m->mode==WAVES_SURVIVAL&&m->sharkX>m->x-260?"Shark behind you!":NULL;
 if(m->trickShow>0&&m->wipe<=0){snprintf(line,sizeof line,"+%u",m->lastTrick);alert=line;}
 if(alert){game_port_rect(p,JUMP_X-2,y+STRIP_H+6,JUMP_W+4,20,m->trickShow>0&&m->wipe<=0?0x1f7a2e:0x8a1d12);game_port_text(p,JUMP_X+JUMP_W/2,y+STRIP_H+10,10,0xffffff,GAME_ALIGN_CENTER,alert);}
 /* Arrow captions follow what the arrows do now: ride the face, or spin in the air. */
 if(m->air){
  game_port_text(p,PAD_CX-PAD_STEP,PAD_CY+24,10,0xffe27a,GAME_ALIGN_CENTER,"Spin");
  game_port_text(p,PAD_CX+PAD_STEP,PAD_CY+24,10,0xffe27a,GAME_ALIGN_CENTER,"Spin");
 }else{
  game_port_text(p,PAD_CX+24,PAD_CY-PAD_STEP-8,10,0xffffff,GAME_ALIGN_LEFT,"Climb");
  game_port_text(p,PAD_CX+24,PAD_CY+PAD_STEP-2,10,0xffffff,GAME_ALIGN_LEFT,"Drop");
  game_port_text(p,PAD_CX-PAD_STEP,PAD_CY+24,10,0xffffff,GAME_ALIGN_CENTER,"Slow");
  game_port_text(p,PAD_CX+PAD_STEP,PAD_CY+24,10,0xffffff,GAME_ALIGN_CENTER,"Pump");
 }
 game_port_pad(p,JUMP_X,JUMP_Y,JUMP_W,JUMP_H,"JUMP",w->jumpHeld||m->air);
 game_port_text(p,JUMP_X+JUMP_W/2,JUMP_Y+JUMP_H+6,9,0xffffff,GAME_ALIGN_CENTER,"A / B  -  Circle Pad");
 game_port_text(p,JUMP_X+JUMP_W/2,JUMP_Y+JUMP_H+19,9,0xffffff,GAME_ALIGN_CENTER,"or D-Pad steers");
}
static void draw_result(Waves *w){
 GamePort *p=w->port;const WavesModel *m=&w->model;char line[96];
 const char *title=m->mode==WAVES_COMPETITION?(w->endFrame==6?"First place!":w->endFrame==5?"Second place":"Third place"):m->success?"Nice run!":"Wiped out!";
 snprintf(line,sizeof line,"%s - %s",MODE_NAMES[m->mode],title);header(p,line,false);
 game_port_rect(p,24,HEADER_H+12,272,108,0x062646);frame_rect(p,24,HEADER_H+12,272,108,2,0x4fc0ff);
 snprintf(line,sizeof line,"Score %u",(unsigned)m->score);game_port_text(p,160,HEADER_H+20,12,0xffffff,GAME_ALIGN_CENTER,line);
 snprintf(line,sizeof line,"Trick points %u   Best combo x%u",(unsigned)m->trickScore,m->maxCombo);game_port_text(p,160,HEADER_H+38,10,0xd6ecff,GAME_ALIGN_CENTER,line);
 snprintf(line,sizeof line,"%u coins",waves_model_coins(m));game_port_text(p,160,HEADER_H+58,18,0xffd23f,GAME_ALIGN_CENTER,line);
 if(!game_port_descriptor(p)->online)game_port_text(p,160,HEADER_H+90,10,0xd6ecff,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
 bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
 game_port_button(p,BUTTON_AGAIN,24,172,128,44,"Play again (A)",!waiting);
 game_port_button(p,BUTTON_PAY,168,172,128,44,waiting?"Paying...":"Get coins (B)",!waiting);
}
static void draw_bottom(void *game){
 Waves *w=game;
 if(w->screen==SCREEN_MENU)draw_menu(w);else if(w->screen==SCREEN_RESULT)draw_result(w);else draw_game(w);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
const GameController GAME_WAVES_CONTROLLER={
 .key="waves",.kind=GAME_SOLO,.room=WAVES_ROOM,.rate=24,
 .banks={"games/waves.gpk"},.order={0,1,2},.nightPanel=true,.backgroundRGB=0x0d74bd,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.lower_view=lower_view,.lower_frame=lower_frame,
};
