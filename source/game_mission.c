#include "game_controller.h"
#include "game_mission.h"
#include "game_mission_flow_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {SCREEN_PLAY,SCREEN_DONE};
enum {BUTTON_ACTION=1,BUTTON_PAY=3,BUTTON_EXIT=2,BUTTON_CHOICE_BASE=20,BUTTON_KEY_BASE=40,BUTTON_CLEAR=60};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 const MissionData *data;
 unsigned step,screen,sceneIndex,orderProgress;
 unsigned pattern;
 char code[12],resource[56];
 bool loaded,online;
} Mission;

const MissionData *game_mission_data(void *game){return ((Mission *)game)->data;}
unsigned game_mission_step(void *game){return ((Mission *)game)->step;}
unsigned game_mission_order_progress(void *game){return ((Mission *)game)->orderProgress;}
const char *game_mission_code(void *game){return ((Mission *)game)->code;}
static const MissionStep *current(const Mission *m){return &m->data->steps[m->step];}
static bool solved(const Mission *m){
 const MissionPuzzle *p=&current(m)->puzzle;
 if(p->type==3)return m->orderProgress>=p->count;
 if(p->type==2)return !strcmp(m->code,p->code);
 if(p->type==1)return (m->pattern&((1u<<p->count)-1))==p->target;
 return true;
}
static void reset_puzzle(Mission *m){
 const MissionPuzzle *p=&current(m)->puzzle;
 m->orderProgress=0;m->pattern=0;m->code[0]=0;
 (void)p;
}
void game_mission_press_order(void *game,unsigned index){
 Mission *m=game;const MissionPuzzle *p=&current(m)->puzzle;if(p->type!=3||index>=p->count)return;
 if(index==p->order[m->orderProgress])m->orderProgress++;else m->orderProgress=0;
}
void game_mission_type_code(void *game,char value){
 Mission *m=game;const MissionPuzzle *p=&current(m)->puzzle;if(p->type!=2)return;
 unsigned length=(unsigned)strlen(m->code);if(length>=(unsigned)p->count)return;
 if(value>='a'&&value<='z')value=(char)(value-'a'+'A');
 m->code[length]=value;m->code[length+1]=0;
}
void game_mission_clear_code(void *game){((Mission *)game)->code[0]=0;}
static void request_scene(Mission *m,unsigned scene){
 if(m->loaded&&m->sceneIndex==scene)return;
 if(m->bank)game_port_release_bank(m->port,0);
 m->bank=NULL;m->scene=NULL;m->loaded=false;m->sceneIndex=scene;
 snprintf(m->resource,sizeof m->resource,"%s",m->data->scenePacks[scene]);
 GameResource state=game_port_require_bank(m->port,0);
 if(state==GAME_RESOURCE_READY){m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);m->loaded=m->scene!=NULL;if(m->loaded)game_port_snap(m->port);}
}
static void *create(GamePort *port){
 Mission *m=calloc(1,sizeof *m);if(!m)return NULL;m->port=port;m->online=game_port_descriptor(port)->online;
 const char *key=game_port_descriptor(port)->key;unsigned wanted=(unsigned)atoi(key+7);
 for(unsigned i=0;i<MISSION_FLOW_COUNT;i++)if(MISSION_FLOW_DATA[i]->mission==wanted)m->data=MISSION_FLOW_DATA[i];
 if(!m->data){free(m);return NULL;}
 snprintf(m->resource,sizeof m->resource,"%s",m->data->scenePacks[0]);return m;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){Mission *m=game;request_scene(m,0);return true;}
