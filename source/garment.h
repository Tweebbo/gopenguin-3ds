#pragma once
#include <stdbool.h>
#include <stdint.h>
#define GARMENT_MAX_BYTES (3u*1024u*1024u)
typedef struct {uint16_t start,count,loop;} GarmentSequence;
typedef struct {uint16_t x,y,w,h;int16_t left,top;} GarmentFrame;
typedef struct {unsigned width,height,count,textureOffset,textureBytes;float scale;GarmentSequence sequences[30];GarmentFrame *frames;} Garment;
bool garment_load(Garment *garment,const char *path);
void garment_free(Garment *garment);
const GarmentFrame *garment_frame(const Garment *garment,unsigned action,unsigned clock);
