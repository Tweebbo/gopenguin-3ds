#include "game_controller.h"
#include "game_dance_model.h"
#include "game_dance.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DANCE_ROOM 952
enum {SCREEN_MENU,SCREEN_GAME,SCREEN_RESULT};
enum {BUTTON_START=1,BUTTON_EXIT,BUTTON_AGAIN,BUTTON_DIFF_EASY=30,BUTTON_DIFF_MEDIUM=31,BUTTON_DIFF_HARD=32,BUTTON_DIFF_EXPERT=33};
typedef struct {GamePort *port;GameBank *bank;SWLScene *scene;DanceModel model;unsigned screen,song,difficulty;double position;int touchDir;uint32_t held;
 unsigned flash[4],popupStart,popupAge,shownScore,shownCombo,shownMood,lastCounts[6];int pendingGrade;} Dance;
/* Touch-screen lane: root frame 2 of the bank (backdrop, receptors, notes, score clip) at LANE_S. */
#define LANE_S .75f
#define LANE_X (4-22*LANE_S)
#define LANE_Y (60-133*LANE_S)
#define HEADER 22
#define SX(x) (((x)-LANE_X)/LANE_S)
#define SY(y) (((y)-LANE_Y)/LANE_S)
DanceModel *game_dance_model(void *game){return &((Dance *)game)->model;}
static const double TARGET[4][2]={{57.5,132},{192.45,134.5},{125,134.5},{261,132}};
static const char *DIRS[]={"left","down","up","right"};
static void sync_notes(Dance *d);
static void hide_notes(Dance *d){char path[32];const char *parts[]={"Note","Stem","Tail"};for(unsigned dir=0;dir<4;dir++)for(unsigned i=0;i<8;i++)for(unsigned part=0;part<3;part++){snprintf(path,sizeof path,"root.%s%s%u",DIRS[dir],parts[part],i);swl_visible(d->scene,path,false);}}
/* The receptors and notes are one set of instances shared by both root frames. The upper screen is the
   club to watch (crowd, speakers, lights), so they are hidden for the upper pass and shown again for the
   touch-screen lane. Upper is drawn first each frame (main.c), then the lower screen. */
