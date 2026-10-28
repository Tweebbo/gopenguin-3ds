#pragma once
#include <3ds.h>
#include <citro2d.h>
#include "swl.h"

typedef struct {C3D_Tex *texture;const Tex3DS_SubTexture *sub;} SWLGPUAtlas;
bool swl_gpu_init(void);
bool swl_gpu_reserve(unsigned groups);
void swl_gpu_free(void);
void swl_gpu_frame(void);
bool swl_gpu_embed(SWLScene *scene,SWLMatrix view,const SWLGPUAtlas *atlases);
bool swl_gpu_draw(SWLScene *scene,C3D_RenderTarget *target,unsigned width,unsigned height,
 SWLMatrix view,int layer,float interpolation,const SWLGPUAtlas *atlases,
 bool (*depth)(void *user,float value),bool (*needed)(void *user,float value),void *user,bool screenClip);
