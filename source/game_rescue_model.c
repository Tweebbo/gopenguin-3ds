#include "game_rescue_model.h"
#include <string.h>

static bool ground(unsigned theme,unsigned tile){
 if(theme==0)return tile>=1&&tile<=21;
 if(theme==2)return tile>=1&&tile<=16&&tile!=14;
 if(tile==2||(tile>=4&&tile<=14&&tile!=5)||tile==44)return false;
 return true;
}
bool rescue_model_passable(const RescueModel *m,unsigned x,unsigned y){
 if(x>=m->width||y>=m->height)return false;
 unsigned at=y*m->width+x;
 if(m->parts[at]!=1||m->parts2[at]!=1)return true;
 return ground(rescue_model_level(m)->theme,m->map[at]);
}
void rescue_model_init(RescueModel *m){memset(m,0,sizeof *m);m->phase=RESCUE_PLAYING;}
void rescue_model_start(RescueModel *m){m->level=0;m->score=0;rescue_model_load(m);}
void rescue_model_load(RescueModel *m){
 const RescueLevelData *l=rescue_model_level(m);m->width=l->width;m->height=l->height;
 memcpy(m->map,l->map,(size_t)l->width*l->height*sizeof(uint16_t));
 memcpy(m->parts,l->parts,(size_t)l->width*l->height);
 memcpy(m->parts2,l->parts2,(size_t)l->width*l->height);
 m->x=l->homeX;m->y=l->homeY;m->carried=0;m->rescued=0;m->moves=0;memset(m->collected,0,sizeof m->collected);m->phase=RESCUE_PLAYING;
}
static void pickup(RescueModel *m){
 const RescueLevelData *l=rescue_model_level(m);
 for(unsigned i=0;i<RESCUE_PUFFLES;i++)if(!m->collected[i]&&m->x==l->puffleX[i]&&m->y==l->puffleY[i]){m->collected[i]=true;m->carried++;}
}
static void deliver(RescueModel *m){
 const RescueLevelData *l=rescue_model_level(m);if(m->x!=l->homeX||m->y!=l->homeY)return;
 if(m->carried){m->rescued+=m->carried;m->score+=m->carried*50;m->carried=0;}
 if(m->rescued==3){m->score+=250;m->phase=RESCUE_DONE;}
}
bool rescue_model_step(RescueModel *m,int dx,int dy){
 if(m->phase!=RESCUE_PLAYING)return false;
 if((dx==0)==(dy==0))return false;
 int nx=(int)m->x+dx,ny=(int)m->y+dy;if(nx<0||ny<0)return false;
 if(!rescue_model_passable(m,(unsigned)nx,(unsigned)ny))return false;
 m->x=(unsigned)nx;m->y=(unsigned)ny;m->moves++;pickup(m);deliver(m);return true;
}
void rescue_model_next(RescueModel *m){
 if(m->phase!=RESCUE_DONE)return;
 if(m->level+1>=RESCUE_LEVELS){m->phase=RESCUE_WON;return;}
 m->level++;
 rescue_model_load(m);
}
const RescueLevelData *rescue_model_level(const RescueModel *m){return &RESCUE_LEVEL_DATA[m->level];}
unsigned rescue_model_total(const RescueModel *m){return m->score;}
