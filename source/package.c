#include "package.h"
#include "resource_stdio.h"
#include "hash.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
static unsigned u32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
bool package_open(GPPackage *p,const char *path){
 memset(p,0,sizeof *p);if(strlen(path)>=sizeof p->path)return false;FILE *file=fopen(path,"rb");if(!file)return false;
 unsigned char header[64],table[224];long bytes=0;bool ok=false;
 if(fseek(file,0,SEEK_END)||(bytes=ftell(file))<80||(unsigned long)bytes>GP_PACKAGE_MAX||fseek(file,0,SEEK_SET)||fread(header,1,64,file)!=64)goto done;
 bool packed=!memcmp(header,"GPK2",4);unsigned stride=packed?16:8;
 if((!packed&&memcmp(header,"GPK1",4))||u32(header+4)!=(packed?2u:1u)||u32(header+8)!=(unsigned)bytes||(p->count=u32(header+12))<2||p->count>13||!hash_file(file,header+32))goto done;
 if(fseek(file,64,SEEK_SET)||fread(table,stride,p->count,file)!=p->count)goto done;
 unsigned expected=64+p->count*stride,decoded=0;
 if(expected>(unsigned)bytes)goto done;
 for(unsigned i=0;i<p->count;i++){
  const unsigned char *entry=table+i*stride;p->offset[i]=u32(entry);p->stored[i]=u32(entry+4);p->bytes[i]=packed?u32(entry+8):p->stored[i];p->codec[i]=packed?u32(entry+12):0;
  if(p->offset[i]!=expected||!p->stored[i]||p->stored[i]>(unsigned)bytes-expected||!p->bytes[i]||p->bytes[i]>GP_PACKAGE_MAX-decoded||p->codec[i]>1||(!p->codec[i]&&p->stored[i]!=p->bytes[i]))goto done;
  expected+=p->stored[i];decoded+=p->bytes[i];
 }
 if(expected!=(unsigned)bytes)goto done;
 strcpy(p->path,path);ok=true;
done:fclose(file);if(!ok)memset(p,0,sizeof *p);return ok;
}
FILE *package_part(const GPPackage *p,unsigned index){if(index>=p->count||p->codec[index])return NULL;FILE *file=fopen(p->path,"rb");if(file&&fseek(file,(long)p->offset[index],SEEK_SET)){fclose(file);file=NULL;}return file;}
void *package_read(const GPPackage *p,unsigned index,size_t limit,size_t *bytes){
 if(bytes)*bytes=0;
 if(index>=p->count||!p->bytes[index]||p->bytes[index]>limit)return NULL;
 FILE *file=fopen(p->path,"rb");if(!file)return NULL;
 void *data=NULL;if(fseek(file,(long)p->offset[index],SEEK_SET))goto done;
 data=malloc(p->bytes[index]);if(!data)goto done;
 if(!p->codec[index]){if(fread(data,1,p->bytes[index],file)!=p->bytes[index])goto fail;}
 else{
  unsigned char input[8192],overflow;z_stream stream={0};bool valid=false,stopped=false;unsigned remaining=p->stored[index];
  if(p->codec[index]!=1||inflateInit(&stream)!=Z_OK)goto fail;
  stream.next_out=data;stream.avail_out=p->bytes[index];
  while(remaining){
   unsigned count=remaining>sizeof input?sizeof input:remaining;
   if(fread(input,1,count,file)!=count)break;
   remaining-=count;stream.next_in=input;stream.avail_in=count;
   while(stream.avail_in){
    if(!stream.avail_out){stream.next_out=&overflow;stream.avail_out=1;}
    unsigned long previousIn=stream.total_in,previousOut=stream.total_out;
    int status=inflate(&stream,Z_NO_FLUSH);
    if(status==Z_STREAM_END){valid=!remaining&&!stream.avail_in&&stream.total_out==p->bytes[index];stopped=true;break;}
    if(status!=Z_OK||stream.total_out>p->bytes[index]||(previousIn==stream.total_in&&previousOut==stream.total_out)){stopped=true;break;}
   }
   if(stopped)break;
  }
  inflateEnd(&stream);if(!valid)goto fail;
 }
 if(bytes)*bytes=p->bytes[index];
 goto done;
fail:free(data);data=NULL;
done:fclose(file);return data;
}
