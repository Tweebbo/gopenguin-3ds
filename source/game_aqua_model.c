#include "game_aqua_model.h"
#include <math.h>
#include <string.h>

#define AQUA_SPEED 3.0
#define AQUA_LIVES 2

static bool solid(unsigned tile){return (tile>=500&&tile<=689)||(tile>=1400&&tile<=1422);}
static bool collectible(unsigned tile){return (tile>=2&&tile<=39)||(tile>=1200&&tile<=1201);}
static unsigned tile_at(const AquaModel *m,double x,double y){
 int tx=(int)(x/AQUA_TILE),ty=(int)(y/AQUA_TILE);
 if(tx<0||ty<0||(unsigned)tx>=m->width||(unsigned)ty>=m->height)return 1;
 return m->map[ty*m->width+tx];
}
void aqua_model_init(AquaModel *m){memset(m,0,sizeof *m);m->phase=AQUA_PLAYING;}
void aqua_model_start(AquaModel *m){m->level=0;m->score=0;m->lives=AQUA_LIVES;aqua_model_load(m);}
void aqua_model_load(AquaModel *m){
 const AquaLevelData *l=aqua_model_level(m);m->width=l->width;m->height=l->height;
 memcpy(m->map,l->map,(size_t)l->width*l->height*sizeof(uint16_t));
 m->x=l->startX;m->y=l->startY;m->air=100;m->treasure=0;m->cargo=0;m->claw=0;m->time=l->time;m->grace=20;m->puffleX=m->x-40;m->puffleY=m->y+10;m->pufflePinched=false;m->phase=AQUA_PLAYING;
}
static bool collides(const AquaModel *m){
 static const int offsets[4][2]={{-14,0},{14,0},{0,-14},{0,14}};
 for(unsigned i=0;i<4;i++)if(solid(tile_at(m,m->x+offsets[i][0],m->y+offsets[i][1])))return true;
 return false;
}
static void collect(AquaModel *m){
 int tx=(int)(m->x/AQUA_TILE),ty=(int)(m->y/AQUA_TILE);
 if(tx<0||ty<0||(unsigned)tx>=m->width||(unsigned)ty>=m->height)return;
 unsigned tile=m->map[ty*m->width+tx];
 if(collectible(tile)){m->treasure++;m->score+=5;m->map[ty*m->width+tx]=1;}
 else if(tile>=2000){(void)0;}
}
static void grab(AquaModel *m){
 for(int dy=80;dy<=260;dy+=60)for(int dx=-40;dx<=40;dx+=40){
  double x=m->x+dx,y=m->y+dy;unsigned tile=tile_at(m,x,y);
  int tx=(int)(x/AQUA_TILE),ty=(int)(y/AQUA_TILE);
  if(tx<0||ty<0||(unsigned)tx>=m->width||(unsigned)ty>=m->height)continue;
  if(collectible(tile)){m->cargo++;m->map[ty*m->width+tx]=1;if(!m->pufflePinched&&hypot(m->puffleX-m->x,m->puffleY-(m->y+80+m->claw*50))<220)m->pufflePinched=true;return;}
 }
}
static void hurt(AquaModel *m,bool dry){
 if(dry||m->air<=0){
  if(m->lives)m->lives--;
  if(m->lives==0)m->phase=AQUA_OVER;
  else{const AquaLevelData *l=aqua_model_level(m);m->x=l->startX;m->y=l->startY;m->air=100;m->grace=20;m->pufflePinched=false;m->puffleX=m->x-40;m->puffleY=m->y+10;}
 }else{
  m->air-=25;if(m->air<0)m->air=0;m->x-=10;m->y+=10;m->grace=12;
 }
}
void aqua_model_step(AquaModel *m,int dx,int dy,bool action){
 if(m->phase!=AQUA_PLAYING)return;
 m->x+=dx*AQUA_SPEED;m->y+=dy*AQUA_SPEED;
 if(m->x<20)m->x=20;
 if(m->y<20)m->y=20;
 if(m->x>m->width*AQUA_TILE-20)m->x=m->width*AQUA_TILE-20;
 if(m->y>m->height*AQUA_TILE-20)m->y=m->height*AQUA_TILE-20;
 if(action)m->claw=m->claw<3?m->claw+1:3;
 else if(m->claw){grab(m);m->claw=0;}
 if(m->pufflePinched){m->puffleX=m->x;m->puffleY=m->y-10;}else{double dx=m->x-40-m->puffleX,dy=m->y+10-m->puffleY;if(hypot(dx,dy)>2){m->puffleX+=dx*0.08;m->puffleY+=dy*0.08;}}
 m->air-=0.05;if(m->air<0)m->air=0;
 if(m->y<150||tile_at(m,m->x,m->y)==2){m->air+=1;if(m->air>100)m->air=100;}
 if(m->grace)m->grace--;
 collect(m);
 if(m->grace==0&&collides(m))hurt(m,false);
 if(m->air<=0)hurt(m,true);
 if(m->time)m->time--;
 if(!m->time)m->phase=AQUA_OVER;
 const AquaLevelData *l=aqua_model_level(m);
 if(m->treasure>=5&&hypot(m->x-l->netX,m->y-l->netY)<120){m->score+=m->treasure*5;m->phase=AQUA_DONE;}
}
void aqua_model_next(AquaModel *m){
 if(m->phase!=AQUA_DONE)return;
 if(m->level+1>=AQUA_LEVELS){m->phase=AQUA_WON;return;}
 m->level++;aqua_model_load(m);
}
const AquaLevelData *aqua_model_level(const AquaModel *m){return &AQUA_LEVEL_DATA[m->level];}
unsigned aqua_model_total(const AquaModel *m){return m->score+(m->phase==AQUA_PLAYING?m->treasure*5:0);}
