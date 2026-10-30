#include "demo.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
static int direction(float x,float y){int d=(int)lroundf((atan2f(y,x)*180.0f/3.14159265f-90.0f)/45.0f);return (d%8+8)%8+1;}
_Static_assert(CAST_COUNT==ACTOR_INDEX_CAPACITY,"Actor index must cover the complete room roster");
void demo_index(Demo *d){
 unsigned i=0,count=d->count<CAST_COUNT?d->count:CAST_COUNT;if(d->actorIndex.count==count){while(i<count&&d->actorIndex.ids[i]==d->actors[i].id)i++;if(i==count)return;}
 unsigned ids[CAST_COUNT];for(i=0;i<count;i++)ids[i]=d->actors[i].id;actor_index_build(&d->actorIndex,ids,count);
}
const Actor *demo_actor(const Demo *d,unsigned id){
 int index=actor_index_find(&d->actorIndex,id);if(index>=0&&(unsigned)index<d->count&&d->actors[index].id==id)return &d->actors[index];
 // A packet can replace or compact the roster before the next frame rebuilds its index.
 for(unsigned i=0;i<d->count&&i<CAST_COUNT;i++)if(d->actors[i].id==id)return &d->actors[i];
 return NULL;
}
void demo_init(Demo *d,uint8_t *mask){
 memset(d,0,sizeof *d);d->mask=mask;
 d->count=1;d->actors[0]=(Actor){.x=380,.y=340,.tx=380,.ty=340,.direction=1,.action=1,.colour=12};
 strcpy(d->actors[0].name,"Penguin");
}
bool demo_valid(const Demo *d,float x,float y){if(x<20||x>740||y<20||y>430)return false;if(d->blocked)return !d->blocked(d->room,(int)lroundf(x),(int)lroundf(y));return !d->mask||d->mask[(int)lroundf(y)*ROOM_W+(int)lroundf(x)]==0;}
void demo_target(Demo *d,float x,float y){
 Actor *a=&d->actors[0];float dx=x-a->x,dy=y-a->y,n=hypotf(dx,dy);if(n<1)return;
 float tx=a->x,ty=a->y;
 for(float t=1;t<=n;t+=1){float nx=a->x+dx*t/n,ny=a->y+dy*t/n;if(!demo_valid(d,nx,ny))break;tx=nx;ty=ny;}
 a->tx=tx;a->ty=ty;a->walking=hypotf(tx-a->x,ty-a->y)>1;
 if(a->walking){a->direction=direction(dx,dy);a->action=a->direction+8;a->clock=0;}
}
void demo_action(Demo *d,int action){Actor *a=&d->actors[0];a->walking=false;a->action=action==17?a->direction+16:action;a->clock=0;}
void demo_say(Demo *d,int phrase){d->phrase=phrase;d->bubbleTime=5;}
void actor_say(Actor *a,const char *text){snprintf(a->chat,sizeof a->chat,"%.511s",text);a->chatOffset=0;a->bubbleTime=125.0f/24;a->emote=0;}
void demo_throw(Demo *d){Actor *a=&d->actors[0];demo_action(d,30);d->ballX=a->x;d->ballY=a->y-20;d->ballTX=a->x+100;d->ballTY=a->y-25;d->ballTime=1.2f;}
void demo_step(Demo *d,float dt,float dx,float dy){
 if(dt<0)return;
 if(dt>.1f)dt=.1f;
 d->time+=dt;if(d->bubbleTime>0)d->bubbleTime-=dt;if(d->ballTime>0)d->ballTime-=dt;
 Actor *a=&d->actors[0];float mag=hypotf(dx,dy);
 if(mag>.15f){float nx=dx/mag,ny=dy/mag;a->direction=direction(nx,ny);a->action=a->direction+8;a->walking=false;
  for(float step=0;step<96*dt;step+=1){float inc=fminf(1,96*dt-step);if(demo_valid(d,a->x+nx*inc,a->y+ny*inc)){a->x+=nx*inc;a->y+=ny*inc;}}
 }else if(a->walking){float x=a->tx-a->x,y=a->ty-a->y,n=hypotf(x,y),step=96*dt;
  if(n<=step){a->x=a->tx;a->y=a->ty;a->walking=false;a->action=a->direction;}else{a->x+=x/n*step;a->y+=y/n*step;}
 }else if(a->action>=9&&a->action<=16)a->action=a->direction;
 for(unsigned i=0;i<d->count;i++){Actor *p=&d->actors[i];
  if(i&&p->walking){float dx=p->tx-p->x,dy=p->ty-p->y,n=hypotf(dx,dy),step=96*dt;if(n<=step){p->x=p->tx;p->y=p->ty;p->walking=false;p->action=p->pendingFrame?(int)p->pendingFrame:p->direction;if(p->pendingFrame)p->clock=0;p->pendingFrame=0;}else{p->x+=dx/n*step;p->y+=dy/n*step;}}
  p->clock+=dt*24;if(p->bubbleTime>0){p->bubbleTime-=dt;if(p->bubbleTime<=0&&!p->emote){const char *next=strchr(p->chat+p->chatOffset,'|');if(next){p->chatOffset=(unsigned)(next-p->chat)+1;p->bubbleTime+=125.0f/24;}}}
  if((p->action==25&&p->clock>=32)||(p->action>=27&&p->action<=30&&p->clock>=28)){p->action=p->direction;p->clock=0;}}
}
