#include "game_rescue_model.h"
#include <math.h>
#include <string.h>

static bool ground(unsigned theme,unsigned tile){
 if(theme==0)return tile>=1&&tile<=21;
 if(theme==2)return tile>=1&&tile<=16&&tile!=14;
 if(tile==2||(tile>=4&&tile<=14&&tile!=5)||tile==44)return false;
 return true;
}
static int mover_offset(const RescueMover *p,double time){return (int)lround(sin(time*1.3+p->phase*0.7)*p->range);}
static int platform_at(const RescueModel *m,unsigned x,unsigned y){
 for(unsigned i=0;i<m->platformCount;i++){const RescueMover *p=&m->platforms[i];if((unsigned)((int)p->x+p->offset)==x&&p->y==y)return (int)i;}
 return -1;
}
static int hazard_at(const RescueModel *m,unsigned x,unsigned y){
 for(unsigned i=0;i<m->hazardCount;i++){const RescueMover *p=&m->hazards[i];if((unsigned)((int)p->x+p->offset)==x&&p->y==y)return (int)i;}
 return -1;
}
bool rescue_model_passable(const RescueModel *m,unsigned x,unsigned y){
 if(x>=m->width||y>=m->height)return false;
 unsigned at=y*m->width+x;
 unsigned part=m->parts[at];
 if(part==3&&m->broken[at])return false;
 if(part==7||part==8)return platform_at(m,x,y)>=0;
 if(part!=1||m->parts2[at]!=1)return true;
 return ground(rescue_model_level(m)->theme,m->map[at]);
}
unsigned rescue_model_platform_count(const RescueModel *m){return m->platformCount;}
bool rescue_model_platform_cell(const RescueModel *m,unsigned index,unsigned *x,unsigned *y,unsigned *frame){
 if(index>=m->platformCount)return false;
 const RescueMover *p=&m->platforms[index];int nx=(int)p->x+p->offset;
 if(nx<0||(unsigned)nx>=m->width)return false;
 if(x)*x=(unsigned)nx;
 if(y)*y=p->y;
 if(frame)*frame=p->value;
 return true;
}
unsigned rescue_model_hazard_count(const RescueModel *m){return m->hazardCount;}
bool rescue_model_hazard_cell(const RescueModel *m,unsigned index,unsigned *x,unsigned *y){
 if(index>=m->hazardCount)return false;
 const RescueMover *p=&m->hazards[index];int nx=(int)p->x+p->offset;
 if(nx<0||(unsigned)nx>=m->width)return false;
 if(x)*x=(unsigned)nx;
 if(y)*y=p->y;
 return true;
}
void rescue_model_init(RescueModel *m){memset(m,0,sizeof *m);m->phase=RESCUE_PLAYING;}
void rescue_model_start(RescueModel *m){m->level=0;m->score=0;rescue_model_load(m);}
void rescue_model_load(RescueModel *m){
 const RescueLevelData *l=rescue_model_level(m);m->width=l->width;m->height=l->height;
 memcpy(m->map,l->map,(size_t)l->width*l->height*sizeof(uint16_t));
 memcpy(m->parts,l->parts,(size_t)l->width*l->height);
 memcpy(m->parts2,l->parts2,(size_t)l->width*l->height);
 m->x=l->homeX;m->y=l->homeY;m->carried=0;m->rescued=0;m->moves=0;m->time=0;m->riding=-1;
 m->platformCount=m->hazardCount=0;memset(m->collected,0,sizeof m->collected);memset(m->broken,0,sizeof m->broken);memset(m->crackMs,0,sizeof m->crackMs);m->phase=RESCUE_PLAYING;
 for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){
  unsigned at=y*m->width+x;unsigned part=m->parts[at];
  if((part==7||part==8)&&m->platformCount<RESCUE_PLATFORMS){RescueMover *p=&m->platforms[m->platformCount++];*p=(RescueMover){.x=(uint8_t)x,.y=(uint8_t)y,.value=(uint8_t)part,.phase=(uint8_t)((x*7+y*13)%16),.range=2};p->offset=mover_offset(p,0);}
  if(l->theme==2&&m->parts2[at]==2&&m->hazardCount<RESCUE_HAZARDS){RescueMover *p=&m->hazards[m->hazardCount++];*p=(RescueMover){.x=(uint8_t)x,.y=(uint8_t)y,.value=2,.phase=(uint8_t)((x*5+y*11)%16),.range=3,.kind=0};p->offset=mover_offset(p,0);}
  if(l->theme==1&&m->map[at]==59&&m->hazardCount<RESCUE_HAZARDS){RescueMover *p=&m->hazards[m->hazardCount++];*p=(RescueMover){.x=(uint8_t)x,.y=(uint8_t)(y>3?y-3:1),.value=4,.phase=(uint8_t)((x*3+y*5)%16),.range=3,.kind=2};p->offset=mover_offset(p,0);}
 }
 if(l->theme==0&&m->hazardCount<RESCUE_HAZARDS-1){for(unsigned i=0;i<2;i++){RescueMover *p=&m->hazards[m->hazardCount++];*p=(RescueMover){.x=(uint8_t)(m->width/2+i*4),.y=(uint8_t)(8+i*4),.value=3,.phase=(uint8_t)(3+i*6),.range=3,.kind=1};p->offset=mover_offset(p,0);}}
 if(l->theme==1&&m->hazardCount==0&&m->hazardCount<RESCUE_HAZARDS){RescueMover *p=&m->hazards[m->hazardCount++];*p=(RescueMover){.x=(uint8_t)(m->width/2),.y=6,.value=4,.phase=5,.range=3,.kind=2};p->offset=mover_offset(p,0);}
 if(l->theme==2&&m->hazardCount<RESCUE_HAZARDS-1){if(m->level==6){RescueMover *p=&m->hazards[m->hazardCount++];*p=(RescueMover){.x=14,.y=16,.value=5,.phase=7,.range=4,.kind=3};p->offset=mover_offset(p,0);}RescueMover *p=&m->hazards[m->hazardCount++];*p=(RescueMover){.x=(uint8_t)(m->width-6),.y=6,.value=6,.phase=11,.range=2,.kind=4};p->offset=mover_offset(p,0);}
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
static void hazard_touch(RescueModel *m){
 int index=hazard_at(m,m->x,m->y);if(index<0)return;
 if(m->hazards[index].kind==4){m->score+=25;m->hazards[index]=m->hazards[--m->hazardCount];return;}
 const RescueLevelData *l=rescue_model_level(m);m->x=l->homeX;m->y=l->homeY;m->carried=0;m->riding=-1;
}
bool rescue_model_step(RescueModel *m,int dx,int dy){
 if(m->phase!=RESCUE_PLAYING)return false;
 if((dx==0)==(dy==0))return false;
 int nx=(int)m->x+dx,ny=(int)m->y+dy;if(nx<0||ny<0)return false;
 if(!rescue_model_passable(m,(unsigned)nx,(unsigned)ny))return false;
 m->x=(unsigned)nx;m->y=(unsigned)ny;m->moves++;m->riding=platform_at(m,m->x,m->y);
 if(m->parts[m->y*m->width+m->x]==3&&!m->broken[m->y*m->width+m->x])m->crackMs[m->y*m->width+m->x]=3000;
 pickup(m);deliver(m);
 hazard_touch(m);
 return true;
}
void rescue_model_tick(RescueModel *m,double milliseconds){
 if(m->phase!=RESCUE_PLAYING)return;
 unsigned step=milliseconds>0?(unsigned)milliseconds:0;
 m->time+=step/1000.0;
 for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){unsigned at=y*m->width+x;if(!m->crackMs[at])continue;if(m->crackMs[at]>step)m->crackMs[at]=(uint16_t)(m->crackMs[at]-step);else{m->crackMs[at]=0;m->broken[at]=1;if(m->x==x&&m->y==y){const RescueLevelData *l=rescue_model_level(m);m->x=l->homeX;m->y=l->homeY;m->carried=0;m->riding=-1;}}}
 for(unsigned i=0;i<m->platformCount;i++){
  RescueMover *p=&m->platforms[i];int old=p->offset,next=mover_offset(p,m->time);
  if(m->riding==(int)i){int nx=(int)p->x+next;if(nx>=0&&(unsigned)nx<m->width){m->x=(unsigned)nx;m->y=p->y;}}
  p->offset=next;(void)old;
 }
 for(unsigned i=0;i<m->hazardCount;i++)m->hazards[i].offset=mover_offset(&m->hazards[i],m->time);
 hazard_touch(m);
 if(m->riding>=0&&platform_at(m,m->x,m->y)<0)m->riding=-1;
}
void rescue_model_next(RescueModel *m){
 if(m->phase!=RESCUE_DONE)return;
 if(m->level+1>=RESCUE_LEVELS){m->phase=RESCUE_WON;return;}
 m->level++;
 rescue_model_load(m);
}
const RescueLevelData *rescue_model_level(const RescueModel *m){return &RESCUE_LEVEL_DATA[m->level];}
unsigned rescue_model_total(const RescueModel *m){return m->score;}
