#pragma once
/* Shared presentation for the PSA missions 3-11 (game_mission3.c, game_mission.c).
   Top screen = the original room scene full-bleed with a mission dialogue box (white box, dark edge and navy ink,
   the same look as the PSA 1-2 speech panels) and a location name plate. Lower screen = the same room as the touch
   surface, a glass header with progress and Exit, puzzle controls as cards over the scene and one large action button.
   Only drawn primitives (rects, text) through the GamePort host: no art is redrawn. */
#include "game_controller.h"
#include "font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Colours. Avoid the exact values ui_game_fill() special-cases (0xffffff cards, 0x003366/0x062646/0x102040 bars). */
#define ML_EDGE 0x1a2832u
#define ML_PAPER 0xfffffeu
#define ML_INK 0x0a2f55u
#define ML_PLATE 0x0b5a99u
#define ML_PLATE_DARK 0x07365fu
#define ML_GOLD 0xffd23fu
#define ML_GOAL 0x9a4a00u
#define ML_OK 0x2f9e44u
#define ML_BAD 0xd9480fu
#define ML_RULE 0xc9dceau

#define ML_BUTTON_EXIT 2u
#define ML_BUTTON_SCENE 4u

static inline float ml_width(unsigned size,const char *text){
 unsigned row=(size<FONT_MIN?FONT_MIN:size>FONT_MAX?FONT_MAX:size)-FONT_MIN;float w=0;
 for(const unsigned char *s=(const unsigned char *)text;*s;s++)w+=fontGlyphs[row][(*s>=32&&*s<127?*s:'?')-32].advance;
 return w;
}
/* Rounded rectangle from horizontal strips (the host only fills rectangles). */
static inline void ml_round(GamePort *p,float x,float y,float w,float h,float r,uint32_t rgb){
 if(w<=0||h<=0)return;
 if(r>h/2)r=floorf(h/2);
 if(r>w/2)r=floorf(w/2);
 unsigned n=(unsigned)r;
 for(unsigned i=0;i<n;i++){
  float dy=r-i-.5f,inset=roundf(r-sqrtf(fmaxf(0,r*r-dy*dy)));
  game_port_rect(p,x+inset,y+i,w-2*inset,1,rgb);
  game_port_rect(p,x+inset,y+h-1-i,w-2*inset,1,rgb);
 }
 if(h-2*n>0)game_port_rect(p,x,y+n,w,h-2*n,rgb);
}
/* Edge, fill and an optional soft drop shadow. */
static inline void ml_panel(GamePort *p,float x,float y,float w,float h,float r,uint32_t edge,uint32_t fill,float thick,bool shadow){
 if(shadow)ml_round(p,x+1,y+2,w,h,r,0x061a2e);
 ml_round(p,x,y,w,h,r,edge);
 ml_round(p,x+thick,y+thick,w-2*thick,h-2*thick,r-thick>1?r-thick:1,fill);
}
/* Word wrap on real glyph widths; draws when draw is true; returns the row count. */
static inline unsigned ml_wrap(GamePort *p,const char *text,float x,float y,float width,unsigned size,float leading,uint32_t rgb,bool draw){
 char line[160];size_t length=0;unsigned rows=0;
 while(*text){
  while(*text==' ')text++;
  const char *end=text;while(*end&&*end!=' ')end++;
  size_t word=(size_t)(end-text);if(!word)break;
  if(word>sizeof line-2)word=sizeof line-2;
  char trial[160];memcpy(trial,line,length);size_t t=length;if(t)trial[t++]=' ';
  if(t+word>=sizeof trial)word=sizeof trial-1-t;
  memcpy(trial+t,text,word);trial[t+word]=0;
  if(length&&ml_width(size,trial)>width){
   line[length]=0;if(draw)game_port_text(p,x,y+rows*leading,size,rgb,GAME_ALIGN_LEFT,line);rows++;
   memcpy(line,text,word);length=word;
  }else{memcpy(line,trial,t+word);length=t+word;}
  text=end;
 }
 if(length){line[length]=0;if(draw)game_port_text(p,x,y+rows*leading,size,rgb,GAME_ALIGN_LEFT,line);rows++;}
 return rows;
}
/* Label that shrinks (down to 10 px) to fit a width. */
static inline unsigned ml_fit(const char *text,unsigned size,float width){while(size>10&&ml_width(size,text)>width)size--;return size;}

