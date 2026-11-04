#include "game_controller.h"
#include "game_aqua.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,left,coins,loads;char loaded[8][32];} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){Fixture *f=user;if(f->loads<8)snprintf(f->loaded[f->loads++],sizeof f->loaded[0],"%s",name);snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"916"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;}else assert(false);return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count,GameInput input){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&input);}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

int main(void){
 Fixture f={0};GameDescriptor descriptor={.key="aqua",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1,(GameInput){0});
 char *join[]={"916"};assert(game_port_packet(p,"jg",1,join));assert(f.join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 assert(f.loads>=2&&!strcmp(f.loaded[0],"games/aqua-sub.gpk")&&!strcmp(f.loaded[1],"games/aqua1.gpk"));
 SWLScene *level=game_bank_scene(game_port_bank(p,0)),*sub=game_bank_scene(game_port_bank(p,1));assert(level&&sub);assert(render(level)>0);assert(render(sub)>0);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 AquaModel *m=game_aqua_model(game_port_game(p));assert(m&&m->phase==AQUA_PLAYING);
 int tx=-1,ty=-1;for(unsigned y=2;y<m->height&&tx<0;y++)for(unsigned x=0;x<m->width&&tx<0;x++){unsigned t=m->map[y*m->width+x];if((t>=2&&t<=39)||t==1200){tx=(int)x;ty=(int)y;}}
 assert(tx>=0);m->x=tx*80+40;m->y=ty*80-80;m->grace=999;
 tick(p,4,(GameInput){.held=GAME_KEY_A});
 tick(p,2,(GameInput){0});
assert(m->cargo>0);
 assert(render(level)>0&&render(sub)>0);
 m->treasure=5;const AquaLevelData *data=aqua_model_level(m);m->x=data->netX;m->y=data->netY;m->grace=999;
 tick(p,1,(GameInput){0});assert(m->phase==AQUA_DONE);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});tick(p,2,(GameInput){0});
 assert(m->level==1);assert(f.loads>=3&&!strcmp(f.loaded[2],"games/aqua2.gpk"));
 level=game_bank_scene(game_port_bank(p,0));assert(level&&render(level)>0);
 m->treasure=5;data=aqua_model_level(m);m->x=data->netX;m->y=data->netY;m->grace=999;
 tick(p,1,(GameInput){0});assert(m->phase==AQUA_DONE);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});tick(p,2,(GameInput){0});assert(m->phase==AQUA_WON);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 game_port_free(p);
 puts("PASS Aqua Grabber native controller: baked level banks, claw cargo, two levels, one payout and return");
 return 0;
}
