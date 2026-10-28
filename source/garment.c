#include "garment.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned read16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned read32(const unsigned char *p){return read16(p)|read16(p+2)<<16;}
void garment_free(Garment *g){free(g->frames);memset(g,0,sizeof *g);}
bool garment_load(Garment *g,const char *path){
 memset(g,0,sizeof *g);FILE *file=fopen(path,"rb");if(!file)return false;
 bool ok=false;unsigned char header[64],sequences[240];long bytes;
 if(fseek(file,0,SEEK_END)||(bytes=ftell(file))<304||(unsigned long)bytes>GARMENT_MAX_BYTES||fseek(file,0,SEEK_SET)||fread(header,1,64,file)!=64)goto done;
 if(memcmp(header,"GPA1",4)||read32(header+4)!=1||read32(header+8)!=(unsigned)bytes)goto done;
 g->width=read16(header+16);g->height=read16(header+18);g->count=read32(header+12);g->textureBytes=read32(header+20);
 if(g->width<8||g->width>1024||g->height<8||g->height>1024||(g->width&(g->width-1))||(g->height&(g->height-1))||g->width*g->height*4>2u*1024u*1024u||!g->count||g->count>4096)goto done;
 g->textureOffset=64+240+g->count*12;
 unsigned scale=read32(header+24);if(scale!=0x3f000000u&&scale!=0x3f800000u)goto done;g->scale=scale==0x3f000000u?.5f:1;
 if(!g->textureBytes||g->textureOffset>(unsigned)bytes||g->textureBytes!=(unsigned)bytes-g->textureOffset||!hash_file(file,header+32))goto done;
 if(fseek(file,64,SEEK_SET)||fread(sequences,1,240,file)!=240)goto done;
 for(unsigned i=0;i<30;i++){unsigned char *s=sequences+i*8;GarmentSequence *out=&g->sequences[i];out->start=read16(s);out->count=read16(s+2);out->loop=read16(s+4);if(!out->count||out->start>=g->count||out->count>g->count-out->start||out->loop>=out->count||read16(s+6))goto done;}
 g->frames=calloc(g->count,sizeof *g->frames);if(!g->frames)goto done;
 for(unsigned i=0;i<g->count;i++){unsigned char frame[12];if(fread(frame,1,12,file)!=12)goto done;GarmentFrame *f=&g->frames[i];f->x=read16(frame);f->y=read16(frame+2);f->w=read16(frame+4);f->h=read16(frame+6);f->left=(int16_t)read16(frame+8);f->top=(int16_t)read16(frame+10);if(f->x>g->width||f->w>g->width-f->x||f->y>g->height||f->h>g->height-f->y||f->left< -4096||f->left>4096||f->top< -4096||f->top>4096)goto done;}
 ok=true;
done:
 fclose(file);if(!ok)garment_free(g);return ok;
}
const GarmentFrame *garment_frame(const Garment *g,unsigned action,unsigned clock){
 if(!g||!g->frames||action<1||action>30)return NULL;
 const GarmentSequence *s=&g->sequences[action-1];unsigned frame=clock<s->count?clock:s->loop+(clock-s->loop)%(s->count-s->loop);return &g->frames[s->start+frame];
}
