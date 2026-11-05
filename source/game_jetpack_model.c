#include "game_jetpack_model.h"
#include <math.h>
#include <string.h>

#define TILE 80.0
#define GRAVITY 4.0
#define MAX_UP 14.0

static unsigned tile_at(const JetpackModel *m,double x,double y){
 int tx=(int)(x/TILE),ty=(int)(y/TILE);
 if(tx<0||ty<0||(unsigned)tx>=m->width||(unsigned)ty>=m->height)return 1;
 return m->map[ty*m->width+tx];
}
static bool solid(unsigned tile){return tile>=15&&tile!=28&&tile!=29;}
static bool landing(unsigned tile){return tile==28||tile==29;}
static void collect(JetpackModel *m){
 int tx=(int)(m->x/TILE),ty=(int)(m->y/TILE);
 if(tx<0||ty<0||(unsigned)tx>=m->width||(unsigned)ty>=m->height)return;
 unsigned *tile=&m->map[ty*m->width+tx];
 if(*tile==2){if(m->fuel<100)m->fuel+=50;if(m->fuel>100)m->fuel=100;*tile=1;}
 else if(*tile==3||*tile==10||*tile==11){m->coins++;*tile=1;}
 else if(*tile==4){m->coins+=10;*tile=1;}
 else if(*tile==5){m->lives++;*tile=1;}
}
static void reset_position(JetpackModel *m){
 const JetpackLevelData *l=jetpack_model_level(m);
 m->x=l->startX;m->y=l->startY;m->rotation=0;m->upSpeed=0;m->fuel=100;m->grace=15;m->landed=true;
}
void jetpack_model_init(JetpackModel *m){memset(m,0,sizeof *m);m->phase=JETPACK_PLAYING;m->lives=3;}
void jetpack_model_start(JetpackModel *m){m->level=0;m->lives=3;m->score=0;m->levelScore=0;jetpack_model_load(m);}
void jetpack_model_load(JetpackModel *m){
 const JetpackLevelData *l=jetpack_model_level(m);
 m->width=l->width;m->height=l->height;
 /* Level tiles are bytes; the model map holds one unsigned per cell (a byte memcpy packed four cells into each). */
 for(size_t i=0;i<(size_t)l->width*l->height;i++)m->map[i]=l->tiles[i];
 m->coins=0;m->phase=JETPACK_PLAYING;m->levelScore=0;reset_position(m);
}
void jetpack_model_next(JetpackModel *m){
 if(m->phase!=JETPACK_DONE)return;
 m->score+=m->levelScore;
 if(m->level+1>=JETPACK_LEVELS){m->phase=JETPACK_WON;return;}
 m->level++;jetpack_model_load(m);
}
void jetpack_model_reset(JetpackModel *m){if(m->phase==JETPACK_OVER||m->phase==JETPACK_PLAYING)reset_position(m);}
static void hurt(JetpackModel *m){
 if(m->fuel>25)m->fuel-=25;else m->fuel=0;
 m->grace=10;
 if(m->fuel<=0){if(m->lives>0)m->lives--;if(m->lives==0)m->phase=JETPACK_OVER;else jetpack_model_reset(m);}
 else{m->y+=20;m->x-=10;}
}
void jetpack_model_step(JetpackModel *m,bool thrust,bool left,bool right,bool down){
 if(m->phase!=JETPACK_PLAYING)return;
 thrust=thrust&&!down&&m->fuel>0;
 if(m->landed&&!thrust){
  m->fuel-=0.06;if(m->fuel<0)m->fuel=0;m->rotation*=0.9;collect(m);
  unsigned t=tile_at(m,m->x,m->y);
  if(landing(t)){m->levelScore+=(int)(m->coins+m->fuel/2);m->phase=JETPACK_DONE;}
  return;
 }
 if(thrust){m->landed=false;m->upSpeed+=1;if(m->upSpeed>MAX_UP)m->upSpeed=MAX_UP;m->fuel-=0.14;if(m->fuel<0)m->fuel=0;}
 else{m->upSpeed-=2;if(m->upSpeed<0)m->upSpeed=0;m->fuel-=0.06;if(m->fuel<0)m->fuel=0;}
 if(left&&!right)m->rotation-=4;else if(right&&!left)m->rotation+=4;else m->rotation*=0.9;
 if(m->rotation<-50)m->rotation=-50;
 if(m->rotation>50)m->rotation=50;
 unsigned wind_tile=tile_at(m,m->x,m->y);double wind=(wind_tile==6||wind_tile==10)?-4:(wind_tile==7||wind_tile==11)?4:0;
 double width=m->width*TILE;
 m->x+=m->rotation/4+wind;if(m->x<25)m->x=25;if(m->x>width-25)m->x=width-25;
 m->y+=GRAVITY-m->upSpeed;if(m->y<90){m->y=90;if(m->upSpeed>0)m->upSpeed=0;}
 if(m->grace>0)m->grace--;
 collect(m);
 unsigned t=tile_at(m,m->x,m->y);
 if(landing(t)){m->levelScore+=(int)(m->coins+m->fuel/2);m->phase=JETPACK_DONE;return;}
 if(m->grace==0&&(solid(tile_at(m,m->x-8,m->y))||solid(tile_at(m,m->x+8,m->y))||solid(tile_at(m,m->x,m->y-8))||solid(tile_at(m,m->x,m->y+8))))hurt(m);
 if(m->fuel<=0&&m->y>(double)m->height*TILE+250){if(m->lives>0)m->lives--;if(m->lives==0)m->phase=JETPACK_OVER;else jetpack_model_reset(m);}
}
const JetpackLevelData *jetpack_model_level(const JetpackModel *m){return &JETPACK_LEVEL_DATA[m->level];}
unsigned jetpack_model_total(const JetpackModel *m){return m->score+m->levelScore;}
