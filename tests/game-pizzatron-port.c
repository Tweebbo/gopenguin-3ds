#include "game_controller.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,score,left,coins;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3&&!strcmp(args[0],"910"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);f->zo++;f->score=(unsigned)strtoul(args[0],NULL,10);}else assert(false);
 return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==330&&x==180&&y==220);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static void check_views(GamePort *p){
 const GameController *controller=game_port_controller(p);SWLScene *scene=game_bank_scene(game_port_bank(p,0));unsigned before=swl_frame(scene,"root");SWLMatrix view;assert(controller->lower_view(game_port_game(p),&view));unsigned draws=0;
 bool rendered=swl_render(scene,(SWLMatrix){.5f,0,0,.5f,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);
 if(!rendered)fprintf(stderr,"Pizzatron frame %u render: %s\n",before,swl_error(scene));
 assert(rendered);assert(draws>10);
 assert(swl_view_frame(scene,"root",controller->lower_frame(game_port_game(p),0)));draws=0;assert(swl_render(scene,view,-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws));assert(draws>1);assert(swl_frame(scene,"root")==before);
 assert(swl_view_frame(scene,"root",0));assert(swl_prepare(scene));assert(swl_frame(scene,"root")==before);assert(!swl_error(scene));
}
static GamePort *open_game(Fixture *f,bool online,bool candy){
 memset(f,0,sizeof *f);GameDescriptor descriptor={.key="pizzatron",.returnRoom=330,.returnX=180,.returnY=220,.colourRGB=0x0099ff,.online=online};GameHost host={.user=f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 if(online){assert(f->join==1);char *args[]={"910"};assert(game_port_packet(p,"jg",1,args));}
 assert(game_port_session(p)==GAME_SESSION_PLAYING);check_views(p);
 if(candy)game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_X});
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});assert(swl_frame(game_bank_scene(game_port_bank(p,0)),"root")==3);return p;
}
int main(void){
 for(unsigned candy=0;candy<2;candy++){
  Fixture fixture;GamePort *p=open_game(&fixture,true,candy);SWLScene *scene=game_bank_scene(game_port_bank(p,0));SWLInstance pizza;unsigned elapsed=0;
  do{tick(p,1);assert(swl_instance(scene,"root.pizza",&pizza));assert(++elapsed<300);}while(pizza.matrix.x<100);
  game_port_touch(p,GAME_TOUCH_DOWN,18,60);
  static const int points[6][2]={{0,-50},{0,50},{-90,0},{90,0},{20,0},{-20,0}};
  for(unsigned i=0;i<6;i++)for(unsigned age=0;age<5;age++){game_port_touch(p,GAME_TOUCH_MOVE,160+points[i][0]*.95f,164+points[i][1]*.95f);tick(p,1);check_views(p);}
  tick(p,10);assert(swl_frame(scene,"root.pizza.base")==((candy?4:2)));
  game_port_touch(p,GAME_TOUCH_UP,160,164);tick(p,1);
  game_port_touch(p,GAME_TOUCH_DOWN,112,60);game_port_touch(p,GAME_TOUCH_MOVE,160,164);game_port_touch(p,GAME_TOUCH_UP,160,164);tick(p,1);
  assert(swl_frame(scene,"root.pizza.cheese")==((candy?3:2)));
  do{tick(p,1);assert(++elapsed<1000);if(!swl_instance(scene,"root.pizza",&pizza)){assert(!swl_error(scene));break;}}while(pizza.matrix.x>0);
  game_port_close(p);assert(fixture.zo==1&&fixture.score==(candy?100u:50u));game_port_close(p);tick(p,30);assert(fixture.zo==1&&!fixture.left);
  char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(fixture.left==1&&fixture.coins==510);game_port_free(p);
 }
 Fixture fixture;GamePort *p=open_game(&fixture,false,false);game_port_close(p);assert(!fixture.zo&&!fixture.join&&fixture.left==1);game_port_free(p);
 puts("PASS Pizzatron native controller: original banks, two compositions without seeking gameplay, stylus sauce/cheese, both modes, score50/100, one zo and authoritative return, offline no payout");return 0;
}
