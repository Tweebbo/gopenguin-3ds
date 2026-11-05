#include "game_controller.h"
#include "game_waves.h"
#include "game_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,coins,left;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"912"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;}else assert(false);return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}
static void tap(GamePort *p,float x,float y){game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);game_port_update(p,0,NULL);}

int main(void){
 Fixture f={0};GameDescriptor descriptor={.key="waves",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 char *join[]={"912"};assert(game_port_packet(p,"jg",1,join));assert(f.join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 SWLScene *scene=game_bank_scene(game_port_bank(p,0));assert(scene);assert(render(scene)>0);
 WavesModel *m=game_waves_model(game_port_game(p));assert(m);

 tap(p,120,45);assert(m->mode==WAVES_FREESTYLE);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});assert(m->air);assert(m->vy<0);
 m->air=true;m->spin=360;m->flip=0;m->vy=0.1;m->y=waves_model_surface(m)+8;m->ride=0;m->soundCount=0;
 waves_model_update(m,10,false,false,false,false);
 assert(!m->air&&m->trickScore==100&&m->combo==1&&m->score>=100);
 m->air=true;m->spin=90;m->flip=0;m->vy=0.1;m->y=waves_model_surface(m)+8;m->lives=3;
 waves_model_update(m,10,false,false,false,false);
 assert(m->lives==2&&m->combo==0&&m->air==false);
 assert(render(scene)>0);
 for(unsigned wave=1;wave<3;wave++){m->time=24999;waves_model_update(m,10,false,false,false,false);assert(m->wave==wave+1);assert(m->phase==WAVES_PLAYING);}
 m->time=24999;waves_model_update(m,10,false,false,false,false);assert(m->phase==WAVES_RESULT&&m->success);
 tick(p,1);assert(m->phase==WAVES_RESULT);
 m->score=1234;tap(p,232,194);assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 game_port_free(p);
 puts("PASS Catchin' Waves native controller: bank renders, menu starts, jump and trick, wipeout, three waves, result and authoritative payout");
 return 0;
}
