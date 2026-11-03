#include "game_controller.h"
#include "game_dj3k.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,left;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"926"));f->join++;}else assert(false);return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

int main(void){
 Fixture f={0};GameDescriptor descriptor={.key="dj3k",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 char *join[]={"926"};assert(game_port_packet(p,"jg",1,join));assert(f.join==1);
 assert(game_port_session(p)==GAME_SESSION_PLAYING);
 SWLScene *scene=game_bank_scene(game_port_bank(p,0));assert(scene);assert(render(scene)>0);
 Dj3kModel *m=game_dj3k_model(game_port_game(p));assert(m);
 dj3k_toggle_loop(m,DJ3K_DRUM,0);tick(p,1);
 assert(swl_frame(scene,"root.gameMC.rackmountedDrumLoops.squareButton1")==3);
 dj3k_toggle_loop(m,DJ3K_BASS,2);tick(p,1);
 assert(swl_frame(scene,"root.gameMC.rackmountedBassLines.roundButton3")==3);
 dj3k_toggle_loop(m,DJ3K_MELODY,1);tick(p,1);
 assert(swl_frame(scene,"root.gameMC.rackmountedMelodyLoops.slider2")==2);
 dj3k_effect(m,0);dj3k_speaker(m,4);dj3k_note(m,12);tick(p,1);
 assert(m->soundCount==0);assert(render(scene)>0);
 game_port_close(p);assert(f.left==1);
 game_port_free(p);
 puts("PASS DJ3K native controller: bank renders, drum/bass exclusivity, melody layer, one-shot queue, exit");
 return 0;
}
