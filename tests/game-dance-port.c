#include "game_controller.h"
#include "game_dance.h"
#include "game_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,coins,left;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"952"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;}else assert(false);return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

int main(void){
 Fixture f={0};GameDescriptor descriptor={.key="dance",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 GameHost host={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 char *join[]={"952"};assert(game_port_packet(p,"jg",1,join));assert(f.join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 SWLScene *scene=game_bank_scene(game_port_bank(p,0));assert(scene);assert(render(scene)>0);
 assert(swl_visible(scene,"root.background",true));assert(swl_visible(scene,"root.arrows",true));
 DanceModel *m=game_dance_model(game_port_game(p));assert(m);

 /* Start a chart from the menu, then hit the first note exactly and check scoring. */
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(m->song==0&&m->difficulty==1);
 tick(p,1);assert(m->phase==DANCE_PLAYING);
 const DanceChartData *chart=dance_model_chart(m);assert(chart->count>10);
 double first=chart->notes[0].time;
 m->position=first;assert(dance_model_hit(m,chart->notes[0].dir)==0);
 assert(m->combo==1&&m->score==300&&m->judged==1&&m->counts[0]==1);
 assert(dance_model_hit(m,chart->notes[0].dir)==-1);
 const char *paths[]={"root.upNote0","root.downNote0","root.leftNote0","root.rightNote0"};
 for(unsigned i=0;i<4;i++)assert(swl_visible(scene,paths[i],false));
 /* A miss clears the combo and a later note can be hit again. */
 m->position=chart->notes[1].time+900;dance_model_update(m,m->position);
 assert(m->combo==0&&m->counts[4]>=1);
 assert(m->judged>=2);
 m->position=chart->notes[chart->count-1].time;
 int grade=dance_model_hit(m,chart->notes[chart->count-1].dir);assert(grade==0);
 game_port_update(p,1.0f/24,&(GameInput){0});
 m->position=m->duration+1;dance_model_update(m,m->duration+1);
 game_port_update(p,1.0f/24,&(GameInput){0});
 assert(m->phase==DANCE_RESULT);
 unsigned expected=dance_model_coins(m);assert(expected>=10&&expected<=120);
 game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,230,138);game_port_touch(p,GAME_TOUCH_UP,230,138);
 game_port_update(p,1.0f/24,&(GameInput){0});
 assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 game_port_free(p);
 puts("PASS Dance Contest native controller: bank renders, chart starts, exact hit, miss, result and authoritative payout");
 return 0;
}
