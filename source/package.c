#include "package.h"
#include "hash.h"
#include <stdlib.h>
#include <string.h>
static unsigned u32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
bool package_open(GPPackage *p,const char *path){
 memset(p,0,sizeof *p);if(strlen(path)>=sizeof p->path)return false;FILE *file=fopen(path,"rb");if(!file)return false;
 unsigned char header[64],table[80];long bytes=0;bool ok=false;
 if(fseek(file,0,SEEK_END)||(bytes=ftell(file))<80||(unsigned long)bytes>GP_PACKAGE_MAX||fseek(file,0,SEEK_SET)||fread(header,1,64,file)!=64)goto done;
 if(memcmp(header,"GPK1",4)||u32(header+4)!=1||u32(header+8)!=(unsigned)bytes||(p->count=u32(header+12))<2||p->count>10||!hash_file(file,header+32))goto done;
 if(fseek(file,64,SEEK_SET)||fread(table,8,p->count,file)!=p->count)goto done;
 unsigned expected=64+p->count*8;
 for(unsigned i=0;i<p->count;i++){p->offset[i]=u32(table+i*8);p->bytes[i]=u32(table+i*8+4);if(p->offset[i]!=expected||!p->bytes[i]||p->bytes[i]>(unsigned)bytes-expected)goto done;expected+=p->bytes[i];}
 if(expected!=(unsigned)bytes)goto done;
 strcpy(p->path,path);ok=true;
done:fclose(file);if(!ok)memset(p,0,sizeof *p);return ok;
}
FILE *package_part(const GPPackage *p,unsigned index){if(index>=p->count)return NULL;FILE *file=fopen(p->path,"rb");if(file&&fseek(file,(long)p->offset[index],SEEK_SET)){fclose(file);file=NULL;}return file;}
void *package_read(const GPPackage *p,unsigned index,size_t limit,size_t *bytes){if(index>=p->count||p->bytes[index]>limit)return NULL;FILE *file=package_part(p,index);if(!file)return NULL;void *data=malloc(p->bytes[index]);if(!data||fread(data,1,p->bytes[index],file)!=p->bytes[index]){free(data);data=NULL;}fclose(file);if(data&&bytes)*bytes=p->bytes[index];return data;}
