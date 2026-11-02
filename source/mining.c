#include "mining.h"
#include "frame_hacks.h"
#include <math.h>
bool mining_eligible(unsigned room,const unsigned look[6],unsigned action,bool moving,float x,float y){
 if(room!=813||moving||action!=26||!isfinite(x)||!isfinite(y)||x<126.55f||x>636.4f||y<274.4f||y>482.4f)return false;
 if(look[4]!=342&&look[0]!=403&&look[0]!=429&&look[0]!=674&&look[0]!=10403)return false;
 return penguin_secret_frame(look,action)!=action;
}
void mining_enter(Mining *m,unsigned room){m->room=room;m->visit++;m->elapsed=0;m->rewardTime=0;}
bool mining_update(Mining *m,float seconds,bool enabled,const unsigned look[6],unsigned action,bool moving,float x,float y){
 float dt=isfinite(seconds)?fmaxf(0,fminf(.25f,seconds)):0;
 m->pending=fmaxf(0,m->pending-dt);m->rewardTime=fmaxf(0,m->rewardTime-dt);
 if(!enabled||!mining_eligible(m->room,look,action,moving,x,y)){m->elapsed=0;return false;}
 m->elapsed+=dt;if(m->elapsed<2.1f||m->pending>0)return false;
 m->elapsed=0;m->pending=10;m->requestRoom=m->room;m->requestVisit=m->visit;m->requestX=x;m->requestY=y;return true;
}
bool mining_reply(Mining *m,unsigned earned){
 if(m->pending<=0)return false;
 m->pending=0;
 if(!earned||earned>100||m->room!=m->requestRoom||m->visit!=m->requestVisit||m->room!=813)return false;
 m->earned=earned;m->rewardX=m->requestX;m->rewardY=m->requestY;m->rewardTime=1.5f;return true;
}
