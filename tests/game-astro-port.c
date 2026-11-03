#include "game_controller.h"
#include "game_astro.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,score,left,coins;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"900"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;f->score=(unsigned)strtoul(args[0],NULL,10);}else assert(false);
 return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

static GamePort *open_game(Fixture *f,bool online){
 memset(f,0,sizeof *f);GameDescriptor descriptor={.key="astro",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=online};
 GameHost host={.user=f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 if(online){assert(f->join==1);char *args[]={"900"};assert(game_port_packet(p,"jg",1,args));}
 assert(game_port_session(p)==GAME_SESSION_PLAYING);
 return p;
}

int main(void){
 Fixture fixture;GamePort *p=open_game(&fixture,true);SWLScene *scene=game_bank_scene(game_port_bank(p,0));
 assert(scene);assert(swl_frames(scene,"root.astroBarrier")>=592);
 assert(render(scene)>0);
 for(unsigned i=0;i<ASTRO_LEVELS;i++){
  unsigned frame=ASTRO_LEVEL_DATA[i].frame;
  assert(swl_view_frame(scene,"root.astroBarrier",frame));
  assert(swl_prepare(scene));
  assert(render(scene)>0);
 }
 assert(swl_view_frame(scene,"root.astroBarrier",1));
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(swl_frame(scene,"root.astroBarrier")==2);
 AstroModel *model=game_astro_model(game_port_game(p));assert(model);
 model->gunX=55;
 for(unsigned i=0;i<40&&model->score==0;i++)game_port_update(p,1.0f/24,&(GameInput){.down=i==0?GAME_KEY_A:0});
 assert(model->score>=10);assert(model->hits>=1);
 unsigned score=model->score;
 game_port_close(p);assert(fixture.zo==1&&fixture.score==score);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));
 assert(fixture.left==1&&fixture.coins==510);
 game_port_free(p);

 Fixture offline;GamePort *q=open_game(&offline,false);
 game_port_update(q,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 game_port_close(q);assert(offline.zo==0);tick(q,1);assert(offline.left==1);
 game_port_free(q);
 puts("PASS Astro Barrier native controller: original bank renders all 60 level frames, title to level 1, fire and hit, one zo and authoritative return, offline no payout");
 return 0;
}
