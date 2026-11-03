#include "game_controller.h"
#include "game_jetpack.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,left,coins;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"906"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;}else assert(false);return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count,GameInput input){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&input);}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

static GamePort *open_game(Fixture *f,bool online){
 memset(f,0,sizeof *f);GameDescriptor descriptor={.key="jetpack",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=online};
 GameHost host={.user=f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1,(GameInput){0});
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 if(online){assert(f->join==1);char *args[]={"906"};assert(game_port_packet(p,"jg",1,args));}
 assert(game_port_session(p)==GAME_SESSION_PLAYING);return p;
}

int main(void){
 Fixture fixture;GamePort *p=open_game(&fixture,true);SWLScene *scene=game_bank_scene(game_port_bank(p,0));assert(scene);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(render(scene)>0);
 JetpackModel *m=game_jetpack_model(game_port_game(p));assert(m);assert(m->phase==JETPACK_PLAYING);
 unsigned startY=(unsigned)m->y;
 tick(p,20,(GameInput){.held=GAME_KEY_A});assert(m->y<startY);assert(m->fuel<100);
 SWLInstance inst;assert(swl_instance(scene,"root.player",&inst));assert(inst.matrix.y>-1000);assert(swl_instance(scene,"root.tile0",&inst));
 for(unsigned level=0;level<JETPACK_LEVELS;level++){
  m->level=level;jetpack_model_load(m);
  m->map[((unsigned)(m->y/80))*m->width+(unsigned)(m->x/80)]=29;m->landed=true;m->fuel=100;
  tick(p,1,(GameInput){0});assert(m->phase==JETPACK_DONE);
  game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
  if(level+1<JETPACK_LEVELS)assert(m->level==level+1);else assert(m->phase==JETPACK_WON);
 }
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(fixture.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));
 assert(fixture.left==1&&fixture.coins==510);
 game_port_free(p);

 Fixture offline;GamePort *q=open_game(&offline,false);
 game_port_update(q,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 game_port_close(q);assert(offline.zo==0);tick(q,1,(GameInput){0});assert(offline.left==1);
 game_port_free(q);
 puts("PASS Jet Pack Adventure native controller: bank renders, thrust and fuel, all five levels, one zo and authoritative return, offline no payout");
 return 0;
}
