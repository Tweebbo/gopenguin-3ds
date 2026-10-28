#include "game_roundup_model.h"
#include "game_roundup_data.h"
#include <math.h>
#include <string.h>

enum {MASK_W=760,MASK_H=480};
static uint8_t wallBits[MASK_W*MASK_H/8],penBits[MASK_W*MASK_H/8];
static bool decoded;
static void decode(const uint32_t *runs,unsigned count,uint8_t *bits){
 unsigned at=0;for(unsigned i=0;i<count;i++){if(i&1)for(unsigned k=0;k<runs[i]&&at+k<MASK_W*MASK_H;k++)bits[(at+k)>>3]|=(uint8_t)(1u<<((at+k)&7));at+=runs[i];}
}
static double js_round(double v){return floor(v+.5);}
/** StageMask.hit: the nearest stage pixel, misses outside the stage. */
static bool hit(const uint8_t *bits,double x,double y){
 if(!decoded){decode(ROUNDUP_WALL,ROUNDUP_WALL_RUNS,wallBits);decode(ROUNDUP_PEN,ROUNDUP_PEN_RUNS,penBits);decoded=true;}
 double px=js_round(x),py=js_round(y);if(!(px>=0&&py>=0&&px<MASK_W&&py<MASK_H))return false;
 unsigned i=(unsigned)py*MASK_W+(unsigned)px;return bits[i>>3]>>(i&7)&1;
}
bool roundup_wall(double x,double y){return hit(wallBits,x+80.3,y+52);}
bool roundup_pen(double x,double y){return hit(penBits,x+80.3,y+52);}

void roundup_model_init(RoundupModel *m,const unsigned types[ROUNDUP_HERD],const double positions[ROUNDUP_HERD][2]){
 memset(m,0,sizeof *m);m->remaining=120;
 for(unsigned i=0;i<ROUNDUP_HERD;i++)m->pets[i]=(RoundupPet){.type=types[i]<ROUNDUP_PETS?types[i]:0,.direction=1,.x=positions[i][0],.y=positions[i][1],.oldX=positions[i][0],.oldY=positions[i][1]};
}

void roundup_model_step(RoundupModel *m,double mouseX,double mouseY){
 if(m->done)return;
 m->elapsed+=1000.0/24;
 for(unsigned i=0;i<ROUNDUP_HERD;i++){
  RoundupPet *p=&m->pets[i];p->oldX=p->x;p->oldY=p->y;p->moving=false;if(p->escaped)continue;
  double distance=hypot(mouseX-p->x,mouseY-p->y),reach=ROUNDUP_PET_RULES[p->type].distance,speed=ROUNDUP_PET_RULES[p->type].speed;
  if(distance<reach&&distance>0){
   double dx=(mouseX-p->x)*speed/distance,dy=(mouseY-p->y)*speed/distance,x=p->x-dx,y=p->y-dy;
   double angle=trunc(atan2(p->y-mouseY,p->x-mouseX)*180/3.141592653589793-90);if(angle<0)angle+=360;
   p->direction=(unsigned)js_round(angle/45)%8+1;p->moving=true;
   if(roundup_wall(x,y)){x=p->x+dx*1.5;y=p->y+dy*1.5;}
   p->captured=roundup_pen(x,y);p->x=x;p->y=y;
   if(x<0||x>600||y<0||y>400)p->escaped=true;
  }
 }
 double spent=floor((m->elapsed+1e-6)/500);m->remaining=spent>=120?0:120-(unsigned)spent;
 m->captured=m->escaped=0;for(unsigned i=0;i<ROUNDUP_HERD;i++){m->captured+=m->pets[i].captured;m->escaped+=m->pets[i].escaped;}
 m->done=m->captured+m->escaped==ROUNDUP_HERD||!m->remaining;
 if(m->done)m->coins=m->remaining*m->captured/10;
}

void roundup_model_herd(double (*random)(void *),void *user,unsigned types[ROUNDUP_HERD],unsigned *map){
 // Web startRound(): map round(random*9+1) clamped by Movie.goto, level floor(random*5), then a shuffle.
 double m=js_round(random(user)*9+1);*map=(unsigned)(m>ROUNDUP_MAPS?ROUNDUP_MAPS:m)-1;
 unsigned level=(unsigned)floor(random(user)*5);if(level>=ROUNDUP_LEVELS)level=ROUNDUP_LEVELS-1;
 for(unsigned i=0;i<ROUNDUP_HERD;i++)types[i]=ROUNDUP_LEVEL_TYPES[level][i];
 for(unsigned i=ROUNDUP_HERD-1;i>0;i--){unsigned j=(unsigned)floor(random(user)*(i+1));unsigned t=types[i];types[i]=types[j];types[j]=t;}
}
