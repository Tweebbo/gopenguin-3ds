#include "game_controller.h"
#include "game_dance_model.h"
#include "game_dance.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DANCE_ROOM 952
enum {SCREEN_MENU,SCREEN_GAME,SCREEN_RESULT};
enum {BUTTON_START=1,BUTTON_EXIT,BUTTON_AGAIN,BUTTON_DIFF_EASY=30,BUTTON_DIFF_MEDIUM=31,BUTTON_DIFF_HARD=32,BUTTON_LEFT=10,BUTTON_RIGHT=11,BUTTON_UP=12,BUTTON_DOWN=13};
typedef struct {GamePort *port;GameBank *bank;SWLScene *scene;DanceModel model;unsigned screen,song,difficulty;double position;unsigned shownNotes[4][8];} Dance;
DanceModel *game_dance_model(void *game){return &((Dance *)game)->model;}
static const double START_X=153.5,START_Y=477.95;
static const double TARGET[4][2]={{57.5,132},{192.45,134.5},{125,134.5},{261,132}};
static const char *DIRS[]={"left","down","up","right"};

static void hide_notes(Dance *d){char path[32];for(unsigned dir=0;dir<4;dir++)for(unsigned i=0;i<8;i++){snprintf(path,sizeof path,"root.%sNote%u",DIRS[dir],i);swl_visible(d->scene,path,false);}}
static void sync_notes(Dance *d){
 hide_notes(d);const DanceChartData *c=dance_model_chart(&d->model);unsigned used[4]={0};
 for(unsigned i=0;i<c->count;i++){
  if(d->model.noteState[i]!=0)continue;
  double remaining=c->notes[i].time-d->position;
  if(remaining>c->approach||remaining<-200)continue;
  unsigned dir=c->notes[i].dir;
  if(used[dir]>=8)continue;
  unsigned slot=used[dir]++;
  double progress=1-remaining/c->approach;if(progress<0)progress=0;if(progress>1)progress=1;
  double x=START_X+(TARGET[dir][0]-START_X)*progress,y=START_Y+(TARGET[dir][1]-START_Y)*progress;
  char path[32];snprintf(path,sizeof path,"root.%sNote%u",DIRS[dir],slot);
  swl_target(d->scene,path,1,NULL,false);swl_transform(d->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});swl_visible(d->scene,path,true);
 }
}
static void start_song(Dance *d,unsigned song,unsigned difficulty){
 d->song=song;d->difficulty=difficulty;d->position=0;
 game_port_stop_sounds(d->port);
 char tune[8];snprintf(tune,sizeof tune,"song%u",song+1);game_port_sound(d->port,tune,80,false);dance_model_start(&d->model,song,difficulty);d->screen=SCREEN_GAME;sync_notes(d);game_port_snap(d->port);
}
static void *create(GamePort *port){Dance *d=calloc(1,sizeof *d);if(!d)return NULL;d->port=port;d->song=0;d->difficulty=1;dance_model_init(&d->model);return d;}
static void destroy(void *game){free(game);}
static bool ready(void *game){Dance *d=game;d->bank=game_port_bank(d->port,0);d->scene=game_bank_scene(d->bank);if(!d->scene)return false;hide_notes(d);swl_visible(d->scene,"root.background",true);swl_visible(d->scene,"root.arrows",true);d->screen=SCREEN_MENU;return true;}
static void tick(void *game){
 Dance *d=game;if(!d->scene)return;swl_tick_deferred(d->scene);
 if(d->screen!=SCREEN_GAME||game_port_session(d->port)!=GAME_SESSION_PLAYING)return;
 d->position+=1000.0/24.0;dance_model_update(&d->model,d->position);sync_notes(d);
 if(d->model.phase==DANCE_RESULT){d->screen=SCREEN_RESULT;game_port_stop_sounds(d->port);}
}
static void frame(void *game,float seconds,const GameInput *input){
 Dance *d=game;(void)seconds;unsigned clicked=game_port_clicked(d->port);
 if(d->screen==SCREEN_MENU){
  if(clicked==BUTTON_EXIT){game_port_leave(d->port);return;}
  if(clicked>=20&&clicked<26)d->song=clicked-20;
  if(clicked>=BUTTON_DIFF_EASY&&clicked<=BUTTON_DIFF_HARD)d->difficulty=clicked-BUTTON_DIFF_EASY;
  if(clicked==BUTTON_START||(input->down&GAME_KEY_A))start_song(d,d->song,d->difficulty);
  return;
 }
 if(d->screen==SCREEN_RESULT){if(clicked==BUTTON_AGAIN||(input->down&GAME_KEY_A)){game_port_stop_sounds(d->port);d->screen=SCREEN_MENU;return;}if(clicked==BUTTON_EXIT){game_port_stop_sounds(d->port);game_port_finish(d->port,dance_model_coins(&d->model));}return;}
 if(clicked==BUTTON_EXIT){game_port_finish(d->port,dance_model_coins(&d->model));return;}
 unsigned dir=4;if(clicked==BUTTON_LEFT||(input->down&GAME_KEY_LEFT))dir=0;else if(clicked==BUTTON_DOWN||(input->down&GAME_KEY_DOWN))dir=1;else if(clicked==BUTTON_UP||(input->down&GAME_KEY_UP))dir=2;else if(clicked==BUTTON_RIGHT||(input->down&GAME_KEY_RIGHT))dir=3;
 if(dir<4){dance_model_hit(&d->model,dir);sync_notes(d);}
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Dance *d=game;game_port_stop_sounds(d->port);if(d->screen==SCREEN_MENU)game_port_leave(d->port);else game_port_finish(d->port,dance_model_coins(&d->model));}
static void draw_bottom(void *game){
 Dance *d=game;GamePort *p=d->port;char line[80];
 if(d->screen==SCREEN_MENU){
  game_port_rect(p,0,0,320,22,0x102040);game_port_text(p,6,6,10,0xffffff,GAME_ALIGN_LEFT,"Dance Contest");
  for(unsigned i=0;i<6;i++)game_port_button(p,20+i,6+(i%2)*155,30+(i/2)*32,145,28,i==d->song?"selected":"Song",true);
  game_port_button(p,BUTTON_DIFF_EASY,30,132,80,28,d->difficulty==0?"Easy*":"Easy",true);
  game_port_button(p,BUTTON_DIFF_MEDIUM,120,132,80,28,d->difficulty==1?"Medium*":"Medium",true);
  game_port_button(p,BUTTON_DIFF_HARD,210,132,80,28,d->difficulty==2?"Hard*":"Hard",true);
  game_port_button(p,BUTTON_START,90,178,140,36,"Start",true);game_port_button(p,BUTTON_EXIT,260,2,54,20,"Exit",true);return;
 }
 if(d->screen==SCREEN_RESULT){snprintf(line,sizeof line,"Score %u  %s  %u coins",d->model.score,dance_model_grade(&d->model),dance_model_coins(&d->model));game_port_text(p,160,60,12,0xffffff,GAME_ALIGN_CENTER,line);game_port_button(p,BUTTON_AGAIN,50,120,100,36,"Play again",true);game_port_button(p,BUTTON_EXIT,180,120,100,36,"Exit",true);return;}
 game_port_rect(p,0,0,320,26,0x003366);snprintf(line,sizeof line,"%s  Score %u  Combo %u",DANCE_SONGS_DATA[d->song].name,d->model.score,d->model.combo);game_port_text(p,5,8,9,0xffffff,GAME_ALIGN_LEFT,line);game_port_button(p,BUTTON_EXIT,266,2,50,20,"Exit",true);
 game_port_button(p,BUTTON_LEFT,4,188,70,46,"<",true);game_port_button(p,BUTTON_DOWN,80,188,70,46,"v",true);game_port_button(p,BUTTON_UP,156,188,70,46,"^",true);game_port_button(p,BUTTON_RIGHT,232,188,70,46,">",true);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
const GameController GAME_DANCE_CONTROLLER={.key="dance",.kind=GAME_SOLO,.room=DANCE_ROOM,.rate=24,.banks={"games/dance.gpk"},.order={0,1,2},.backgroundRGB=0x101020,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom};
