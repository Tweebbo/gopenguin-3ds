#include "../source/puffles.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 for(unsigned direction=0;direction<8;direction++){
  Demo world={.count=1};world.actors[0]=(Actor){.id=7,.x=380,.y=200,.action=9+direction};
  Puffles pets;puffles_init(&pets,NULL,NULL);pets.mine=7;pets.count=1;pets.pets[0]=(Puffle){.owner=7,.id=1,.walking=true,.direction=1,.frame=1,.present=true,.x=360,.y=208};Puffle *p=&pets.pets[0];unsigned first=0;
  for(unsigned i=0;i<80;i++){
   world.actors[0].x+=cosf(direction*3.141592653589793f/4)*4;world.actors[0].y+=sinf(direction*3.141592653589793f/4)*4;float x=p->x,y=p->y;
   puffles_step(&pets,&world,1.0f/24);assert(hypotf(p->x-x,p->y-y)<=6.001f);assert(p->frame>=9&&p->frame<=16);if(!first)first=p->frame;assert(p->frame==first);
  }
  assert(p->clock>=79);world.actors[0].action=1;for(unsigned i=0;i<12;i++)puffles_step(&pets,&world,1.0f/24);assert(p->frame==p->direction);unsigned idle=p->frame;
  for(unsigned i=0;i<180;i++){world.actors[0].x+=i%2?.5f:-.5f;puffles_step(&pets,&world,1.0f/24);assert(p->frame==idle);}
 }
 puts("PASS eight native puffle walk directions, uninterrupted animation clocks, settled facing and stationary noise");
}
