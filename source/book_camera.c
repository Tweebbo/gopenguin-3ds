#include "book_camera.h"
#include <math.h>
void book_camera_reset(BookCamera *c,float x,float y){*c=(BookCamera){.x=x,.y=y};}
void book_camera_cancel(BookCamera *c){c->tracking=c->dragging=false;}
void book_camera_begin(BookCamera *c,float x,float y){
 c->tracking=true;c->dragging=false;c->touchX=c->startX=x;c->touchY=c->startY=y;
}
void book_camera_drag(BookCamera *c,float x,float y,float scaleX,float scaleY){
 if(!c->tracking||!isfinite(x)||!isfinite(y)||!isfinite(scaleX)||!isfinite(scaleY)||scaleX<=0||scaleY<=0)return;
 if(!c->dragging&&hypotf(x-c->startX,y-c->startY)<4)return;
 c->dragging=true;c->x+=(x-c->touchX)/scaleX;c->y+=(y-c->touchY)/scaleY;c->touchX=x;c->touchY=y;
}
bool book_camera_end(BookCamera *c){bool tap=c->tracking&&!c->dragging;book_camera_cancel(c);return tap;}
void book_camera_pan(BookCamera *c,float dx,float dy,float seconds){
 if(c->dragging||!isfinite(dx)||!isfinite(dy)||!isfinite(seconds)||seconds<=0)return;
 float magnitude=hypotf(dx,dy);const float deadzone=.2f;if(magnitude<=deadzone)return;
 float strength=(fminf(magnitude,1)-deadzone)/(1-deadzone),distance=240*fminf(seconds,.1f)*strength/magnitude;
 c->x+=dx*distance;c->y+=dy*distance;
}
void book_camera_clamp(BookCamera *c,float left,float top,float right,float bottom,float halfWidth,float halfHeight){
 if(!isfinite(left)||!isfinite(top)||!isfinite(right)||!isfinite(bottom)||!isfinite(halfWidth)||!isfinite(halfHeight)||right<left||bottom<top||halfWidth<0||halfHeight<0)return;
 halfWidth=fminf(halfWidth,(right-left)/2);halfHeight=fminf(halfHeight,(bottom-top)/2);
 c->x=fmaxf(left+halfWidth,fminf(right-halfWidth,c->x));c->y=fmaxf(top+halfHeight,fminf(bottom-halfHeight,c->y));
}
