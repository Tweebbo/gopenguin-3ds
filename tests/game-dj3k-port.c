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
static void tap_art(GamePort *p,float x,float y){SWLMatrix view;assert(game_port_controller(p)->lower_view(game_port_game(p),&view));game_port_draw_bottom(p);x=x*view.a+view.x;y=y*view.d+view.y;assert(x>=0&&x<320&&y>=24&&y<240);game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);}

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
 size_t textures=game_port_texture_bytes(p);SWLMatrix view;assert(game_port_controller(p)->lower_view(game_port_game(p),&view));unsigned draws=0;assert(swl_render(scene,view,-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws)&&draws>0);
 /* Each original control reaches its model without registering a replacement grid. */
 for(unsigned i=0;i<6;i++){char path[96];SWLInstance pose;snprintf(path,sizeof path,"root.gameMC.rackmountedDrumLoops.squareButton%u",i+1);assert(swl_pose(scene,path,&pose));int before=m->drum;tap_art(p,pose.matrix.x+15.75f*pose.matrix.a,pose.matrix.y+15.75f*pose.matrix.d);assert(m->drum==(before==(int)i?-1:(int)i));tick(p,1);}
 const float bass[][2]={{66,134},{118,141},{168,148}},melody[][2]={{86,321},{107,316},{127,312},{145,307},{161,303}};
 for(unsigned i=0;i<3;i++){int before=m->bass;tap_art(p,bass[i][0],bass[i][1]);assert(m->bass==(before==(int)i?-1:(int)i));tick(p,1);}
 const float effects[][2]={{73,26},{105,32},{136,37},{166,42},{69,60},{99,66},{131,70},{161,75}};
 for(unsigned i=0;i<8;i++){tap_art(p,effects[i][0],effects[i][1]);assert(m->soundCount==1&&!strcmp(m->sounds[0],dj3k_effect_name(i)));tick(p,1);}
 /* Page "Keys" (R or X): melody sliders and the keyboard, zoomed. */
 game_port_update(p,0,&(GameInput){.down=GAME_KEY_R});
 for(unsigned i=0;i<5;i++){bool before=m->melody[i];tap_art(p,melody[i][0],melody[i][1]);assert(m->melody[i]!=before);tick(p,1);}
 const float keys[][2]={{20,460},{40,430},{54,460},{75,430},{88,460},{122,460},{146,430},{155,460},{179,430},{190,460},{217,430},{223,460},{258,460}};
 for(unsigned i=0;i<13;i++){tap_art(p,keys[i][0],keys[i][1]);assert(m->soundCount==1&&!strcmp(m->sounds[0],dj3k_note_name(i)));tick(p,1);}
 assert(game_port_controller(p)->lower_view(game_port_game(p),&view));
 game_port_draw_bottom(p);float y=460*view.d+view.y,x=20*view.a+view.x;
 game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_MOVE,x,y);assert(m->soundCount==1);
 game_port_touch(p,GAME_TOUCH_MOVE,54*view.a+view.x,y);assert(m->soundCount==2&&!strcmp(m->sounds[1],dj3k_note_name(2)));
 game_port_touch(p,GAME_TOUCH_UP,54*view.a+view.x,y);tick(p,1);
 /* Page "Sounds" (X): the speaker's five sound buttons. */
 game_port_update(p,0,&(GameInput){.down=GAME_KEY_X});
 const float speakers[][2]={{620,311},{648,323},{678,336},{708,350},{739,364}};
 for(unsigned i=0;i<5;i++){tap_art(p,speakers[i][0],speakers[i][1]);assert(m->soundCount==1&&!strcmp(m->sounds[0],dj3k_speaker_name(i)));tick(p,1);}
 /* L goes back to "Keys". */
 game_port_update(p,0,&(GameInput){.down=GAME_KEY_L});assert(game_port_controller(p)->lower_view(game_port_game(p),&view)&&view.a>.85f);
 assert(game_port_texture_bytes(p)==textures);
 game_port_close(p);assert(f.left==1);
 game_port_free(p);
 puts("PASS DJ3K native controller: three zoomed deck pages (L/R/X), 40 authored touch controls, unchanged texture memory, loop and sound routing, exit");
 return 0;
}
