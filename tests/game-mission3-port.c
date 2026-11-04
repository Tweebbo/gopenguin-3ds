#include "game_controller.h"
#include "game_mission3.h"
#include "game_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,coins,left,loads,soundCount;char loaded[16][48],sounds[16][16];} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){
 Fixture *f=user;if(f->loads<16)snprintf(f->loaded[f->loads++],sizeof f->loaded[0],"%s",name);
 snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;
}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;
 if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"911"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);assert(!strcmp(args[0],"300"));f->zo++;}else assert(false);
 return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void sound(void *user,const char *bank,const char *name,unsigned volume,bool loop){Fixture *f=user;(void)bank;(void)volume;(void)loop;if(f->soundCount<16)snprintf(f->sounds[f->soundCount++],sizeof f->sounds[0],"%s",name);}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

int main(void){
 Fixture f={0};GameDescriptor descriptor={.key="mission3",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins,.sound=sound};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,2);
 char *join[]={"911"};assert(game_port_packet(p,"jg",1,join));assert(f.join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 assert(f.loads==1&&!strcmp(f.loaded[0],"missions/3/vault.gpk"));
 SWLScene *scene=game_bank_scene(game_port_bank(p,0));assert(scene);assert(render(scene)>0);
 assert(game_mission3_step(game_port_game(p))==0&&game_mission3_fuses(game_port_game(p))==0);
 for(unsigned step=1;step<10;step++){
  game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
  tick(p,2);
  assert(game_mission3_step(game_port_game(p))==step);
  assert(game_port_bank(p,0)&&game_bank_scene(game_port_bank(p,0)));
 }
 assert(f.loads>=5);
 assert(game_mission3_step(game_port_game(p))==9);
 for(unsigned i=0;i<5;i++){unsigned sequence[]={0,0,1,2,3};game_mission3_toggle(game_port_game(p),sequence[i]);}
 assert(game_mission3_fuses(game_port_game(p))==5);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 tick(p,2);assert(game_mission3_step(game_port_game(p))==10);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 tick(p,2);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(f.soundCount==11&&!strcmp(f.sounds[0],"scene0")&&!strcmp(f.sounds[4],"scene2"));
 assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 assert(f.loads>=8);
 for(unsigned i=0;i<f.loads;i++)assert(!strncmp(f.loaded[i],"missions/3/",11));
 game_port_free(p);
 puts("PASS PSA mission 3 native controller: lazy scene banks load and switch, 11 steps, fuse gate, one payout and return");
 return 0;
}