static bool top_view(void *game,SWLMatrix *view){
 Dance *d=game;(void)view;
 if(d->scene){swl_visible(d->scene,"root.arrows",false);hide_notes(d);}
 return false;
}
static bool lower_view(void *game,SWLMatrix *view){
 Dance *d=game;
 /* Menu and results use the same club wall (root frame 2) without the lane, never a copy of the upper screen. */
 bool lane=d->screen==SCREEN_GAME;
 if(d->scene){swl_visible(d->scene,"root.arrows",lane);swl_visible(d->scene,"root.score",lane);if(lane)sync_notes(d);else hide_notes(d);}
 *view=(SWLMatrix){LANE_S,0,0,LANE_S,LANE_X,LANE_Y};
 return true;
}
static unsigned lower_frame(void *game,unsigned bank){(void)game;return bank?0:2;}
static void sync_notes(Dance *d){
 hide_notes(d);const DanceChartData *c=dance_model_chart(&d->model);unsigned used[4]={0};
 for(unsigned i=0;i<c->count;i++){
  bool holding=d->model.noteState[i]>0&&d->model.noteState[i]!=5&&c->notes[i].hold>0&&!d->model.released[i];
  if(d->model.noteState[i]!=0&&!holding)continue;
  double remaining=c->notes[i].time-(holding?d->model.pressed[i]:d->position);
  if(remaining>c->approach||remaining<-(c->approach*119.45/344+c->notes[i].hold))continue;
  unsigned dir=c->notes[i].dir;
  if(used[dir]>=7)continue;
  unsigned slot=used[dir]++;
  double x=TARGET[dir][0],y=133.95+344*remaining/c->approach;
  char path[32];snprintf(path,sizeof path,"root.%sNote%u",DIRS[dir],slot);
  swl_target(d->scene,path,1,NULL,false);swl_transform(d->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});swl_visible(d->scene,path,true);
  double hold=holding?fmax(0,d->model.pressed[i]+c->notes[i].hold-d->position):c->notes[i].hold;
  float height=(float)(344*hold/c->approach);
  if(height>0){
   snprintf(path,sizeof path,"root.%sStem%u",DIRS[dir],slot);swl_transform(d->scene,path,(SWLMatrix){1,0,0,height,(float)x,(float)y});swl_visible(d->scene,path,true);
   snprintf(path,sizeof path,"root.%sTail%u",DIRS[dir],slot);swl_transform(d->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y+height});swl_visible(d->scene,path,true);
  }
 }
 /* Receptor flash after a press: the original note art on its target (slot 7, the PC port's notePress flash). */
 for(unsigned dir=0;dir<4;dir++)if(d->flash[dir]){
  char path[32];snprintf(path,sizeof path,"root.%sNote7",DIRS[dir]);
  swl_target(d->scene,path,1,NULL,false);swl_transform(d->scene,path,(SWLMatrix){1,0,0,1,(float)TARGET[dir][0],133.95f});swl_visible(d->scene,path,true);
 }
}
/* Original score clip on the touch screen, moved into the right-hand column. */
static void place(Dance *d,const char *path,float sx,float sy,float scale){swl_transform(d->scene,path,(SWLMatrix){scale,0,0,scale,SX(sx),SY(sy)});}
static void layout_score(Dance *d){
 SWLScene *s=d->scene;
 swl_transform(s,"root.score",(SWLMatrix){1,0,0,1,0,0});
 place(d,"root.score.score",216,HEADER+6,1);
 place(d,"root.score.scoreValue",216,HEADER+18,1);
 place(d,"root.score.streakLabel",316-164*LANE_S,HEADER+58,1);
 place(d,"root.score.streakValue",222,HEADER+78,1);
 swl_transform(s,"root.score.multiplier",(SWLMatrix){.65f,0,0,.49f,SX(232),SY(HEADER+132)});
 swl_visible(s,"root.wall.wallposter",false);
 swl_visible(s,"root.score.multiplayerNames",false);swl_visible(s,"root.score.multiplayerLabel",false);
 swl_visible(s,"root.score.combo",false);swl_visible(s,"root.score.streakPopup",false);
 swl_target(s,"root.score.notePopup",1,NULL,false);
 for(unsigned i=0;i<4;i++){char path[40];snprintf(path,sizeof path,"root.score.noteHoldPopup%u",i);swl_visible(s,path,false);}
 d->shownScore=d->shownCombo=~0u;
}
static const unsigned POPUP[6]={2,17,32,47,62,77};
static void sync_score(Dance *d){
 DanceModel *m=&d->model;char value[16];
 if(d->shownScore!=m->score){d->shownScore=m->score;snprintf(value,sizeof value,"%u",m->score);game_bank_text(d->bank,"root.score.scoreValue",value);}
 if(d->shownCombo!=m->combo){d->shownCombo=m->combo;snprintf(value,sizeof value,"%u",m->combo);game_bank_text(d->bank,"root.score.streakValue",value);
  unsigned step=m->combo/10;swl_target(d->scene,"root.score.multiplier",(step>7?7:step)+1,NULL,false);}
 /* Judgement popups: the score clip lives only in the lower frame, so its frames are stepped here. */
 for(unsigned g=0;g<6;g++)if(m->counts[g]!=d->lastCounts[g]){unsigned grade=g==4&&d->pendingGrade!=4?5:g;d->popupStart=POPUP[grade];d->popupAge=0;d->lastCounts[g]=m->counts[g];}
 d->pendingGrade=-1;
 if(d->popupStart){unsigned frame=d->popupStart+d->popupAge;if(d->popupAge>=14){d->popupStart=0;frame=1;}swl_target(d->scene,"root.score.notePopup",frame,NULL,false);}
 /* Speakers and lights follow the crowd rating, as in the original club. */
 unsigned mood=m->rating<10?2:m->rating<20?3:4;
 if(mood!=d->shownMood){d->shownMood=mood;static const char *labels[]={"inactive","danceBad","danceOK","danceGreat"};const char *label=labels[mood-1];
  swl_goto(d->scene,"root.background.speakerLeft",0,label,true);swl_goto(d->scene,"root.background.speakerRight",0,label,true);swl_goto(d->scene,"root.background.lights",0,label,true);}
}

