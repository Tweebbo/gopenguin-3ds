#pragma once
#include <math.h>

typedef struct {float scale,x,y;} Viewport;

/* Cover keeps the original proportions and clamps the camera inside the stage. */
static inline Viewport viewport_cover(float width,float height,float stageWidth,float stageHeight,float cx,float cy,float zoom){
 float scale=fmaxf(width/stageWidth,height/stageHeight)*fmaxf(1,zoom);
 float halfX=width/(2*scale),halfY=height/(2*scale);
 cx=fmaxf(halfX,fminf(stageWidth-halfX,cx));cy=fmaxf(halfY,fminf(stageHeight-halfY,cy));
 return (Viewport){scale,width/2-cx*scale,height/2-cy*scale};
}
