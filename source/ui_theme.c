#include "ui_theme.h"
#include "ui_logo_data.h"
#include <3ds.h>
#include <citro2d.h>
#include <math.h>

// Palette of the Club Penguin app style guide (HUD buttons, gradients, drop shadow).
#define BUTTON_TOP 0x086aa6
#define BUTTON_BOTTOM 0x064679
#define BUTTON_DOWN_TOP 0x075b94
#define BUTTON_DOWN_BOTTOM 0x04365d
#define SHADOW 0x0b3d6e
#define CARD_STROKE 0x9fd6f5
#define INK 0x003366

static bool touching;
static float touchX,touchY;

static u32 colour(uint32_t rgb,uint8_t alpha){return C2D_Color32((rgb>>16)&255,(rgb>>8)&255,rgb&255,alpha);}

void ui_frame(void){
 touching=(hidKeysHeld()&KEY_TOUCH)!=0;
 if(touching){touchPosition p;hidTouchRead(&p);touchX=p.px;touchY=p.py;}
}
bool ui_touching(float x,float y,float w,float h){return touching&&touchX>=x&&touchY>=y&&touchX<x+w&&touchY<y+h;}

void ui_round(float x,float y,float w,float h,float r,uint32_t rgb,uint8_t alpha){
 if(w<=0||h<=0)return;
 r=fminf(r,fminf(h,w)/2);
 u32 c=colour(rgb,alpha);
 if(r<1){C2D_DrawRectSolid(x,y,.5f,w,h,c);return;}
 if(alpha==255){
  C2D_DrawRectSolid(x+r,y,.5f,w-2*r,h,c);
  C2D_DrawRectSolid(x,y+r,.5f,r,h-2*r,c);C2D_DrawRectSolid(x+w-r,y+r,.5f,r,h-2*r,c);
  C2D_DrawCircleSolid(x+r,y+r,.5f,r,c);C2D_DrawCircleSolid(x+w-r,y+r,.5f,r,c);
  C2D_DrawCircleSolid(x+r,y+h-r,.5f,r,c);C2D_DrawCircleSolid(x+w-r,y+h-r,.5f,r,c);
  return;
 }
 // Translucent fills must not overlap themselves: the rounded rows are separate strips.
 C2D_DrawRectSolid(x,y+r,.5f,w,h-2*r,c);
 for(int i=0;i<(int)ceilf(r);i++){
  float dy=r-i-.5f,inset=r-sqrtf(fmaxf(0,r*r-dy*dy)),row=fminf(1,r-i);
  C2D_DrawRectSolid(x+inset,y+i,.5f,w-2*inset,row,c);
  C2D_DrawRectSolid(x+inset,y+h-i-row,.5f,w-2*inset,row,c);
 }
}

static uint32_t mix(uint32_t a,uint32_t b,float t){
 unsigned out=0;for(int s=0;s<=16;s+=8){float v=((a>>s)&255)*(1-t)+((b>>s)&255)*t;out|=(unsigned)lroundf(v)<<s;}return out;
}
void ui_round_gradient(float x,float y,float w,float h,float r,uint32_t top,uint32_t bottom){
 if(w<=0||h<=0)return;
 r=fminf(r,fminf(h,w)/2);
 uint32_t upper=mix(top,bottom,r/h),lower=mix(top,bottom,1-r/h);
 // Corner discs first in their row's colour, then the gradient quads over everything but the corners.
 if(r>=1){
  u32 a=colour(mix(top,upper,.5f),255),c=colour(mix(lower,bottom,.5f),255);
  C2D_DrawCircleSolid(x+r,y+r,.5f,r,a);C2D_DrawCircleSolid(x+w-r,y+r,.5f,r,a);
  C2D_DrawCircleSolid(x+r,y+h-r,.5f,r,c);C2D_DrawCircleSolid(x+w-r,y+h-r,.5f,r,c);
  C2D_DrawRectangle(x+r,y,.5f,w-2*r,r,colour(top,255),colour(top,255),colour(upper,255),colour(upper,255));
  C2D_DrawRectangle(x+r,y+h-r,.5f,w-2*r,r,colour(lower,255),colour(lower,255),colour(bottom,255),colour(bottom,255));
 }
 C2D_DrawRectangle(x,y+r,.5f,w,h-2*r,colour(upper,255),colour(upper,255),colour(lower,255),colour(lower,255));
}

void ui_background(void){
 C2D_DrawRectangle(0,0,.5f,320,240,colour(0xcdeeff,255),colour(0xcdeeff,255),colour(0xf4fbff,255),colour(0xf4fbff,255));
 // Fixed snow: a few soft flakes, the same every frame.
 static const unsigned short flakes[][3]={{18,46,2},{61,120,1},{97,70,2},{140,160,1},{176,52,1},{214,128,2},{251,84,1},{292,170,2},{38,182,1},{120,196,2},{230,190,1},{300,40,1},{160,98,1},{80,150,1}};
 for(unsigned i=0;i<sizeof flakes/sizeof *flakes;i++)C2D_DrawCircleSolid(flakes[i][0],flakes[i][1],.5f,flakes[i][2],colour(0xffffff,200));
}

