#include "game_controller.h"
#include "game_mission.h"
#include "game_mission_flow_data.h"
#include "game_mission_layout.h"
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
 unsigned pattern,focus;
 float wrong;
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
 m->orderProgress=0;m->pattern=0;m->code[0]=0;m->focus=0;m->wrong=0;
 (void)p;
}
void game_mission_press_order(void *game,unsigned index){
 Mission *m=game;const MissionPuzzle *p=&current(m)->puzzle;if(p->type!=3||index>=p->count||m->orderProgress>=p->count)return;
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
 /* Always release: a scene switch while the previous bank is still loading must discard that job, or its room
    (the wrong one) would be shown when it finishes. */
 game_port_release_bank(m->port,0);
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
static void scene_audio(Mission *m,unsigned scene){
 char name[16];snprintf(name,sizeof name,"amb%u",scene);game_port_sound(m->port,name,25,true);
 snprintf(name,sizeof name,"voice%u",scene);game_port_sound(m->port,name,60,false);
}
static bool ready(void *game){Mission *m=game;request_scene(m,0);scene_audio(m,0);return true;}
static void tick(void *game){
 Mission *m=game;
 if(!m->loaded){GameResource state=game_port_require_bank(m->port,0);if(state==GAME_RESOURCE_READY){m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);m->loaded=m->scene!=NULL;if(m->loaded)game_port_snap(m->port);}return;}
 if(m->scene)swl_tick_deferred(m->scene);
}
static void advance(Mission *m){
 if(!solved(m))return;
 char sound[16];game_port_stop_sounds(m->port);snprintf(sound,sizeof sound,"scene%u",current(m)->scene);game_port_sound(m->port,sound,45,false);
 if(m->step+1>=m->data->stepCount){m->screen=SCREEN_DONE;return;}
 m->step++;reset_puzzle(m);
 const MissionStep *next=current(m);scene_audio(m,next->scene);
 if(next->scene!=m->sceneIndex)request_scene(m,next->scene);
}
/* Lower-screen geometry for the puzzle controls (shared by drawing and D-pad focus). */
static void choice_rect(unsigned count,unsigned i,float *x,float *y,float *w,float *h){
 unsigned rows=(count+1)/2;float top=188-rows*28-(rows-1)*5;
 *w=150;*h=28;*y=top+(i/2)*33;*x=(count%2&&i==count-1)?85:8+(i%2)*154;
}
static unsigned unique_keys(const char *code,char *out){
 unsigned n=0;for(unsigned i=0;code[i]&&n<10;i++)if(!memchr(out,code[i],n))out[n++]=code[i];
 out[n]=0;return n;
}
static void key_rect(unsigned count,unsigned i,float *x,float *y,float *w,float *h){
 *w=count>5?40:52;*h=44;float gap=8,total=count*(*w)+(count-1)*gap;*x=160-total/2+i*(*w+gap);*y=104;
}
static unsigned control_count(const Mission *m){
 const MissionPuzzle *p=&current(m)->puzzle;char keys[12];
 return p->type==3?p->count:p->type==2?unique_keys(p->code,keys):0;
}
static bool chosen(const Mission *m,unsigned index){
 const MissionPuzzle *p=&current(m)->puzzle;for(unsigned j=0;j<m->orderProgress&&j<8;j++)if(p->order[j]==index)return true;return false;
}
static void press_choice(Mission *m,unsigned index){
 /* A choice already in the sequence is shown ticked and disabled: pressing it again is ignored. */
 if(chosen(m,index))return;
 unsigned before=m->orderProgress;game_mission_press_order(m,index);
 if(m->orderProgress<=before&&before<current(m)->puzzle.count)m->wrong=1.6f;else m->wrong=0;
}
static void press_key(Mission *m,unsigned index){
 const MissionPuzzle *p=&current(m)->puzzle;char keys[12];unsigned n=unique_keys(p->code,keys);
 if(index<n){if(strlen(m->code)>=(size_t)p->count)game_mission_clear_code(m);game_mission_type_code(m,keys[index]);}
}
static void frame(void *game,float seconds,const GameInput *input){
 Mission *m=game;
 if(!m->online){game_port_leave(m->port);return;}
 if(m->wrong>0)m->wrong-=seconds;
 unsigned clicked=game_port_clicked(m->port);
 if(clicked==BUTTON_EXIT){if(m->screen==SCREEN_DONE)game_port_finish(m->port,m->data->reward);else game_port_leave(m->port);return;}
 if(m->screen==SCREEN_DONE){if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(m->port,m->data->reward);return;}
 const MissionPuzzle *p=&current(m)->puzzle;unsigned controls=control_count(m);
 if(controls){
  unsigned row=p->type==3?2:controls;
  if(input->down&GAME_KEY_RIGHT)m->focus=(m->focus+1)%controls;
  if(input->down&GAME_KEY_LEFT)m->focus=(m->focus+controls-1)%controls;
  if(p->type==3&&(input->down&GAME_KEY_DOWN)&&m->focus+row<controls)m->focus+=row;
  if(p->type==3&&(input->down&GAME_KEY_UP)&&m->focus>=row)m->focus-=row;
  if(m->focus>=controls)m->focus=0;
 }
 if(clicked>=BUTTON_CHOICE_BASE&&clicked<BUTTON_CHOICE_BASE+8){m->focus=clicked-BUTTON_CHOICE_BASE;press_choice(m,m->focus);}
 else if(clicked>=BUTTON_KEY_BASE&&clicked<BUTTON_KEY_BASE+10){m->focus=clicked-BUTTON_KEY_BASE;press_key(m,m->focus);}
 else if(clicked==BUTTON_CLEAR||(p->type==2&&(input->down&GAME_KEY_B)))game_mission_clear_code(m);
 else if(clicked==BUTTON_ACTION||clicked==ML_BUTTON_SCENE)advance(m);
 else if(input->down&GAME_KEY_A){
  /* A works the focused control until the puzzle is solved, then performs the step's action. */
  if(solved(m))advance(m);
  else if(p->type==3)press_choice(m,m->focus);
  else if(p->type==2)press_key(m,m->focus);
 }
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Mission *m=game;if(m->screen==SCREEN_DONE)game_port_finish(m->port,m->data->reward);else game_port_leave(m->port);}
static void draw_top(void *game){
 Mission *m=game;GamePort *p=m->port;const MissionStep *step=current(m);char line[160];
 if(m->screen==SCREEN_DONE){
  ml_top_tags(p,m->data->mission,m->data->title,0,0);
  ml_accomplished(p,m->data->title,m->data->reward/10);
  snprintf(line,sizeof line,"Great work, Agent. The case of %s is closed.",m->data->title);
  ml_dialog(p,"G",line,"Collect your coins and return to the island.");
  return;
 }
 ml_top_tags(p,m->data->mission,m->data->title,m->step+1,m->data->stepCount);
 ml_dialog(p,step->title,step->text,step->objective);
}
static void draw_sequence(Mission *m,const MissionPuzzle *puzzle){
 GamePort *p=m->port;char line[64];float top;{float x,w,h;choice_rect(puzzle->count,0,&x,&top,&w,&h);}
 float y=top-30;if(y<28)y=28;
 if(m->wrong>0){ml_panel(p,6,y,308,24,9,0xffffff,ML_BAD,1,true);game_port_text(p,160,y+5,11,0xffffff,GAME_ALIGN_CENTER,"Wrong order! The sequence starts again.");return;}
 ml_panel(p,6,y,308,24,9,0x9fd6f5,ML_PLATE_DARK,1,true);
 snprintf(line,sizeof line,"%u/%u",m->orderProgress,puzzle->count);
 game_port_text(p,16,y+5,11,ML_GOLD,GAME_ALIGN_LEFT,line);
 float x=16+ml_width(11,line)+8,right=306;
 if(!m->orderProgress){game_port_text(p,x,y+5,11,0xd6ecff,GAME_ALIGN_LEFT,"Choose the steps in order.");return;}
 /* Chips for the chosen sequence; the newest stay visible when they do not all fit. */
 unsigned first=0;
 for(;;){float w=0;for(unsigned i=first;i<m->orderProgress;i++)w+=ml_width(10,puzzle->labels[puzzle->order[i]])+14;if(x+w<=right||first+1>=m->orderProgress)break;first++;}
 if(first){game_port_text(p,x,y+5,11,0xd6ecff,GAME_ALIGN_LEFT,"..");x+=12;}
 for(unsigned i=first;i<m->orderProgress;i++){
  const char *label=puzzle->labels[puzzle->order[i]];float w=ml_width(10,label)+10;if(x+w>right)w=right-x;if(w<12)break;
  ml_round(p,x,y+4,w,16,7,0xd3f9d8);game_port_text(p,x+w/2,y+6,10,0x1b5e20,GAME_ALIGN_CENTER,label);x+=w+4;
 }
}
static void draw_bottom(void *game){
 Mission *m=game;GamePort *p=m->port;const MissionStep *step=current(m);const MissionPuzzle *puzzle=&step->puzzle;
 if(m->screen==SCREEN_DONE){
  ml_header(p,"Mission accomplished",m->data->stepCount+1,m->data->stepCount);
  char line[48];snprintf(line,sizeof line,"Get %u coins",m->data->reward/10);
  ml_action(p,BUTTON_PAY,line,true,NULL);return;
 }
 ml_header(p,step->title,m->step+1,m->data->stepCount);
 if(!m->loaded)return;
 if(puzzle->type==3){
  draw_sequence(m,puzzle);
  for(unsigned i=0;i<puzzle->count&&i<8;i++){
   float x,y,w,h;choice_rect(puzzle->count,i,&x,&y,&w,&h);
   unsigned at=0;for(unsigned j=0;j<m->orderProgress;j++)if(puzzle->order[j]==i)at=j+1;
   if(m->focus==i)ml_focus(p,x,y,w,h);
   unsigned id=BUTTON_CHOICE_BASE+i;game_port_button(p,id,x,y,w,h,puzzle->labels[i],!at);
   if(at)ml_tick(p,x+5,y+5,at);
  }
 }else if(puzzle->type==2){
  char keys[12];unsigned n=unique_keys(puzzle->code,keys);
  ml_panel(p,24,34,272,152,12,0x9fd6f5,ML_PLATE_DARK,1,true);
  bool full=strlen(m->code)>=(size_t)puzzle->count,right=full&&solved(m);
  game_port_text(p,160,40,11,right?0x8ce99a:full?0xffa8a8:0xd6ecff,GAME_ALIGN_CENTER,right?"Code accepted!":full?"Wrong code. Tap a key to try again.":"Enter the code");
  float sw=34,gap=8,total=puzzle->count*sw+(puzzle->count-1)*gap,sx=160-total/2;size_t typed=strlen(m->code);
  for(unsigned i=0;i<puzzle->count;i++){
   bool filled=i<typed,next=i==typed;char c[2]={filled?m->code[i]:0,0};
   ml_panel(p,sx+i*(sw+gap),56,sw,38,7,right?ML_OK:full?ML_BAD:next?ML_GOLD:0x9fd6f5,filled?ML_PAPER:0x0a2a4a,2,false);
   if(filled)game_port_text(p,sx+i*(sw+gap)+sw/2,66,16,ML_INK,GAME_ALIGN_CENTER,c);
  }
  for(unsigned i=0;i<n;i++){
   float x,y,w,h;key_rect(n,i,&x,&y,&w,&h);char c[2]={keys[i],0};
   if(m->focus==i)ml_focus(p,x,y,w,h);
   game_port_button(p,BUTTON_KEY_BASE+i,x,y,w,h,c,true);
   uint32_t swatch=keys[i]=='R'?0xe03131:keys[i]=='G'?0x2f9e44:keys[i]=='B'?0x1c7ed6:keys[i]=='Y'?0xfcc419:keys[i]=='O'?0xf76707:0;
   if(swatch&&strspn(puzzle->code,"RGBYO")==strlen(puzzle->code))ml_round(p,x+w/2-9,y+h-11,18,6,3,swatch);
  }
  game_port_button(p,BUTTON_CLEAR,112,156,96,24,"Clear",typed>0);
  if(typed)ml_key(p,118,160,"B");
 }
 const char *hint=puzzle->type==3?NULL:puzzle->type==2?NULL:"Tap the room or press A";
 if(!puzzle->type)game_port_hotspot(p,ML_BUTTON_SCENE,0,24,320,148,true);
 ml_action(p,BUTTON_ACTION,step->action,solved(m),hint);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Mission *m=game;return bank==0?m->resource:NULL;}
#endif
/* Top screen: the room scene itself, full-bleed (the host covers 400x240 with the 760x480 stage). */
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)game;*local=(SWLMatrix){1,0,0,1,0,0};return bank==0;}
static bool lower_view(void *game,SWLMatrix *view){Mission *m=game;*view=mission_scene_view();return m->loaded;}
static bool lower_bank_view(void *game,unsigned bank,SWLMatrix *view){return bank==0&&lower_view(game,view);}

#ifdef GP_NINJA_GAMES
#define MISSION_CONTROLLER(number,rm) const GameController GAME_MISSION##number##_CONTROLLER={ \
 .key="mission" #number,.kind=GAME_SOLO,.room=rm,.rate=24,.nightPanel=true,.backgroundRGB=0x071c33, \
 .banks={"missions/" #number "/scene.gpk"},.lazy=1,.order={0,1,2}, \
 .bank_resource=bank_resource,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet, \
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.lower_view=lower_view,.lower_bank_view=lower_bank_view,.draw_top=draw_top, \
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
