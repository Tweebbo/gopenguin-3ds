#pragma once
#include "common.h"
typedef struct {void *data;unsigned width,height,size;} C3D_Tex;
typedef struct {unsigned width,height;float left,top,right,bottom;} Tex3DS_SubTexture;
typedef struct C3D_RenderTarget C3D_RenderTarget;
typedef struct {C3D_Tex *tex;const Tex3DS_SubTexture *sub;} C2D_Image;
enum {GPU_RGBA8,GPU_LINEAR,GPU_CLAMP_TO_EDGE};
bool C3D_TexInit(C3D_Tex *,unsigned,unsigned,int);
void C3D_TexDelete(C3D_Tex *);
void C3D_TexFlush(C3D_Tex *);
void C3D_TexSetFilter(C3D_Tex *,int,int);
void C3D_TexSetWrap(C3D_Tex *,int,int);
void C2D_DrawImageAt(C2D_Image,float,float,float,const void *,float,float);
