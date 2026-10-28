#pragma once
#include <stdbool.h>
typedef struct {float x,y,toX,toY,time,duration;bool active;} Snowball;
typedef struct {Snowball balls[64];unsigned next;} Overlay;
void overlay_throw(Overlay *overlay,float x,float y,float toX,float toY);
void overlay_step(Overlay *overlay,float elapsed);
bool overlay_ball(const Snowball *ball,float *x,float *y);
