#include "../source/name_tag.h"
#include "../source/font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned address(unsigned x,unsigned y,unsigned width,unsigned height){
 (void)height;unsigned pixel=((y/8)*(width/8)+x/8)*64;
 for(unsigned bit=0;bit<3;bit++){pixel|=(x&(1u<<bit))<<bit;pixel|=(y&(1u<<bit))<<(bit+1);}
 return pixel*2;
}
static unsigned char font[512*256*2],atlas[NAME_TAG_W*NAME_TAG_H*2],reference[NAME_TAG_W*NAME_TAG_H];
int main(void){
 for(unsigned y=0;y<256;y++)for(unsigned x=0;x<512;x++){unsigned n=address(x,y,512,256);font[n]=(x*7+y*13)%256;font[n+1]=255;}
 const char *samples[]={"Penguin","Captain Rockhopper","Gold0","jj_WWgy","!$@[]{}~","a b c","0123456789","iiiiiiiiiiiiiiii","WWWWWWWWWWWWWWWW"};
 unsigned tested=0;
 for(unsigned pass=0;pass<2;pass++)for(unsigned slot=0;slot<NAME_TAG_COUNT;slot++){
  const char *name=samples[(slot+pass)%9];NameTag tag;if(!name_tag_layout(&tag,name))continue;
  name_tag_write(atlas,slot,&tag,name,font,512,256);
  unsigned dx=slot%NAME_TAG_COLUMNS*NAME_TAG_SLOT_W,dy=slot/NAME_TAG_COLUMNS*NAME_TAG_SLOT_H;
  for(unsigned y=0;y<NAME_TAG_SLOT_H;y++)memset(reference+(dy+y)*NAME_TAG_W+dx,0,NAME_TAG_SLOT_W);
  int pen=0;
  for(const char *s=name;*s;s++){
   const FontGlyph *g=&fontGlyphs[0][*s-32];
   for(int y=0;y<g->h;y++)for(int x=0;x<g->w;x++){
    unsigned alpha=((g->x+x)*7+(g->y+y)*13)%256;
    unsigned char *out=&reference[(dy+fontAscents[0]+g->top-tag.top+y)*NAME_TAG_W+dx+pen+g->left-tag.left+x];
    *out=(unsigned char)(alpha+(*out*(255-alpha)+127)/255);
   }
   pen+=g->advance;
  }
  assert(pen==tag.advance);tested++;
 }
 for(unsigned y=0;y<NAME_TAG_H;y++)for(unsigned x=0;x<NAME_TAG_W;x++)assert(atlas[address(x,y,NAME_TAG_W,NAME_TAG_H)]==reference[y*NAME_TAG_W+x]);
 NameTag tag;assert(!name_tag_layout(&tag,"Pingu\xc3\xafno"));assert(!name_tag_layout(&tag,"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"));assert(!name_tag_layout(&tag,""));assert(!name_tag_layout(&tag,"  "));assert(!name_tag_layout(&tag,"A\nB"));
 for(unsigned c=33;c<127;c++){char name[]={(char)c,0};assert(name_tag_layout(&tag,name));assert(tag.width>0&&tag.height>0);}
 printf("PASS %u cached names and replacements across all 80 slots; glyph overhang, alpha overlap, tile boundaries, all ASCII glyphs and Unicode/long-name fallbacks\n",tested);
}