void ui_logo(float x,float y,UILogoSize size,float s){
 const UILogoRun *runs=size==UI_LOGO_SMALL?UI_LOGO_16:size==UI_LOGO_MEDIUM?UI_LOGO_24:UI_LOGO_32;
 unsigned count=size==UI_LOGO_SMALL?sizeof UI_LOGO_16/sizeof *UI_LOGO_16:size==UI_LOGO_MEDIUM?sizeof UI_LOGO_24/sizeof *UI_LOGO_24:sizeof UI_LOGO_32/sizeof *UI_LOGO_32;
 for(unsigned i=0;i<count;i++)C2D_DrawRectSolid(x+runs[i].x*s,y+runs[i].y*s,.5f,runs[i].length*s,s,colour(runs[i].rgb,255));
}
float ui_logo_width(UILogoSize size,float s){return (size==UI_LOGO_SMALL?UI_LOGO_16_W:size==UI_LOGO_MEDIUM?UI_LOGO_24_W:UI_LOGO_32_W)*s;}

void ui_header(void){
 ui_round(0,0,320,29,0,SHADOW,60);
 C2D_DrawRectangle(0,0,.5f,320,27,colour(0x075c99,255),colour(0x075c99,255),colour(0x063c68,255),colour(0x063c68,255));
 C2D_DrawRectSolid(0,0,.5f,320,1,colour(0x7fd2ff,255));
 // The micro-logo sits on a small white disc like the app's profile badge.
 C2D_DrawCircleSolid(14,13.5f,.5f,11,colour(0xffffff,255));C2D_DrawCircleSolid(14,13.5f,.5f,10,colour(0xdff3ff,255));
 ui_logo(14-UI_LOGO_16_W/2.0f,13.5f-UI_LOGO_16_H/2.0f,UI_LOGO_SMALL,1);
}

bool ui_button(float x,float y,float w,float h,bool pressed,bool enabled){
 float r=fminf(8,h/2.6f);
 if(!enabled){ui_round(x,y+1,w,h,r,SHADOW,40);ui_round(x,y,w,h,r,0xffffff,255);ui_round(x+1.5f,y+1.5f,w-3,h-3,r-1,0xc9dce8,255);return false;}
 if(!pressed)ui_round(x,y+2,w,h,r,SHADOW,90);
 float dy=pressed?1:0;
 ui_round(x,y+dy,w,h,r,0xffffff,255);
 ui_round_gradient(x+1.5f,y+1.5f+dy,w-3,h-3,r-1.5f,pressed?BUTTON_DOWN_TOP:BUTTON_TOP,pressed?BUTTON_DOWN_BOTTOM:BUTTON_BOTTOM);
 // Gloss on the upper half, as on the app's round buttons.
 if(!pressed&&h>=14)ui_round(x+4,y+3,w-8,(h-6)*.42f,fminf(r,(h-6)*.21f),0xffffff,16);
 return pressed;
}

void ui_box(float x,float y,float w,float h,uint32_t fill){
 bool down=ui_touching(x,y,w,h);
 if(fill==0x0085c7){ui_button(x,y,w,h,down,true);return;}
 float r=fminf(7,h/3);
 if(fill==0xffffff){
  ui_round(x,y+1.5f,w,h,r,SHADOW,34);
  ui_round(x,y,w,h,r,down?0x4fc0ff:CARD_STROKE,255);
  ui_round(x+1,y+1,w-2,h-2,r-1,down?0xe2f5ff:0xffffff,255);
  return;
 }
 if(fill==INK||fill==0x006699){ui_round(x,y+1,w,h,r,SHADOW,60);ui_round_gradient(x,y,w,h,r,fill==INK?0x0b5a99:0x075c99,fill==INK?0x062f57:0x063c68);return;}
 ui_round(x,y,w,h,fminf(r,4),mix(fill,0x000000,.35f),255);
 ui_round(x+1,y+1,w-2,h-2,fminf(r,4)-1,fill,255);
}

void ui_fill(float x,float y,float w,float h,uint32_t rgb){
 if(w>=320&&h>=240){
  C2D_DrawRectangle(x,y,.5f,w,h,colour(mix(rgb,0xffffff,.18f),255),colour(mix(rgb,0xffffff,.18f),255),colour(mix(rgb,0x000000,.22f),255),colour(mix(rgb,0x000000,.22f),255));
  return;
 }
 ui_round(x,y,w,h,fminf(6,h/3),rgb,255);
}

void ui_tab_bar(void){
 C2D_DrawRectangle(0,205,.5f,320,35,colour(0x0d4a85,255),colour(0x0d4a85,255),colour(0x062646,255),colour(0x062646,255));
 C2D_DrawRectSolid(0,205,.5f,320,1,colour(0x4fc0ff,180));
}
void ui_tab(float x,float y,float w,float h,bool active){
 if(active)ui_button(x,y,w,h,false,true);
 else if(ui_touching(x,y,w,h))ui_round(x,y,w,h,8,0xffffff,40);
}

void ui_label(float x,float y,float w,float h){
 ui_round(x,y+2,w,h,h/2,SHADOW,80);
 ui_round(x,y,w,h,h/2,0xffffff,230);
 ui_round_gradient(x+1.5f,y+1.5f,w-3,h-3,(h-3)/2,0x075c99,0x063c68);
 ui_logo(x+5,y+(h-UI_LOGO_16_H)/2,UI_LOGO_SMALL,1);
}
