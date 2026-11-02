#include "../source/puffles.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned resident=1;
static bool ready(const Puffle *pet,unsigned frame){(void)pet;return frame==resident;}
static bool packet(Puffles *p,Demo *w,const char *command,unsigned count,char **args){PNetMessage m={.count=count,.args=args};snprintf(m.command,sizeof m.command,"%s",command);return puffles_apply(p,&m,w);}
static void step(Puffles *p,Demo *w,unsigned ticks){demo_index(w);for(unsigned i=0;i<ticks;i++)puffles_step(p,w,1.f/24);}
int main(void){
 static Demo w;static Puffles p;puffles_init(&p,NULL,NULL);w.count=2;w.actors[0]=(Actor){.id=10,.x=600,.y=150,.direction=1,.action=1};w.actors[1]=(Actor){.id=20,.x=120,.y=400,.direction=1,.action=1};puffles_room(&p,10,0);
 // A walking pet starts beside its owner in a new room, and a remote one too.
 char *mine[]={"10","1|Blue|0|100|100|100|100|100|100|0|0|0|1"},*other[]={"20","1|Pink|1|100|100|100|100|100|100|0|0|0|1"};
 assert(packet(&p,&w,"pw",2,mine));assert(packet(&p,&w,"pw",2,other));step(&p,&w,1);
 Puffle *blue=puffles_find(&p,10,1),*pink=puffles_find(&p,20,1);assert(hypotf(blue->x-580,blue->y-158)<.01f&&hypotf(pink->x-100,pink->y-408)<.01f);
 w.count=1;w.actors[0].x=200;w.actors[0].y=350;puffles_room(&p,10,0);step(&p,&w,1);assert(hypotf(blue->x-180,blue->y-358)<.01f);
 // A resting pet returns home in the igloo instead of keeping coordinates from outside.
 char *stop[]={"10","1|Blue|0|100|100|100|100|100|100|0|0|0|0"};assert(packet(&p,&w,"pw",2,stop));w.actors[0].x=700;w.actors[0].y=60;puffles_room(&p,10,10);step(&p,&w,1);assert(blue->x==280&&blue->y==300);
 // The old pose stays on screen until the next page is resident.
 assert(packet(&p,&w,"pw",2,mine));step(&p,&w,1);w.actors[0].x+=100;w.actors[0].walking=true;p.poseReady=ready;resident=1;blue->frame=1;step(&p,&w,5);assert(blue->frame==1&&blue->wanted>=9);resident=blue->wanted;step(&p,&w,1);assert(blue->frame>=9);
 // A lost reply must not disable the puffle buttons until reconnection.
 p.poseReady=NULL;static char sent;(void)sent;p.action=PUFFLE_WALK;p.actionAge=0;step(&p,&w,400);assert(p.action==PUFFLE_IDLE&&strstr(p.error,"timed out"));
 // The drawn position must always follow its own tick clock.
 p.accumulator=.5f/24;assert(fabsf(puffles_interpolation(&p)-.5f)<.001f);
 puts("PASS puffle arrival, igloo home, held poses, action timeout and interpolation");return 0;
}
