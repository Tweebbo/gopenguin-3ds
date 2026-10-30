#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { NAME_TAG_COLUMNS=5, NAME_TAG_SLOT_W=100, NAME_TAG_SLOT_H=16, NAME_TAG_COUNT=80, NAME_TAG_W=512, NAME_TAG_H=256 };
typedef struct {int advance,left,top,width,height;} NameTag;
bool name_tag_layout(NameTag *tag,const char *name);
/* Both images use PICA's tiled LA8 layout, with their top edge at texture v=1. */
void name_tag_write(uint8_t *target,unsigned slot,const NameTag *tag,const char *name,const uint8_t *font,unsigned fontWidth,unsigned fontHeight);
