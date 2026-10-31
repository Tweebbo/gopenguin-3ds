#pragma once
#include <stdbool.h>
enum {SNOWBALL_LIMIT=11};
typedef struct {float x,y,toX,toY,time,duration;bool active;} Snowball;
typedef struct {Snowball balls[SNOWBALL_LIMIT];unsigned next;} Overlay;
void overlay_throw(Overlay *overlay,float x,float y,float toX,float toY);
void overlay_step(Overlay *overlay,float elapsed);
bool overlay_ball(const Snowball *ball,float *x,float *y);
bool overlay_shadow(const Snowball *ball,float *x,float *y);
bool overlay_impact(const Snowball *ball,float *x,float *y);
