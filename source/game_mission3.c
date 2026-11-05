#include "game_controller.h"
#include "game_mission3.h"
#include "game_mission3_data.h"
#include "game_mission_layout.h"
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
 unsigned fuses,focus;
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
 /* Always release: a scene switch while the previous bank is still loading must discard that job, or its room
    (the wrong one) would be shown when it finishes. */
 game_port_release_bank(m->port,0);
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
static void scene_audio(Mission3 *m,unsigned scene){
 char name[16];snprintf(name,sizeof name,"amb%u",scene);game_port_sound(m->port,name,25,true);
 snprintf(name,sizeof name,"voice%u",scene);game_port_sound(m->port,name,60,false);
}
static bool ready(void *game){Mission3 *m=game;request_scene(m,0);scene_audio(m,0);return true;}
static void tick(void *game){
 Mission3 *m=game;
 if(!m->loaded){GameResource state=game_port_require_bank(m->port,0);if(state==GAME_RESOURCE_READY){m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);m->loaded=m->scene!=NULL;if(m->loaded)game_port_snap(m->port);}return;}
 if(m->scene)swl_tick_deferred(m->scene);
}
static void advance(Mission3 *m){
 const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 if(step->fuse&&m->fuses!=step->target)return;
 char sound[16];game_port_stop_sounds(m->port);snprintf(sound,sizeof sound,"scene%u",step->scene);game_port_sound(m->port,sound,45,false);
 if(m->step+1>=MISSION3_STEPS){m->screen=SCREEN_DONE;return;}
 m->step++;
 const Mission3Step *next=&MISSION3_STEP_DATA[m->step];
 scene_audio(m,next->scene);
 if(next->scene!=m->sceneIndex)request_scene(m,next->scene);
 if(!next->fuse)m->fuses=0;
}
#define FUSE_X(i) (22.0f+(i)*72.0f)
#define FUSE_Y 96.0f
static void toggle(Mission3 *m,unsigned index){game_mission3_toggle(m,index);m->focus=index;}
static void frame(void *game,float seconds,const GameInput *input){
 Mission3 *m=game;(void)seconds;
 if(!m->online){game_port_leave(m->port);return;}
 unsigned clicked=game_port_clicked(m->port);
 if(clicked==BUTTON_EXIT){if(m->screen==SCREEN_DONE)game_port_finish(m->port,MISSION3_SCORE);else game_port_leave(m->port);return;}
 if(m->screen==SCREEN_DONE){if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(m->port,MISSION3_SCORE);return;}
 const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 if(step->fuse){
  if(input->down&GAME_KEY_RIGHT)m->focus=(m->focus+1)%4;
  if(input->down&GAME_KEY_LEFT)m->focus=(m->focus+3)%4;
  if(clicked>=BUTTON_FUSE_BASE&&clicked<BUTTON_FUSE_BASE+4){toggle(m,clicked-BUTTON_FUSE_BASE);return;}
  /* A flips the focused switch until the pattern is right, then restores the power. */
  if(!clicked&&(input->down&GAME_KEY_A)&&m->fuses!=step->target){toggle(m,m->focus);return;}
 }
 if(clicked==BUTTON_ACTION||clicked==ML_BUTTON_SCENE||(input->down&GAME_KEY_A))advance(m);
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Mission3 *m=game;if(m->screen==SCREEN_DONE)game_port_finish(m->port,MISSION3_SCORE);else game_port_leave(m->port);}
static void draw_top(void *game){
 Mission3 *m=game;const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 if(m->screen==SCREEN_DONE){
  ml_top_tags(m->port,3,"Case of the Missing Coins",0,0);
  ml_accomplished(m->port,"Case of the Missing Coins",MISSION3_SCORE/10);
  ml_dialog(m->port,"G","Great work, Agent. The vault is full again and the dance floor is lit.","Collect your coins and return to the island.");
  return;
 }
 ml_top_tags(m->port,3,"Case of the Missing Coins",m->step+1,MISSION3_STEPS);
 ml_dialog(m->port,step->title,step->text,step->objective);
}
static void lamp(GamePort *p,float cx,float y,bool on,float size){
 ml_round(p,cx-size/2,y,size,size,size/2,0x10202c);
 ml_round(p,cx-size/2+2,y+2,size-4,size-4,size/2-2,on?0x51cf66:0x5c1a1a);
 if(on)ml_round(p,cx-size/2+4,y+3,size/2-2,3,1.5f,0xd3f9d8);
}
static void draw_bottom(void *game){
 Mission3 *m=game;GamePort *p=m->port;const Mission3Step *step=&MISSION3_STEP_DATA[m->step];
 if(m->screen==SCREEN_DONE){ml_header(p,"Mission accomplished",MISSION3_STEPS+1,MISSION3_STEPS);ml_action(p,BUTTON_PAY,"Get 30 coins",true,NULL);return;}
 ml_header(p,step->title,m->step+1,MISSION3_STEPS);
 if(!m->loaded)return;
 bool ready=!step->fuse||m->fuses==step->target;
 if(step->fuse){
  /* The fuse panel: target lamps from the panel label, then one switch per fuse with its live lamp. */
  ml_panel(p,8,30,304,158,12,0x9fd6f5,ML_PLATE_DARK,1,true);
  game_port_text(p,20,37,11,0xd6ecff,GAME_ALIGN_LEFT,"Panel pattern");
  for(unsigned i=0;i<4;i++){bool on=step->target>>i&1;lamp(p,140+i*40,35,on,14);}
  game_port_rect(p,18,56,284,1,0x2b6aa0);
  game_port_text(p,160,62,11,ready?0x8ce99a:0xffffff,GAME_ALIGN_CENTER,ready?"Pattern matched! Restore the power.":"Each switch also flips its neighbours.");
  for(unsigned i=0;i<4;i++){
   bool on=m->fuses>>i&1;float x=FUSE_X(i);
   lamp(p,x+31,FUSE_Y-18,on,16);
   if(m->focus==i)ml_focus(p,x,FUSE_Y,62,58);
   game_port_button(p,BUTTON_FUSE_BASE+i,x,FUSE_Y,62,58,on?"ON":"OFF",true);
   char n[4];snprintf(n,sizeof n,"%u",i+1);game_port_text(p,x+31,FUSE_Y+62,10,0xd6ecff,GAME_ALIGN_CENTER,n);
  }
 }else game_port_hotspot(p,ML_BUTTON_SCENE,0,24,320,148,true);
 ml_action(p,BUTTON_ACTION,ready?step->action:"Match the panel",ready,step->fuse?NULL:"Tap the room or press A");
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
#ifdef GP_NINJA_GAMES
static const char *bank_resource(void *game,unsigned bank){Mission3 *m=game;return bank==0?m->resource:NULL;}
#endif
/* Top screen: the room scene itself, full-bleed. */
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)game;*local=(SWLMatrix){1,0,0,1,0,0};return bank==0;}
static bool lower_view(void *game,SWLMatrix *view){Mission3 *m=game;*view=mission_scene_view();return m->loaded;}
static bool lower_bank_view(void *game,unsigned bank,SWLMatrix *view){return bank==0&&lower_view(game,view);}

const GameController GAME_MISSION3_CONTROLLER={
 .key="mission3",.kind=GAME_SOLO,.room=MISSION3_ROOM,.rate=24,
 .nightPanel=true,.backgroundRGB=0x071c33,
 .banks={"missions/3/vault.gpk"},.lazy=1,.order={0,1,2},
#ifdef GP_NINJA_GAMES
 .bank_resource=bank_resource,
#endif
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.lower_view=lower_view,.lower_bank_view=lower_bank_view,.draw_top=draw_top,
};
