#include "overlay.h"
#include <math.h>
void overlay_throw(Overlay *overlay,float x,float y,float toX,float toY){Snowball *ball=&overlay->balls[overlay->next++%64];*ball=(Snowball){x,y,toX,toY,0,fmaxf(1,hypotf(toX-x,toY-y)/15),true};}
void overlay_step(Overlay *overlay,float elapsed){if(elapsed<0||!isfinite(elapsed))return;for(unsigned i=0;i<64;i++){Snowball *ball=&overlay->balls[i];ball->time+=elapsed*24;if(ball->time>=20+ball->duration)ball->active=false;}}
bool overlay_ball(const Snowball *ball,float *x,float *y){if(!ball->active||ball->time<=20)return false;float p=fmaxf(0,(ball->time-20)/ball->duration),q=p<.5f?p*2:p*2-1,height=p<.5f?-6-94*(1-(1-q)*(1-q)):-100+100*q*q;*x=ball->x+(ball->toX-ball->x)*p;*y=ball->y+(ball->toY-ball->y)*p+height;return true;}
