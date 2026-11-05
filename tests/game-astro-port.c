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
 AstroModel *model=game_astro_model(game_port_game(p));assert(model);assert(model->phase==ASTRO_TITLE);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});tick(p,1);
 assert(model->phase==ASTRO_PLAYING&&model->level==1);assert(swl_frame(scene,"root.astroBarrier")==2);
 {
  const GameController *controller=game_port_controller(p);SWLMatrix view;SWLInstance monitor;
  assert(controller->top_view(game_port_game(p),&view)&&swl_pose(scene,"root.astroBarrier",&monitor));
  /* Upper: the whole cabinet monitor fits the 400x240 screen. */
  float left=view.a*monitor.matrix.x+view.x,top=view.d*monitor.matrix.y+view.y;
  assert(fabsf(view.a-.552f)<.001f&&left>0&&left<200&&top>-10&&top<60);
  /* Lower: the control panel (root frame 2) drawn 1:1; joystick on the left, fire on the right. */
  assert(controller->lower_view(game_port_game(p),&view)&&view.a==1&&view.d==1);
  assert(controller->lower_frame&&controller->lower_frame(game_port_game(p),0)==2);
  double oldX=model->gunX;game_port_touch(p,GAME_TOUCH_DOWN,42,100);tick(p,3);
  assert(model->gunX<oldX&&!model->fireHeld);
  game_port_touch(p,GAME_TOUCH_UP,42,100);game_port_touch(p,GAME_TOUCH_DOWN,236,124);tick(p,2);assert(model->fireHeld);
  game_port_touch(p,GAME_TOUCH_UP,236,124);tick(p,1);assert(!model->fireHeld);
  astro_model_start(model,1);tick(p,1);
 }
 /* Hold A: the gun follows ob0's moving wall until a shot lands (targets move every frame). */
 for(unsigned i=0;i<2000&&model->score==0;i++){
  double box[4];const AstroObject *ob=NULL;for(unsigned j=0;j<model->objectCount;j++)if(!strcmp(model->objects[j].data->name,"ob0"))ob=&model->objects[j];
  assert(ob&&astro_model_wall_box(model,ob,box));double centre=(box[0]+box[2])/2;
  unsigned held=GAME_KEY_A|(model->gunX<centre-3?GAME_KEY_RIGHT:model->gunX>centre+3?GAME_KEY_LEFT:0);
  game_port_update(p,1.0f/24,&(GameInput){.held=held});
  if(model->phase!=ASTRO_PLAYING)break;
 }
 assert(model->score>=10);assert(model->hitCounter>=1);
 /* The clip frame drawn is the model's frame (drawing and collisions agree). */
 for(unsigned j=0;j<model->objectCount;j++){const AstroObject *o=&model->objects[j];if(o->data->clip<0)continue;char path[64];snprintf(path,sizeof path,"root.astroBarrier.%s",o->data->name);assert(swl_frame(scene,path)==o->clipFrame);}
 unsigned score=model->score;
 assert(swl_visible(scene,"root.astroBarrier.leveldone.score_txt",false));
 game_port_close(p);assert(fixture.zo==1&&fixture.score==score);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));
 assert(fixture.left==1&&fixture.coins==510);
 game_port_free(p);

 Fixture offline;GamePort *q=open_game(&offline,false);
 /* Level 10 start from the title's expert buttons, then the level-7 instruction screen. */
 AstroModel *om=game_astro_model(game_port_game(q));
 astro_model_start(om,7);tick(q,1);assert(om->phase==ASTRO_INTRO);assert(swl_frame(game_bank_scene(game_port_bank(q,0)),"root.astroBarrier")==61);
 tick(q,12);game_port_update(q,1.0f/24,&(GameInput){.down=GAME_KEY_START});tick(q,1);assert(om->phase==ASTRO_PLAYING&&om->level==7);
 game_port_close(q);assert(offline.zo==0);tick(q,1);assert(offline.left==1);
 game_port_free(q);
 puts("PASS Astro Barrier native controller: original bank renders all 60 level frames, title to level 1, held fire hits a moving target, drawn clip frames match the model, intro screen, one zo and authoritative return, offline no payout");
 return 0;
}