static void start_song(Dance *d,unsigned song,unsigned difficulty){
 d->song=song;d->difficulty=difficulty;d->position=0;d->touchDir=-1;d->held=0;memset(d->flash,0,sizeof d->flash);memset(d->lastCounts,0,sizeof d->lastCounts);d->popupStart=0;d->pendingGrade=-1;d->shownMood=0;
 game_port_stop_sounds(d->port);
 char tune[8];snprintf(tune,sizeof tune,"song%u",song+1);game_port_sound(d->port,tune,80,false);dance_model_start_seed(&d->model,song,difficulty,(uint32_t)(game_port_clock(d->port)*1000)+1);d->position=-dance_model_audio_offset(&d->model);d->screen=SCREEN_GAME;layout_score(d);sync_notes(d);sync_score(d);game_port_snap(d->port);
}
static void *create(GamePort *port){Dance *d=calloc(1,sizeof *d);if(!d)return NULL;d->port=port;d->song=0;d->difficulty=1;d->touchDir=-1;dance_model_init(&d->model);return d;}
static void destroy(void *game){free(game);}
static bool ready(void *game){Dance *d=game;d->bank=game_port_bank(d->port,0);d->scene=game_bank_scene(d->bank);if(!d->scene)return false;hide_notes(d);swl_visible(d->scene,"root.background",true);swl_visible(d->scene,"root.arrows",true);d->screen=SCREEN_MENU;return true;}
static void tick(void *game){
 Dance *d=game;if(!d->scene)return;swl_tick_deferred(d->scene);
 if(d->screen!=SCREEN_GAME||game_port_session(d->port)!=GAME_SESSION_PLAYING)return;
 d->position+=1000.0/24.0;dance_model_update(&d->model,d->position);
 for(unsigned dir=0;dir<4;dir++)if(d->flash[dir])d->flash[dir]--;
 sync_notes(d);sync_score(d);
 if(d->model.phase==DANCE_RESULT){d->screen=SCREEN_RESULT;game_port_stop_sounds(d->port);}
}
static void frame(void *game,float seconds,const GameInput *input){
 Dance *d=game;(void)seconds;unsigned clicked=game_port_clicked(d->port);
 if(d->screen==SCREEN_MENU){
  if(clicked==BUTTON_EXIT){game_port_leave(d->port);return;}
  if(clicked>=20&&clicked<26)d->song=clicked-20;
  if(clicked>=BUTTON_DIFF_EASY&&clicked<=BUTTON_DIFF_EXPERT)d->difficulty=clicked-BUTTON_DIFF_EASY;
  /* D-pad: up/down pick the song, left/right the difficulty. */
  if(input->down&GAME_KEY_UP)d->song=(d->song+5)%6;
  if(input->down&GAME_KEY_DOWN)d->song=(d->song+1)%6;
  if(input->down&GAME_KEY_LEFT)d->difficulty=(d->difficulty+3)%4;
  if(input->down&GAME_KEY_RIGHT)d->difficulty=(d->difficulty+1)%4;
  if(clicked==BUTTON_START||(input->down&GAME_KEY_A))start_song(d,d->song,d->difficulty);
  return;
 }
 if(d->screen==SCREEN_RESULT){if(clicked==BUTTON_AGAIN||(input->down&GAME_KEY_A)){game_port_stop_sounds(d->port);d->screen=SCREEN_MENU;return;}if(clicked==BUTTON_EXIT){game_port_stop_sounds(d->port);game_port_finish(d->port,dance_model_coins(&d->model));}return;}
 if(clicked==BUTTON_EXIT){game_port_finish(d->port,dance_model_coins(&d->model));return;}
 static const uint32_t keys[]={GAME_KEY_LEFT,GAME_KEY_DOWN,GAME_KEY_UP,GAME_KEY_RIGHT};
 d->held=input->held|input->down;
 for(unsigned dir=0;dir<4;dir++){
  if((input->down&keys[dir])&&d->touchDir!=(int)dir){d->pendingGrade=dance_model_hit(&d->model,dir);d->flash[dir]=3;}
  if(!(d->held&keys[dir])&&d->touchDir!=(int)dir)dance_model_release(&d->model,dir);
 }
 sync_notes(d);
}
static void touch(void *game,GameTouch phase,float x,float y){
 Dance *d=game;if(d->screen!=SCREEN_GAME)return;
 int dir=-1;
 if(phase!=GAME_TOUCH_UP&&y>=HEADER&&x<210){
  float stageX=SX(x);
  /* Model ids are left/down/up/right; the original art places up before down. */
  float distance=40;
  for(unsigned i=0;i<4;i++){float dx=fabsf(stageX-(float)TARGET[i][0]);if(dx<distance){distance=dx;dir=(int)i;}}
 }
 if(dir==d->touchDir)return;
 static const uint32_t keys[]={GAME_KEY_LEFT,GAME_KEY_DOWN,GAME_KEY_UP,GAME_KEY_RIGHT};
 if(d->touchDir>=0&&!(d->held&keys[d->touchDir]))dance_model_release(&d->model,(unsigned)d->touchDir);
 d->touchDir=dir;if(dir>=0&&!(d->held&keys[dir])){d->pendingGrade=dance_model_hit(&d->model,(unsigned)dir);d->flash[dir]=3;}sync_notes(d);
}
static void close_game(void *game){Dance *d=game;game_port_stop_sounds(d->port);if(d->screen==SCREEN_MENU)game_port_leave(d->port);else game_port_finish(d->port,dance_model_coins(&d->model));}
/* Drawn chrome in the ui_theme palette: header band, a lit (selected) choice. */
static void header(GamePort *p,const char *title){game_port_rect(p,0,0,320,HEADER,0x102040);game_port_rect(p,0,HEADER-1,320,1,0x4fc0ff);game_port_text(p,6,5,10,0xffffff,GAME_ALIGN_LEFT,title);game_port_button(p,BUTTON_EXIT,266,1,50,19,"Exit",true);}
static void choice(GamePort *p,unsigned id,float x,float y,float w,float h,const char *label,bool lit){
 if(!lit){game_port_button(p,id,x,y,w,h,label,true);return;}
 game_port_hotspot(p,id,x,y,w,h,true);
 game_port_rect(p,x+1,y+2,w,h,0x06203a);game_port_rect(p,x,y,w,h,0xffffff);game_port_rect(p,x+2,y+2,w-4,h-4,0xffd23f);game_port_rect(p,x+2,y+2,w-4,(h-4)/2,0xffe680);
 game_port_text(p,x+w/2,y+h/2-6,11,0x102040,GAME_ALIGN_CENTER,label);
}
static void draw_bottom(void *game){
 Dance *d=game;GamePort *p=d->port;char line[80];
 static const char *names[]={"Easy","Medium","Hard","Expert"};
 if(d->screen==SCREEN_MENU){
  header(p,"Dance Contest");
  game_port_rect(p,6,HEADER+4,104,14,0x102040);game_port_text(p,10,HEADER+5,9,0xffe08a,GAME_ALIGN_LEFT,"Choose a song");
  for(unsigned i=0;i<6;i++)choice(p,20+i,6+(i%2)*156,HEADER+20+(i/2)*30,152,26,DANCE_SONGS_DATA[i].name,i==d->song);
  game_port_rect(p,6,HEADER+112,80,14,0x102040);game_port_text(p,10,HEADER+113,9,0xffe08a,GAME_ALIGN_LEFT,"Difficulty");
  for(unsigned i=0;i<4;i++)choice(p,BUTTON_DIFF_EASY+i,6+78*i,HEADER+128,74,26,names[i],i==d->difficulty);
  game_port_button(p,BUTTON_START,100,HEADER+162,120,32,"Dance! (A)",true);
  game_port_rect(p,0,226,320,14,0x102040);game_port_text(p,160,228,9,0xd6ecff,GAME_ALIGN_CENTER,"D-pad: song and difficulty    A: start");
  return;
 }
 if(d->screen==SCREEN_RESULT){
  header(p,"Dance Contest - Results");
  game_port_rect(p,30,36,260,76,0x102040);game_port_rect(p,30,36,260,1,0x4fc0ff);
  snprintf(line,sizeof line,"Rating: %s",dance_model_grade(&d->model));game_port_text(p,160,42,16,0xffd23f,GAME_ALIGN_CENTER,line);
  snprintf(line,sizeof line,"Score %u   Best combo %u",d->model.score,d->model.maxCombo);game_port_text(p,160,68,10,0xffffff,GAME_ALIGN_CENTER,line);
  snprintf(line,sizeof line,"You won %u coins!",dance_model_coins(&d->model));game_port_text(p,160,88,11,0x9fe870,GAME_ALIGN_CENTER,line);
  game_port_button(p,BUTTON_AGAIN,50,128,100,36,"Play again",true);game_port_button(p,BUTTON_EXIT,170,128,100,36,"Get coins",true);
  game_port_rect(p,0,226,320,14,0x102040);game_port_text(p,160,228,9,0xd6ecff,GAME_ALIGN_CENTER,"A: play again");
  return;
 }
 /* Thin header; score and combo are the original clip in the right-hand column. */
 snprintf(line,sizeof line,"%s  -  %s",DANCE_SONGS_DATA[d->song].name,names[d->difficulty&3]);header(p,line);
 /* Controls reminder in the free corner of the score column (the lane's left side stays clear for notes). */
 game_port_text(p,266,212,9,0x102040,GAME_ALIGN_CENTER,"Tap a lane");
 game_port_text(p,266,224,9,0x102040,GAME_ALIGN_CENTER,"or use the D-pad");
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
const GameController GAME_DANCE_CONTROLLER={.key="dance",.kind=GAME_SOLO,.room=DANCE_ROOM,.rate=24,.banks={"games/dance.gpk"},.order={0,1,2},.nightPanel=true,.backgroundRGB=0x101020,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.top_view=top_view,.lower_view=lower_view,.lower_frame=lower_frame};
