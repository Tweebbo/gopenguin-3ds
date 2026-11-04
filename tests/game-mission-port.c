#include "game_controller.h"
#include "game_mission.h"
#include "game_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,coins,left,loads;char loaded[24][56];} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){
 Fixture *f=user;if(f->loads<24)snprintf(f->loaded[f->loads++],sizeof f->loaded[0],"%s",name);
 snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;
}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;
 if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3);f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1&&!strcmp(args[0],"300"));f->zo++;}else assert(false);
 return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}
static void solo(GamePort *p,Fixture *f,char *const *join){
 assert(f->join==1);assert(game_port_packet(p,"jg",1,join));assert(game_port_session(p)==GAME_SESSION_PLAYING);
}

static void play_mission(const char *key,unsigned room){
 Fixture f={0};GameDescriptor descriptor={.key="",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 snprintf(descriptor.key,sizeof descriptor.key,"%s",key);
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,2);
 char roomArg[8];snprintf(roomArg,sizeof roomArg,"%u",room);char *join[]={roomArg};solo(p,&f,join);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 void *game=game_port_game(p);const MissionData *data=game_mission_data(game);assert(data&&data->room==room);
 assert(game_mission_step(game)==0);
 for(unsigned step=0;step<data->stepCount;step++){
  assert(game_mission_step(game)==step);
  const MissionPuzzle *puzzle=&data->steps[step].puzzle;
  if(puzzle->type==3)for(unsigned i=0;i<puzzle->count;i++)game_mission_press_order(game,puzzle->order[i]);
  else if(puzzle->type==2)for(unsigned i=0;puzzle->code[i];i++)game_mission_type_code(game,puzzle->code[i]);
  if(step+1==data->stepCount){
   game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
   tick(p,2);
   break;
  }
  game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
  tick(p,2);
  assert(game_mission_step(game)==step+1);
  SWLScene *scene=game_bank_scene(game_port_bank(p,0));assert(scene);assert(render(scene)>0);
 }
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 assert(f.loads>=2);
 for(unsigned i=0;i<f.loads;i++)assert(!strncmp(f.loaded[i],"missions/",9));
 game_port_free(p);
 printf("PASS PSA %s: %u steps, lazy banks and payout\n",key,data->stepCount);
}

int main(void){
 play_mission("mission4",913);
 play_mission("mission7",920);
 play_mission("mission11",927);
 puts("PASS PSA missions 4, 7 and 11 native controller: scene banks, order and code puzzles, one payout each");
 return 0;
}
