#include "game_thinice_model.h"
#include "game_thinice_data.h"
#include <math.h>
#include <string.h>

static const IcePoint DELTA[4]={{0,-1},{0,1},{-1,0},{1,0}};
static bool same(IcePoint a,IcePoint b){return a.x==b.x&&a.y==b.y;}
static IcePoint next(IcePoint p,IceDirection d){return (IcePoint){p.x+DELTA[d].x,p.y+DELTA[d].y};}
static IcePoint point(const int p[2]){return (IcePoint){p[0],p[1]};}
static const ThinIceLevelData *level(const ThinIceModel *m){return &THINICE_LEVEL_DATA[m->level>=1&&m->level<=THINICE_LEVELS?m->level-1:0];}
static void sound(ThinIceModel *m,IceSound s){if(m->soundCount<THINICE_MAX_SOUNDS)m->sounds[m->soundCount++]=s;}
unsigned thinice_model_tile(const ThinIceModel *m,IcePoint p){return p.x>=0&&p.y>=0&&p.x<THINICE_W&&p.y<THINICE_H?m->board[p.y][p.x]:20;}
static void set_tile(ThinIceModel *m,IcePoint p,unsigned v){if(p.x>=0&&p.y>=0&&p.x<THINICE_W&&p.y<THINICE_H)m->board[p.y][p.x]=(uint8_t)v;}
unsigned thinice_model_score(const ThinIceModel *m){return m->tiles+m->savedTiles+100*(m->bags+m->savedBags)+m->solvedPoints+m->firstTryPoints+m->timePoints;}
unsigned thinice_model_coins(const ThinIceModel *m){return thinice_model_score(m)/10;}
unsigned thinice_model_goal(const ThinIceModel *m){return level(m)->goal;}

static void load(ThinIceModel *m,bool newLevel){
 const ThinIceLevelData *l=level(m);
 if(newLevel)m->variant=(unsigned)floor(m->random(m->user)*l->maps)%l->maps;
 memcpy(m->board,THINICE_MAPS[l->first+m->variant],sizeof m->board);
 if(l->bonus[0]>=0&&m->lastSolved)set_tile(m,point(l->bonus),7);
 m->position=m->previous=m->start;
 m->hasBlock=l->block[0]>=0;if(m->hasBlock)m->block=m->previousBlock=point(l->block);
 m->warpCount=l->warpCount;for(unsigned i=0;i<l->warpCount;i++)m->warps[i]=point(l->warps[i]);
 m->warped=m->blockWarped=false;m->sliding=false;
 m->keys=m->tiles=m->bags=m->steps=m->blockSteps=m->phaseTick=0;
 m->phase=newLevel?ICE_ENTERING:ICE_PLAYING;
}
void thinice_model_init(ThinIceModel *m,IceRandom random,void *user){
 memset(m,0,sizeof *m);m->level=1;m->position=m->previous=m->start=(IcePoint){14,10};m->tries=1;m->countdown=1000;m->random=random;m->user=user;load(m,true);
}
void thinice_model_press(ThinIceModel *m,IceDirection d){m->held[d]=true;}
void thinice_model_release(ThinIceModel *m,IceDirection d){m->held[d]=false;}
void thinice_model_release_all(ThinIceModel *m){memset(m->held,0,sizeof m->held);}
void thinice_model_reset(ThinIceModel *m){if(m->phase==ICE_WON)return;m->tries++;load(m,false);}