static void tick(void *game){
 Mission *m=game;
 if(!m->loaded){GameResource state=game_port_require_bank(m->port,0);if(state==GAME_RESOURCE_READY){m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);m->loaded=m->scene!=NULL;if(m->loaded)game_port_snap(m->port);}return;}
 if(m->scene)swl_tick_deferred(m->scene);
}
static void advance(Mission *m){
 if(!solved(m))return;
 char sound[16];snprintf(sound,sizeof sound,"scene%u",current(m)->scene);
 game_port_sound(m->port,sound,45,false);
 if(m->step+1>=m->data->stepCount){m->screen=SCREEN_DONE;return;}
 m->step++;reset_puzzle(m);
 const MissionStep *next=current(m);if(next->scene!=m->sceneIndex)request_scene(m,next->scene);
}
static void frame(void *game,float seconds,const GameInput *input){
 Mission *m=game;(void)seconds;
 if(!m->online){game_port_leave(m->port);return;}
 unsigned clicked=game_port_clicked(m->port);
 if(clicked==BUTTON_EXIT){if(m->screen==SCREEN_DONE)game_port_finish(m->port,m->data->reward);else game_port_leave(m->port);return;}
 if(m->screen==SCREEN_DONE){if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(m->port,m->data->reward);return;}
 const MissionPuzzle *p=&current(m)->puzzle;
 if(clicked>=BUTTON_CHOICE_BASE&&clicked<BUTTON_CHOICE_BASE+8)game_mission_press_order(m,clicked-BUTTON_CHOICE_BASE);
 else if(clicked>=BUTTON_KEY_BASE&&clicked<BUTTON_KEY_BASE+10){
  unsigned index=clicked-BUTTON_KEY_BASE;const char *keys=p->code;
  char seen[12]={0};unsigned n=0;
  for(unsigned i=0;keys[i];i++){bool unique=true;for(unsigned j=0;j<n;j++)if(seen[j]==keys[i])unique=false;if(unique){if(n==index){game_mission_type_code(m,keys[i]);return;}seen[n++]=keys[i];}}
 }else if(clicked==BUTTON_CLEAR)game_mission_clear_code(m);
 else if(clicked==BUTTON_ACTION||(input->down&GAME_KEY_A))advance(m);
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Mission *m=game;if(m->screen==SCREEN_DONE)game_port_finish(m->port,m->data->reward);else game_port_leave(m->port);}
static unsigned wrap(const char *text,char lines[][56],unsigned capacity){
 unsigned count=0,length=0;lines[0][0]=0;
 for(const char *word=text;;){
  while(*word==' ')word++;
  if(!*word)break;
  const char *end=word;while(*end&&*end!=' ')end++;
  unsigned size=(unsigned)(end-word);
  if(length&&length+1+size>=54){count++;if(count>=capacity)return count;length=0;lines[count][0]=0;}
  if(length){lines[count][length++]=' ';lines[count][length]=0;}
  if(size>53)size=53;
  memcpy(lines[count]+length,word,size);length+=size;lines[count][length]=0;
  word=end;
 }
 return count+1;
}
static void draw_bottom(void *game){
 Mission *m=game;GamePort *p=m->port;char line[96],lines[8][56];
 if(m->screen==SCREEN_DONE){
  game_port_text(p,160,46,14,0xffffff,GAME_ALIGN_CENTER,"Mission accomplished!");
  game_port_text(p,160,74,10,0xd6ecff,GAME_ALIGN_CENTER,m->data->title);
  snprintf(line,sizeof line,"You earned %u coins.",m->data->reward/10);
  game_port_text(p,160,100,11,0xffffff,GAME_ALIGN_CENTER,line);
  game_port_button(p,BUTTON_PAY,90,146,140,42,"Get coins",true);
  game_port_button(p,BUTTON_EXIT,260,2,54,20,"Exit",true);
  return;
 }
 const MissionStep *step=current(m);const MissionPuzzle *puzzle=&step->puzzle;
 snprintf(line,sizeof line,"PSA %u: %s",m->data->mission,m->data->title);
 game_port_text(p,6,5,9,0xffffff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"%u/%u",m->step+1,m->data->stepCount);
 game_port_text(p,314,5,8,0xffffff,GAME_ALIGN_RIGHT,line);
 game_port_button(p,BUTTON_EXIT,270,18,44,17,"Exit",true);
 game_port_text(p,6,20,10,0xffe08a,GAME_ALIGN_LEFT,step->title);
 unsigned count=wrap(step->text,lines,7);
 for(unsigned i=0;i<count;i++)game_port_text(p,6,36+i*12,8,0xf2f6ff,GAME_ALIGN_LEFT,lines[i]);
 unsigned y=36+count*12+2;if(y>118)y=118;
 game_port_text(p,6,y,8,0x9fe870,GAME_ALIGN_LEFT,step->objective);
 if(puzzle->type==3){
  for(unsigned i=0;i<puzzle->count&&i<8;i++){
   snprintf(line,sizeof line,"%s",puzzle->labels[i]);
   game_port_button(p,BUTTON_CHOICE_BASE+i,6+(i%3)*104,124+(i/3)*24,100,22,line,i<(unsigned)m->orderProgress||true);
  }
  snprintf(line,sizeof line,"Order %u/%u",m->orderProgress,puzzle->count);
  game_port_text(p,6,198,8,0xffffff,GAME_ALIGN_LEFT,line);
 }else if(puzzle->type==2){
  snprintf(line,sizeof line,"Code: %s",m->code);
  game_port_text(p,6,122,9,0xffffff,GAME_ALIGN_LEFT,line);
  char seen[12]={0};unsigned n=0;
  for(unsigned i=0;puzzle->code[i];i++){bool unique=true;for(unsigned j=0;j<n;j++)if(seen[j]==puzzle->code[i])unique=false;if(unique)seen[n++]=puzzle->code[i];}
  for(unsigned i=0;i<n&&i<10;i++){line[0]=seen[i];line[1]=0;game_port_button(p,BUTTON_KEY_BASE+i,6+i*31,142,28,26,line,true);}
  game_port_button(p,BUTTON_CLEAR,6,174,60,22,"Clear",true);
 }else{
  game_port_text(p,6,124,8,0xffffff,GAME_ALIGN_LEFT,"Follow the objective to continue.");
 }
 game_port_button(p,BUTTON_ACTION,40,198,240,38,step->action,solved(m));
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Mission *m=game;return bank==0?m->resource:NULL;}
#endif
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Mission *m=game;return bank==0&&m->loaded;}

#ifdef GP_NINJA_GAMES
#define MISSION_CONTROLLER(number,rm) const GameController GAME_MISSION##number##_CONTROLLER={ \
 .key="mission" #number,.kind=GAME_SOLO,.room=rm,.rate=24,.backgroundRGB=0x071c33, \
 .banks={"missions/" #number "/scene.gpk"},.lazy=1,.order={0,1,2}, \
 .bank_resource=bank_resource,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet, \
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view, \
};
MISSION_CONTROLLER(4,913)
MISSION_CONTROLLER(5,914)
MISSION_CONTROLLER(6,915)
MISSION_CONTROLLER(7,920)
MISSION_CONTROLLER(8,921)
MISSION_CONTROLLER(9,922)
MISSION_CONTROLLER(10,923)
MISSION_CONTROLLER(11,927)
#endif
