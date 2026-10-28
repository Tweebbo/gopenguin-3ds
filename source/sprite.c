#include "sprite.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned u16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned u32(const unsigned char *p){return u16(p)|u16(p+2)<<16;}
Sprite *sprite_retain(Sprite *s){if(s)s->references++;return s;}
void sprite_free(Sprite *s){if(!s||--s->references)return;free(s->sequences);free(s->frames);free(s->textures);free(s);}
Sprite *sprite_load(const char *path){
 if(strlen(path)>=256)return NULL;
 FILE *file=fopen(path,"rb");if(!file)return NULL;Sprite *s=calloc(1,sizeof *s);if(!s){fclose(file);return NULL;}s->references=1;unsigned char h[64];long length;
 if(fseek(file,0,SEEK_END)||(length=ftell(file))<104||(unsigned long)length>SPRITE_MAX_BYTES||fseek(file,0,SEEK_SET)||fread(h,1,64,file)!=64)goto fail;
 if(memcmp(h,"GPA2",4)||u32(h+4)!=2||u32(h+8)!=(unsigned)length||!hash_file(file,h+32))goto fail;
 s->count=u32(h+12);s->pages=u32(h+16);s->actions=u32(h+20);s->fps=u16(h+28);unsigned scale=u32(h+24);if(!s->count||s->count>16384||!s->pages||s->pages>64||!s->actions||s->actions>512||!s->fps||s->fps>60||u16(h+30)>1||(scale!=0x3f000000u&&scale!=0x3f800000u))goto fail;s->nativeTimeline=u16(h+30)!=0;s->scale=scale==0x3f000000u?.5f:1;
 unsigned expected=64+s->actions*12+s->count*12+s->pages*16;if(expected>(unsigned)length)goto fail;
 s->sequences=calloc(s->actions,sizeof *s->sequences);s->frames=calloc(s->count,sizeof *s->frames);s->textures=calloc(s->pages,sizeof *s->textures);if(!s->sequences||!s->frames||!s->textures||fseek(file,64,SEEK_SET))goto fail;
 for(unsigned i=0;i<s->actions;i++){unsigned char b[12];if(fread(b,1,12,file)!=12)goto fail;SpriteSequence *q=&s->sequences[i];*q=(SpriteSequence){u16(b),u16(b+2),u16(b+4),u16(b+6),u16(b+8),u16(b+10)};if(!q->count||q->start>=s->count||q->count>s->count-q->start||q->loop>=q->count||q->page>=s->pages||!q->rotation||!q->variation||q->rotation>4096||q->variation>4096)goto fail;if(!i){if(q->rotation!=1||q->variation!=1)goto fail;}else{const SpriteSequence *previous=&s->sequences[i-1];if(q->rotation==previous->rotation){if(q->variation!=previous->variation+1)goto fail;}else if(q->rotation!=previous->rotation+1||q->variation!=1)goto fail;}}
 for(unsigned i=0;i<s->count;i++){unsigned char b[12];if(fread(b,1,12,file)!=12)goto fail;GarmentFrame *f=&s->frames[i];*f=(GarmentFrame){u16(b),u16(b+2),u16(b+4),u16(b+6),(int16_t)u16(b+8),(int16_t)u16(b+10)};if(f->left< -4096||f->left>4096||f->top< -4096||f->top>4096)goto fail;}
 for(unsigned i=0;i<s->pages;i++){unsigned char b[16];if(fread(b,1,16,file)!=16)goto fail;SpriteTexture *t=&s->textures[i];*t=(SpriteTexture){u32(b),u32(b+4),u16(b+8),u16(b+10)};if(t->offset!=expected||!t->bytes||t->bytes>(unsigned)length-expected||t->width<8||t->width>1024||t->height<8||t->height>1024||(t->width&(t->width-1))||(t->height&(t->height-1))||t->width*t->height*4>2u*1024u*1024u||u32(b+12))goto fail;expected+=t->bytes;}
 if(expected!=(unsigned)length)goto fail;
 for(unsigned i=0;i<s->actions;i++){const SpriteSequence *q=&s->sequences[i];const SpriteTexture *t=&s->textures[q->page];for(unsigned n=q->start;n<q->start+q->count;n++){const GarmentFrame *f=&s->frames[n];if(f->x>t->width||f->w>t->width-f->x||f->y>t->height||f->h>t->height-f->y)goto fail;}}
 strcpy(s->path,path);memcpy(s->digest,h+32,32);fclose(file);return s;
fail:fclose(file);sprite_free(s);return NULL;
}
unsigned sprite_rotations(const Sprite *s){return s&&s->actions?s->sequences[s->actions-1].rotation:0;}
unsigned sprite_variations(const Sprite *s,unsigned rotation){if(!s)return 0;unsigned lo=0,hi=s->actions;while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(s->sequences[mid].rotation<=rotation)lo=mid+1;else hi=mid;}return lo&&s->sequences[lo-1].rotation==rotation?s->sequences[lo-1].variation:0;}
const SpriteSequence *sprite_pose(const Sprite *s,unsigned rotation,unsigned variation){if(!s)return NULL;unsigned max=sprite_rotations(s);if(!rotation)rotation=1;if(rotation>max)rotation=max;max=sprite_variations(s,rotation);if(!variation)variation=1;if(variation>max)variation=max;unsigned key=rotation<<16|variation,lo=0,hi=s->actions;while(lo<hi){unsigned mid=lo+(hi-lo)/2;const SpriteSequence *q=&s->sequences[mid];unsigned value=q->rotation<<16|q->variation;if(value<key)lo=mid+1;else hi=mid;}return lo<s->actions?&s->sequences[lo]:NULL;}
const GarmentFrame *sprite_frame(const Sprite *s,const SpriteSequence *q,unsigned clock){if(!s||!q)return NULL;unsigned tick=(unsigned)((uint64_t)clock*s->fps/24),frame=tick<q->count?tick:q->loop+(tick-q->loop)%(q->count-q->loop);return &s->frames[q->start+frame];}
void *sprite_texture_read(const Sprite *s,unsigned page,size_t *bytes){if(!s||page>=s->pages)return NULL;FILE *file=fopen(s->path,"rb");if(!file)return NULL;unsigned char header[64];void *data=NULL;const SpriteTexture *t=&s->textures[page];if(fread(header,1,64,file)!=64||memcmp(header,"GPA2",4)||memcmp(header+32,s->digest,32)||fseek(file,(long)t->offset,SEEK_SET))goto done;data=malloc(t->bytes);if(!data||fread(data,1,t->bytes,file)!=t->bytes){free(data);data=NULL;}if(data&&bytes)*bytes=t->bytes;done:fclose(file);return data;}
