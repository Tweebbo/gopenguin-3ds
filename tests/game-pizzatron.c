#include "../source/game_pizzatron_model.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t seed=0x12345678;
static double random01(void *unused){(void)unused;seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return (double)seed/4294967296.0;}
int main(int argc,char **argv){
 PizzModel m;pizz_model_init(&m,argc>1&&atoi(argv[1]),random01,NULL);
 double x,y;unsigned held,pickup;
 while(scanf("%lf %lf %u %u",&x,&y,&held,&pickup)==4){
  assert(isfinite(x)&&isfinite(y)&&fabs(x)<10000&&fabs(y)<10000&&pickup<8&&held<2);
  m.mouseX=x;m.mouseY=y;m.held=held;if(pickup)pizz_model_pickup(&m,pickup);pizz_model_step(&m);
  unsigned splats=0,toppings=0;double art=0,drop=0;
  for(unsigned i=0;i<PIZZ_SAUCE_SLOTS;i++)if(m.splats[i].active){PizzSplat *s=&m.splats[i];splats++;art+=s->x+3*s->y+7*s->kind+11*s->variant+13*s->age+17*s->slot;}
  for(unsigned i=0;i<PIZZ_TOPPING_SLOTS;i++)if(m.toppings[i].active){PizzTopping *t=&m.toppings[i];toppings++;art+=t->x+3*t->y+7*t->kind+11*t->variant+17*t->slot;}
  for(unsigned i=0;i<PIZZ_DROP_SLOTS;i++)if(m.drops[i].active){PizzDrop *d=&m.drops[i];drop+=d->x+3*d->y+7*d->vx+11*d->vy+13*d->rotation;}
  printf("%.9f %.9f %.9f %u %u %u %u %u %u %d %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %.9f %.9f\n",m.x,m.speed,m.boost,m.phase,m.order,m.made,m.ordersMade,m.failures,m.score,m.handicap,m.tips,m.consecutive,m.normal,m.hot,m.cheese,m.perfect,m.ingredient,m.variant,m.result,m.splatCount,m.toppingCount,m.serial,m.counts[0],m.counts[1],m.counts[2],m.counts[3],splats+toppings,art,drop);
 }
 assert(feof(stdin));return 0;
}
