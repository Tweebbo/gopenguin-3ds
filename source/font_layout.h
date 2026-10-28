#pragma once
#include "font.h"
#include <limits.h>

typedef struct {int left,top,right,bottom;} FontBounds;

static inline FontBounds font_ink(unsigned size,const char *text){
 unsigned row=(size<FONT_MIN?FONT_MIN:size>FONT_MAX?FONT_MAX:size)-FONT_MIN;
 FontBounds out={INT_MAX,INT_MAX,INT_MIN,INT_MIN};int pen=0;
 for(const unsigned char *s=(const unsigned char *)text;*s;s++){
  const FontGlyph *g=&fontGlyphs[row][(*s>=32&&*s<127?*s:'?')-32];
  if(g->w&&g->h){int x=pen+g->left,y=fontAscents[row]+g->top;if(x<out.left)out.left=x;if(y<out.top)out.top=y;if(x+g->w>out.right)out.right=x+g->w;if(y+g->h>out.bottom)out.bottom=y+g->h;}
  pen+=g->advance;
 }
 return out.left==INT_MAX?(FontBounds){0,0,pen,0}:out;
}
