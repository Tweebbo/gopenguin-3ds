#include "wear_texture.h"
#include <3ds/util/decompress.h>
#include <stdlib.h>
#include <string.h>

static unsigned read16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
void wear_texture_free(WearTexture *texture){free(texture->pixels);memset(texture,0,sizeof *texture);}

bool wear_texture_decode(WearTexture *out,const void *encoded,size_t bytes,unsigned width,unsigned height){
 memset(out,0,sizeof *out);
 const unsigned char *data=encoded;
 if(!data||bytes<21||read16(data)!=1||data[2]&0xc0||data[3]!=0||data[4]!=0)return false;
 if(width!=(8u<<(data[2]&7))||height!=(8u<<((data[2]>>3)&7)))return false;
 size_t need=(size_t)width*height*4;
 if(need>2u*1024u*1024u)return false;
 WearSubTexture sub={(uint16_t)read16(data+5),(uint16_t)read16(data+7),
  read16(data+9)/1024.0f,read16(data+11)/1024.0f,read16(data+13)/1024.0f,read16(data+15)/1024.0f};
 if(!sub.width||!sub.height||sub.width>width||sub.height>height||sub.left<0||sub.bottom<0||sub.right>1||sub.top>1||sub.left>=sub.right||sub.bottom>=sub.top)return false;
 decompressType type;size_t decoded=0;
 if(decompressHeader(&type,&decoded,NULL,(void *)(data+17),bytes-17)<0||decoded!=need)return false;
 void *pixels=malloc(need);if(!pixels)return false;
 if(!decompress(pixels,need,NULL,(void *)(data+17),bytes-17)){free(pixels);return false;}
 *out=(WearTexture){width,height,sub,need,pixels};return true;
}
