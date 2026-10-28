#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "swl_gpu.h"
void wear_begin(void);
void wear_want(unsigned id);
void wear_want_pose(unsigned id,unsigned rotation,unsigned variation);
void wear_update(void);
void wear_draw(unsigned id,unsigned action,unsigned clock,float x,float y,float scale);
void wear_draw_pose(unsigned id,unsigned rotation,unsigned variation,unsigned clock,float x,float y,float scale);
bool wear_pose_info(unsigned id,unsigned rotation,unsigned *rotations,unsigned *variations);
bool wear_pose_view(unsigned id,unsigned rotation,unsigned variation,unsigned clock,SWLGPUAtlas *atlas,SWLArt *art);
bool wear_pose_contains(unsigned id,unsigned rotation,unsigned variation,unsigned clock,float x,float y,float scale,float px,float py);
void wear_fit(unsigned id,float x,float y,float width,float height);
bool wear_contains(unsigned id,float x,float y,float scale,float px,float py);
void wear_free(void);
size_t wear_bytes(void);
