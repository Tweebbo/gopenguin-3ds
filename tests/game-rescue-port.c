#include "game_controller.h"
#include "game_rescue.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,score,left,coins;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"949"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;f->score=(unsigned)strtoul(args[0],NULL,10);}else assert(false);return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

int main(void){
 Fixture f={0};GameDescriptor descriptor={.key="rescue",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 char *join[]={"949"};assert(game_port_packet(p,"jg",1,join));assert(f.join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 RescueModel *m=game_rescue_model(game_port_game(p));assert(m);assert(m->phase==RESCUE_PLAYING);
 for(unsigned level=0;level<RESCUE_LEVELS;level++){
  m->level=level;rescue_model_load(m);tick(p,1);
  const RescueLevelData *data=rescue_model_level(m);SWLScene *scene=game_bank_scene(game_port_bank(p,data->theme));
  assert(scene);assert(render(scene)>0);
 }
 m->level=2;rescue_model_load(m);
 assert(m->platformCount>0);
 unsigned ax=0,ay=0,frame=0;assert(rescue_model_platform_cell(m,0,&ax,&ay,&frame));
 unsigned bx=ax,by=ay;
 for(unsigned i=0;i<8;i++){rescue_model_tick(m,500);unsigned cx=0,cy=0;assert(rescue_model_platform_cell(m,0,&cx,&cy,&frame));if(cx!=bx||cy!=by){bx=cx;by=cy;break;}}
 assert(ax!=bx||ay!=by);
 assert(rescue_model_passable(m,bx,by));
 m->x=bx;m->y=by;m->riding=0;
 rescue_model_tick(m,1000);
 unsigned cx=0,cy=0;assert(rescue_model_platform_cell(m,0,&cx,&cy,&frame));
 assert(m->x==cx&&m->y==cy);
 m->level=6;rescue_model_load(m);assert(m->hazardCount>0);
 unsigned hx=0,hy=0;assert(rescue_model_hazard_cell(m,0,&hx,&hy));
 m->x=hx;m->y=hy;m->carried=1;m->riding=-1;
 rescue_model_tick(m,1);
 const RescueLevelData *water=rescue_model_level(m);
 assert(m->x==water->homeX&&m->y==water->homeY&&m->carried==0);
 m->level=0;rescue_model_load(m);
 const RescueLevelData *home=rescue_model_level(m);assert(rescue_model_passable(m,home->homeX,home->homeY));
 bool moved=false;
 for(int dx=-1;dx<=1&&!moved;dx++)for(int dy=-1;dy<=1&&!moved;dy++)if((dx==0)!=(dy==0)&&rescue_model_passable(m,(unsigned)((int)home->homeX+dx),(unsigned)((int)home->homeY+dy))){m->x=(unsigned)((int)home->homeX+dx);m->y=(unsigned)((int)home->homeY+dy);m->carried=3;moved=rescue_model_step(m,-dx,-dy);}
 assert(moved&&m->phase==RESCUE_DONE);assert(m->rescued==3);assert(m->score>=400);
 rescue_model_next(m);assert(m->level==1);
 m->level=0;rescue_model_load(m);m->score=3600;
 game_port_close(p);assert(f.zo==1&&f.score==3600);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 game_port_free(p);
 puts("PASS Puffle Rescue native controller: nine levels, moving platforms carry the player, octopus hazards reset to home, delivery and authoritative payout");
 return 0;
}