static void finish_level(ThinIceModel *m){
 m->lastSolved=m->tiles==thinice_model_goal(m);
 if(m->lastSolved){sound(m,ICE_SOUND_COMPLETE);m->solved++;m->solvedPoints+=m->tiles;if(m->tries==1){m->firstTries++;m->firstTryPoints+=m->tiles;}}
 m->savedTiles+=m->tiles;m->savedBags+=m->bags;m->tiles=m->bags=0;
 if(m->level==19){m->timePoints=m->solved==19?m->countdown*6:0;m->phase=ICE_WON;thinice_model_release_all(m);return;}
 m->start=m->position;m->level++;m->tries=1;load(m,true);
}
static bool can_move(const ThinIceModel *m,IceDirection d){
 IcePoint n=next(m->position,d);
 if(thinice_model_tile(m,n)>=20)return false;
 return !m->hasBlock||!same(n,m->block)||thinice_model_tile(m,next(m->block,d))<20;
}
static void collect(ThinIceModel *m){
 unsigned t=thinice_model_tile(m,m->position);
 if(t==7){m->bags++;set_tile(m,m->position,2);sound(m,ICE_SOUND_MONEY);}
 if(t==6||t==8||t==9){m->keys++;set_tile(m,m->position,t==6?2:t==8?3:4);sound(m,ICE_SOUND_KEY);}
 for(unsigned d=0;d<4;d++){IcePoint n=next(m->position,d);if(m->keys&&thinice_model_tile(m,n)==21){m->keys--;set_tile(m,n,2);sound(m,ICE_SOUND_KEY);}}
}
static void melt(ThinIceModel *m){
 unsigned t=thinice_model_tile(m,m->position);
 if(t==2){set_tile(m,m->position,22);m->tiles++;}
 else if(t==4){set_tile(m,m->position,2);m->tiles++;sound(m,ICE_SOUND_ICE);}
 else if(thinice_model_tile(m,next(m->position,ICE_DOWN))==14){
  sound(m,ICE_SOUND_WARP);
  for(unsigned i=0;i<THINICE_SECRET;i++){const uint8_t *s=THINICE_SECRET_DATA[i];set_tile(m,(IcePoint){s[0],s[1]},s[2]==7&&m->solved<s[3]?2:s[2]);}
 }
}
static int warp_at(const ThinIceModel *m,IcePoint p){for(unsigned i=0;i<m->warpCount;i++)if(same(m->warps[i],p))return (int)i;return -1;}
static void slide_block(ThinIceModel *m){
 if(!m->hasBlock||!m->sliding)return;
 if(m->blockSteps>0){m->blockSteps--;return;}
 m->previousBlock=m->block;
 int w=warp_at(m,m->block);
 if(!m->blockWarped&&w>=0){m->block=m->warps[1-w];m->previousBlock=m->block;m->blockWarped=true;}
 IcePoint n=next(m->block,m->slide);
 if(thinice_model_tile(m,n)>=20){m->sliding=false;return;}
 m->block=n;m->blockSteps=2;
}
void thinice_model_step(ThinIceModel *m){
 if(m->phase==ICE_WON)return;
 if(++m->timer==25){m->timer=0;if(m->countdown)m->countdown--;}
 if(m->phase==ICE_ENTERING){if(++m->phaseTick>=18){m->phase=ICE_PLAYING;m->phaseTick=0;}return;}
 if(m->phase==ICE_SINKING){if(++m->phaseTick>=40)thinice_model_reset(m);return;}
 slide_block(m);
 if(m->steps>0){m->steps--;return;}
 m->previous=m->position;collect(m);
 int w=warp_at(m,m->position);
 if(!m->warped&&w>=0){m->position=m->warps[1-w];m->previous=m->position;m->warped=true;sound(m,ICE_SOUND_WARP);return;}
 if(thinice_model_tile(m,m->position)==5){finish_level(m);return;}
 bool any=false;for(unsigned d=0;d<4;d++)if(can_move(m,d))any=true;
 if(!any){m->phase=ICE_SINKING;m->phaseTick=0;set_tile(m,m->position,12);return;}
 int direction=-1;for(unsigned d=0;d<4&&direction<0;d++)if(m->held[d]&&can_move(m,d))direction=(int)d;
 if(direction<0)return;
 IcePoint n=next(m->position,direction);
 if(m->hasBlock&&same(n,m->block)){m->slide=direction;m->sliding=true;m->blockSteps=0;slide_block(m);sound(m,ICE_SOUND_MOVE);}
 melt(m);m->position=n;m->steps=3;
}
void thinice_model_render_position(const ThinIceModel *m,double alpha,double *x,double *y){
 double p=m->steps?fmin(1,(4-(double)m->steps+alpha)/3):1;
 *x=m->previous.x+(m->position.x-m->previous.x)*p;*y=m->previous.y+(m->position.y-m->previous.y)*p;
}
bool thinice_model_render_block(const ThinIceModel *m,double alpha,double *x,double *y){
 if(!m->hasBlock)return false;
 double p=fmin(1,(3-(double)m->blockSteps+alpha)/3);
 *x=m->previousBlock.x+(m->block.x-m->previousBlock.x)*p;*y=m->previousBlock.y+(m->block.y-m->previousBlock.y)*p;return true;
}
