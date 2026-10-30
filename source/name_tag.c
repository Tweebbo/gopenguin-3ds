#include "name_tag.h"
#include "font.h"
static unsigned tiled(unsigned x,unsigned y,unsigned width){
 unsigned xx=x&7,yy=y&7;
 return ((y&~7u)*width+(x&~7u)*8)+((xx&1)|((yy&1)<<1)|((xx&2)<<1)|((yy&2)<<2)|((xx&4)<<2)|((yy&4)<<3));
}
bool name_tag_layout(NameTag *tag,const char *name){
 *tag=(NameTag){0};int pen=0,left=0,top=0,right=0,bottom=0;bool ink=false;
 for(const unsigned char *s=(const unsigned char *)name;*s;s++){
  if(*s<32||*s>=127)return false;
  const FontGlyph *g=&fontGlyphs[0][*s-32];
  if(g->w&&g->h){int x=pen+g->left,y=fontAscents[0]+g->top;if(!ink||x<left)left=x;if(!ink||y<top)top=y;if(!ink||x+g->w>right)right=x+g->w;if(!ink||y+g->h>bottom)bottom=y+g->h;ink=true;}
  pen+=g->advance;if(pen>96)return false;
 }
 if(!ink||right-left>NAME_TAG_SLOT_W||bottom-top>NAME_TAG_SLOT_H)return false;
 *tag=(NameTag){pen,left,top,right-left,bottom-top};return true;
}
void name_tag_write(uint8_t *target,unsigned slot,const NameTag *tag,const char *name,const uint8_t *font,unsigned fontWidth,unsigned fontHeight){
 if(slot>=NAME_TAG_COUNT)return;
 unsigned dx=(slot%NAME_TAG_COLUMNS)*NAME_TAG_SLOT_W,dy=(slot/NAME_TAG_COLUMNS)*NAME_TAG_SLOT_H;
 for(unsigned y=0;y<NAME_TAG_SLOT_H;y++)for(unsigned x=0;x<NAME_TAG_SLOT_W;x++){unsigned offset=2*tiled(dx+x,dy+y,NAME_TAG_W);target[offset]=0;target[offset+1]=255;}
 int pen=0;
 for(const unsigned char *s=(const unsigned char *)name;*s;s++){
  const FontGlyph *g=&fontGlyphs[0][*s-32];
  for(int y=0;y<g->h;y++)for(int x=0;x<g->w;x++){
   unsigned sx=g->x+x,sy=g->y+y;if(sx>=fontWidth||sy>=fontHeight)continue;
   unsigned source=2*tiled(sx,sy,fontWidth),out=2*tiled(dx+pen+g->left-tag->left+x,dy+fontAscents[0]+g->top-tag->top+y,NAME_TAG_W);
   unsigned alpha=font[source];target[out]=(uint8_t)(alpha+(target[out]*(255-alpha)+127)/255);
  }
  pen+=g->advance;
 }
}
