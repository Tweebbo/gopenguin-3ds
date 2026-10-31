#include "overlay.h"
#include <math.h>
void overlay_throw(Overlay *overlay,float x,float y,float toX,float toY){Snowball *ball=&overlay->balls[overlay->next++%OVERLAY_BALLS];*ball=(Snowball){x,y,toX,toY,0,fmaxf(1,hypotf(toX-x,toY-y)/15),true,false};}
void overlay_step(Overlay *overlay,float elapsed){if(elapsed<0||!isfinite(elapsed))return;for(unsigned i=0;i<OVERLAY_BALLS;i++){Snowball *ball=&overlay->balls[i];if(!ball->active||ball->landed)continue;ball->time+=elapsed*24;if(ball->time>=20+ball->duration)ball->landed=true;}}
bool overlay_ball(const Snowball *ball,float *x,float *y,float *groundY){if(!ball->active||ball->time<=20)return false;if(ball->landed){*x=ball->toX;*y=ball->toY;*groundY=ball->toY;return true;}float p=fmaxf(0,(ball->time-20)/ball->duration),q=p<.5f?p*2:p*2-1,height=p<.5f?-6-94*(1-(1-q)*(1-q)):-100+100*q*q;*x=ball->x+(ball->toX-ball->x)*p;*groundY=ball->y+(ball->toY-ball->y)*p;*y=*groundY+height;return true;}