/* ---------- Top screen ---------- */
static inline void ml_top_tags(GamePort *p,unsigned mission,const char *title,unsigned step,unsigned count){
 char line[96];snprintf(line,sizeof line,"PSA %u: %s",mission,title);
 float w=ml_width(11,line)+18;ml_panel(p,6,5,w,20,8,0xd6ecff,ML_PLATE_DARK,1,true);
 game_port_text(p,15,9,11,0xffffff,GAME_ALIGN_LEFT,line);
 if(count){
  snprintf(line,sizeof line,"Step %u of %u",step,count);float sw=ml_width(11,line)+18;
  ml_panel(p,394-sw,5,sw,20,8,0xd6ecff,ML_PLATE_DARK,1,true);game_port_text(p,394-sw/2,9,11,ML_GOLD,GAME_ALIGN_CENTER,line);
 }
}
/* Dialogue box anchored to the bottom of the top screen, sized to its text. */
static inline void ml_dialog(GamePort *p,const char *speaker,const char *text,const char *objective){
 const float x=6,w=388,pad=10,tw=w-2*pad,lead=14;
 unsigned rows=ml_wrap(p,text,0,0,tw,11,lead,0,false);
 unsigned goals=objective&&*objective?ml_wrap(p,objective,0,0,tw-62,11,lead,0,false):0;
 float h=pad+8+rows*lead+(goals?7+goals*lead:0)+pad-2;
 float y=236-h;
 ml_panel(p,x,y,w,h,9,ML_EDGE,ML_PAPER,2,true);
 ml_wrap(p,text,x+pad,y+pad+5,tw,11,lead,ML_INK,true);
 if(goals){
  float gy=y+pad+5+rows*lead+3;
  game_port_rect(p,x+pad,gy,tw,1,ML_RULE);
  ml_round(p,x+pad,gy+5,56,15,6,ML_GOAL);game_port_text(p,x+pad+28,gy+7,10,0xffffff,GAME_ALIGN_CENTER,"GOAL");
  ml_wrap(p,objective,x+pad+62,gy+6,tw-62,11,lead,ML_GOAL,true);
 }
 /* Name plate over the top-left edge, as a dialogue speaker tag. */
 if(speaker&&*speaker){
  unsigned size=ml_fit(speaker,12,220);float nw=ml_width(size,speaker)+20;
  ml_panel(p,x+12,y-12,nw,21,8,ML_EDGE,ML_PLATE,2,false);
  game_port_text(p,x+12+nw/2,y-8,size,0xffffff,GAME_ALIGN_CENTER,speaker);
 }
}
static inline void ml_accomplished(GamePort *p,const char *title,unsigned coins){
 char line[64];
 ml_panel(p,60,44,280,112,12,ML_GOLD,ML_PLATE_DARK,3,true);
 game_port_text(p,200,58,16,ML_GOLD,GAME_ALIGN_CENTER,"Mission accomplished!");
 unsigned size=ml_fit(title,13,250);game_port_text(p,200,84,size,0xffffff,GAME_ALIGN_CENTER,title);
 snprintf(line,sizeof line,"+%u coins",coins);
 ml_round(p,140,108,120,30,10,ML_GOLD);game_port_text(p,200,115,16,ML_INK,GAME_ALIGN_CENTER,line);
}

/* ---------- Lower screen ---------- */
/* Cover fit: 760x480 stage at 0.5 fills 320x240 (crop 30 px each side); art is exported at 0.5, so 1:1 texels. */
static inline SWLMatrix mission_scene_view(void){return (SWLMatrix){.5f,0,0,.5f,-30,0};}
static inline void ml_header(GamePort *p,const char *place,unsigned step,unsigned count){
 game_port_rect(p,0,0,320,24,0x003366); /* themed glass bar */
 unsigned size=ml_fit(place,12,150);game_port_text(p,8,5,size,0xffffff,GAME_ALIGN_LEFT,place);
 if(count){
  float gap=count>10?7:9,x0=258-count*gap;
  for(unsigned i=0;i<count;i++){
   uint32_t c=i+1<step?ML_GOLD:i+1==step?0xffffff:0x2b6aa0;
   ml_round(p,x0+i*gap,9,6,6,3,c);
  }
 }
 game_port_button(p,ML_BUTTON_EXIT,266,2,50,20,"Exit",true);
}
/* A round face-button glyph (A, B) for physical-control hints. */
static inline void ml_key(GamePort *p,float x,float y,const char *letter){
 ml_round(p,x,y,16,16,8,0xffffff);ml_round(p,x+1.5f,y+1.5f,13,13,6.5f,0x4a5a68);
 game_port_text(p,x+8,y+2,10,0xffffff,GAME_ALIGN_CENTER,letter);
}
/* Large action button at the foot of the lower screen; the A glyph says the physical button does the same. */
static inline void ml_action(GamePort *p,unsigned id,const char *label,bool enabled,const char *hint){
 if(hint&&*hint){
  float w=ml_width(10,hint)+16;ml_round(p,160-w/2,174,w,16,7,0x0a2a4a);
  game_port_text(p,160,176,10,0xd6ecff,GAME_ALIGN_CENTER,hint);
 }
 game_port_button(p,id,36,194,248,40,label,enabled);
 if(enabled)ml_key(p,46,206,"A");
}
/* Focus ring for D-pad selection. */
static inline void ml_focus(GamePort *p,float x,float y,float w,float h){ml_round(p,x-3,y-3,w+6,h+6,10,ML_GOLD);}
/* Small tag drawn on a choice that is already in the sequence. */
static inline void ml_tick(GamePort *p,float x,float y,unsigned number){
 char n[12];snprintf(n,sizeof n,"%u",number);ml_round(p,x,y,18,18,9,ML_OK);ml_round(p,x+2,y+2,14,14,7,0x3fbf55);
 game_port_text(p,x+9,y+3,11,0xffffff,GAME_ALIGN_CENTER,n);
}
