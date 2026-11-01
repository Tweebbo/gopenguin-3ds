#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
 uint16_t width,height;
 float left,top,right,bottom;
} WearSubTexture;
typedef struct {
 unsigned width,height;
 WearSubTexture sub;
 size_t bytes;
 void *pixels;
} WearTexture;

bool wear_texture_decode(WearTexture *out,const void *encoded,size_t bytes,unsigned width,unsigned height);
void wear_texture_free(WearTexture *texture);
