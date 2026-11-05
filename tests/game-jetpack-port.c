#include "game_controller.h"
#include "game_jetpack.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,left,coins,loads;char loaded[8][32];} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){Fixture *f=user;if(f->loads<8)snprintf(f->loaded[f->loads++],sizeof f->loaded[0],"%s",name);snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
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
 /* The original background_mc backdrop loads as bank 1 and shows the level's frame. */
 {SWLScene *bg=game_bank_scene(game_port_bank(p,1));assert(bg);assert(swl_frame(bg,"root.bg")==1);assert(render(bg)>0);
  const GameController *c=game_port_controller(p);SWLMatrix local;assert(c->bank_view(game_port_game(p),1,&local));}
 JetpackModel *m=game_jetpack_model(game_port_game(p));assert(m);assert(m->phase==JETPACK_PLAYING);
 {
  const GameController *controller=game_port_controller(p);SWLMatrix view;SWLInstance piece;
  /* The touch screen draws only the backdrop bank's interFace frame (no second copy of the flight). */
  assert(controller->lower_view(game_port_game(p),&view));
  assert(controller->lower_frame(game_port_game(p),0)==0&&controller->lower_frame(game_port_game(p),1)==2);
  assert(controller->lower_bank_view(game_port_game(p),1,&view)&&!controller->lower_bank_view(game_port_game(p),0,&view));
  /* The terrain pieces (hazard clips) are placed under the camera. */
  assert(swl_pose(scene,"root.haz0",&piece));assert(swl_frame(scene,"root.haz0")>1);
  double y=m->y;game_port_touch(p,GAME_TOUCH_DOWN,160,200);tick(p,8,(GameInput){0});assert(m->y<y&&m->fuel<100);
  game_port_touch(p,GAME_TOUCH_UP,160,4);double upward=m->upSpeed;tick(p,2,(GameInput){0});assert(m->upSpeed<upward);
  jetpack_model_load(m);tick(p,1,(GameInput){0});
 }
 /* Regression: every map cell equals the authored level byte (cells were once packed four per unsigned). */
 {const JetpackLevelData *l=jetpack_model_level(m);for(unsigned i=0;i<l->width*l->height;i++)assert(m->map[i]==l->tiles[i]);}
 unsigned startY=(unsigned)m->y;
 /* The lower-screen Thrust pad lifts the penguin while the stylus is held (it was overwritten every frame). */
 {double y0=m->y;game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,160,215);tick(p,12,(GameInput){0});assert(m->y<y0);game_port_touch(p,GAME_TOUCH_UP,160,215);
  jetpack_model_load(m);m->fuel=100;startY=(unsigned)m->y;}
 tick(p,20,(GameInput){.held=GAME_KEY_A});assert(m->y<startY);assert(m->fuel<100);
 SWLInstance inst;assert(swl_instance(scene,"root.player",&inst));assert(inst.matrix.y>-1000);assert(swl_instance(scene,"root.tile0",&inst));
 for(unsigned level=0;level<JETPACK_LEVELS;level++){
  m->level=level;jetpack_model_load(m);
  m->map[((unsigned)(m->y/80))*m->width+(unsigned)(m->x/80)]=29;m->landed=true;m->fuel=100;
  tick(p,1,(GameInput){0});assert(m->phase==JETPACK_DONE);
  game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
  if(level+1<JETPACK_LEVELS){assert(m->level==level+1);assert(swl_frame(game_bank_scene(game_port_bank(p,1)),"root.bg")==level+2);}else assert(m->phase==JETPACK_WON);
 }
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(fixture.loads==6&&!strcmp(fixture.loaded[0],"games/jetpackbg.gpk"));
 static const char *levels[]={"games/jetpack.gpk","games/jetpack2.gpk","games/jetpack3.gpk","games/jetpack4.gpk","games/jetpack5.gpk"};
 for(unsigned i=0;i<5;i++)assert(!strcmp(fixture.loaded[i+1],levels[i]));
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
