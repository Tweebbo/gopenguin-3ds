#include "game_controller.h"
#include "game_mission3.h"
#include "game_mission3_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MISSION3_ROOM 911
#define MISSION3_SCORE 300
enum {SCREEN_PLAY,SCREEN_DONE};
enum {BUTTON_ACTION=1,BUTTON_PAY=3,BUTTON_EXIT=2,BUTTON_FUSE_BASE=10};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 unsigned step,screen,sceneIndex;
 unsigned fuses;
 bool loaded,online;
 char resource[48];
} Mission3;

unsigned game_mission3_step(void *game){return ((Mission3 *)game)->step;}
unsigned game_mission3_fuses(void *game){return ((Mission3 *)game)->fuses;}
void game_mission3_toggle(void *game,unsigned index){
 Mission3 *m=game;if(index>3)return;
 for(int at=(int)index-1;at<=(int)index+1;at++)if(at>=0&&at<4)m->fuses^=1u<<at;
}
static void request_scene(Mission3 *m,unsigned scene){
 if(m->loaded&&m->sceneIndex==scene)return;
 if(m->bank)game_port_release_bank(m->port,0);
 m->bank=NULL;m->scene=NULL;m->loaded=false;m->sceneIndex=scene;
 snprintf(m->resource,sizeof m->resource,"%s",MISSION3_SCENE_PACKS[scene]);
 GameResource state=game_port_require_bank(m->port,0);
 if(state==GAME_RESOURCE_READY){m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);m->loaded=m->scene!=NULL;if(m->loaded)game_port_snap(m->port);}
}
static void *create(GamePort *port){
 Mission3 *m=calloc(1,sizeof *m);if(!m)return NULL;m->port=port;m->online=game_port_descriptor(port)->online;
 snprintf(m->resource,sizeof m->resource,"%s",MISSION3_SCENE_PACKS[0]);return m;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){Mission3 *m=game;request_scene(m,0);return true;}
static void tick(void *game){
 Mission3 *m=game;
 if(!m->loaded){GameResource state=game_port_require_bank(m->port,0);if(state==GAME_RESOURCE_READY){m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);m->loaded=m->scene!=NULL;if(m->loaded)game_port_snap(m->port);}return;}
 if(m->scene)swl_tick_deferred(m->scene);
}
static void advance(Mission3 *m){
 const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 if(step->fuse&&m->fuses!=step->target)return;
 if(m->step+1>=MISSION3_STEPS){m->screen=SCREEN_DONE;return;}
 m->step++;
 const Mission3Step *next=&MISSION3_STEP_DATA[m->step];
 if(next->scene!=m->sceneIndex)request_scene(m,next->scene);
 if(!next->fuse)m->fuses=0;
}
static void frame(void *game,float seconds,const GameInput *input){
 Mission3 *m=game;(void)seconds;
 if(!m->online){game_port_leave(m->port);return;}
 unsigned clicked=game_port_clicked(m->port);
 if(clicked==BUTTON_EXIT){if(m->screen==SCREEN_DONE)game_port_finish(m->port,MISSION3_SCORE);else game_port_leave(m->port);return;}
 if(m->screen==SCREEN_DONE){if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(m->port,MISSION3_SCORE);return;}
 const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 if(step->fuse&&clicked>=BUTTON_FUSE_BASE&&clicked<BUTTON_FUSE_BASE+4){game_mission3_toggle(m,clicked-BUTTON_FUSE_BASE);return;}
 if(clicked==BUTTON_ACTION||(input->down&GAME_KEY_A))advance(m);
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Mission3 *m=game;if(m->screen==SCREEN_DONE)game_port_finish(m->port,MISSION3_SCORE);else game_port_leave(m->port);}
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
 Mission3 *m=game;GamePort *p=m->port;char line[96],lines[10][56];
 if(m->screen==SCREEN_DONE){
  game_port_text(p,160,54,15,0xffffff,GAME_ALIGN_CENTER,"Mission accomplished!");
  game_port_text(p,160,82,10,0xd6ecff,GAME_ALIGN_CENTER,"The coins are back and the Night Club has power.");
  snprintf(line,sizeof line,"You earned %u coins.",MISSION3_SCORE/10);
  game_port_text(p,160,106,11,0xffffff,GAME_ALIGN_CENTER,line);
  game_port_button(p,BUTTON_PAY,90,150,140,42,"Get coins",true);
  game_port_button(p,BUTTON_EXIT,260,2,54,20,"Exit",true);
  return;
 }
 const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 game_port_text(p,6,6,10,0xffffff,GAME_ALIGN_LEFT,"PSA: Case of the Missing Coins");
 snprintf(line,sizeof line,"%u/%u",m->step+1,MISSION3_STEPS);
 game_port_text(p,314,6,9,0xffffff,GAME_ALIGN_RIGHT,line);
 game_port_button(p,BUTTON_EXIT,270,22,44,18,"Exit",true);
 game_port_text(p,6,22,10,0xffe08a,GAME_ALIGN_LEFT,step->title);
 unsigned count=wrap(step->text,lines,8);
 for(unsigned i=0;i<count;i++)game_port_text(p,6,40+i*13,8,0xf2f6ff,GAME_ALIGN_LEFT,lines[i]);
 unsigned y=40+count*13+4;if(y>142)y=142;
 game_port_text(p,6,y,9,0x9fe870,GAME_ALIGN_LEFT,step->objective);
 if(step->fuse){
  for(unsigned i=0;i<4;i++){
   snprintf(line,sizeof line,"%u %s",i+1,(m->fuses>>i&1)?"ON":"OFF");
   game_port_button(p,BUTTON_FUSE_BASE+i,8+i*78,164,72,28,line,true);
  }
  game_port_button(p,BUTTON_ACTION,40,198,240,38,(m->fuses==step->target)?step->action:"Set the fuses",m->fuses==step->target);
 }else{
  game_port_button(p,BUTTON_ACTION,40,198,240,38,step->action,true);
 }
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Mission3 *m=game;return bank==0?m->resource:NULL;}
#endif
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Mission3 *m=game;return bank==0&&m->loaded;}

const GameController GAME_MISSION3_CONTROLLER={
 .key="mission3",.kind=GAME_SOLO,.room=MISSION3_ROOM,.rate=24,
 .backgroundRGB=0x071c33,
 .banks={"missions/3/vault.gpk"},.lazy=1,.order={0,1,2},
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,
};
