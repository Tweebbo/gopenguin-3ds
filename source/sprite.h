#pragma once
#include "garment.h"
#include <stddef.h>
#define SPRITE_MAX_BYTES (24u*1024u*1024u)
typedef struct {unsigned offset,bytes,width,height;} SpriteTexture;
typedef struct {unsigned start,count,loop,page,rotation,variation;} SpriteSequence;
typedef struct Sprite {
 unsigned references,count,actions,pages,fps;bool nativeTimeline;float scale;char path[256];unsigned char digest[32];
 SpriteSequence *sequences;GarmentFrame *frames;SpriteTexture *textures;
} Sprite;
Sprite *sprite_load(const char *path);
Sprite *sprite_retain(Sprite *sprite);
void sprite_free(Sprite *sprite);
const SpriteSequence *sprite_pose(const Sprite *sprite,unsigned rotation,unsigned variation);
const GarmentFrame *sprite_frame(const Sprite *sprite,const SpriteSequence *sequence,unsigned clock);
void *sprite_texture_read(const Sprite *sprite,unsigned page,size_t *bytes);
unsigned sprite_rotations(const Sprite *sprite);
unsigned sprite_variations(const Sprite *sprite,unsigned rotation);
