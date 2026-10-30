#include "../source/demo.h"
#include "../source/animations.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
 Demo a,b;demo_init(&a,NULL);demo_init(&b,NULL);
 for(int i=0;i<60;i++)demo_step(&a,1.0f/60,1,0);
 for(int i=0;i<30;i++)demo_step(&b,1.0f/30,1,0);
 assert(fabsf(a.actors[0].x-b.actors[0].x)<.02f);assert(fabsf(a.actors[0].x-476)<.02f);
 demo_init(&a,NULL);for(int i=0;i<60;i++)demo_step(&a,1.0f/60,1,1);
 assert(fabsf(hypotf(a.actors[0].x-380,a.actors[0].y-340)-96)<.1f);
 uint8_t *mask=calloc(ROOM_W*ROOM_H,1);assert(mask);for(int y=0;y<ROOM_H;y++)mask[y*ROOM_W+420]=255;
 demo_init(&a,mask);demo_target(&a,500,340);for(int i=0;i<120;i++)demo_step(&a,1.0f/60,0,0);assert(a.actors[0].x<420);assert(!a.actors[0].walking);
 demo_target(&a,300,340);demo_action(&a,26);float x=a.actors[0].x;for(int i=0;i<60;i++)demo_step(&a,1.0f/60,0,0);assert(a.actors[0].x==x);assert(fabsf(a.actors[0].clock-24)<.01f);
 demo_action(&a,25);for(int i=0;i<90;i++)demo_step(&a,1.0f/60,0,0);assert(a.actors[0].action==a.actors[0].direction);
 demo_throw(&a);assert(a.ballTX>a.ballX);assert(a.actors[0].action==30);assert(!a.actors[0].walking);
 for(int i=0;i<90;i++)demo_step(&a,1.0f/60,0,0);
 assert(a.actors[0].action==a.actors[0].direction);assert(a.ballTime<=0);
 for(int action=1;action<=30;action++)assert(sequences[action].count>0);
 assert(sequences[26].count==193);assert(!demo_valid(&a,-1,20));assert(!demo_valid(&a,740,440));
 free(mask);puts("PASS: frame-rate independent movement, collision, cancellation, timing, all 30 poses/actions");
}
