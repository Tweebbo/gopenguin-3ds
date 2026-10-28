#include "client_data.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct ClientData {uint8_t *data;const uint8_t *products,*safe,*jokes,*tours,*strings;unsigned nProducts,nSafe,nJokes,nTours,stringBytes;};
static unsigned u32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static bool valid(const ClientData *d,unsigned offset){return offset<d->stringBytes&&memchr(d->strings+offset,0,d->stringBytes-offset);}
void client_data_free(ClientData *d){if(d){free(d->data);free(d);}}
ClientData *client_data_load(const char *path){
 FILE *file=fopen(path,"rb");if(!file)return NULL;ClientData *d=calloc(1,sizeof *d);if(!d){fclose(file);return NULL;}long bytes=0;bool ok=false;
 if(fseek(file,0,SEEK_END)||(bytes=ftell(file))<32||bytes>4*1024*1024||fseek(file,0,SEEK_SET))goto done;
 d->data=malloc((size_t)bytes);if(!d->data||fread(d->data,1,(size_t)bytes,file)!=(size_t)bytes)goto done;
 const uint8_t *p=d->data;if(memcmp(p,"GPDB",4)||u32(p+4)!=1||u32(p+8)!=(unsigned)bytes)goto done;
 d->nProducts=u32(p+12);d->nSafe=u32(p+16);d->nJokes=u32(p+20);d->nTours=u32(p+24);d->stringBytes=u32(p+28);
 if(d->nProducts>20000||d->nSafe>4096||d->nJokes>1024||d->nTours>128||!d->stringBytes||d->stringBytes>2*1024*1024||32+d->nProducts*20+d->nSafe*12+d->nJokes*4+d->nTours*8+d->stringBytes!=(unsigned)bytes)goto done;
 d->products=p+32;d->safe=d->products+d->nProducts*20;d->jokes=d->safe+d->nSafe*12;d->tours=d->jokes+d->nJokes*4;d->strings=d->tours+d->nTours*8;if(d->strings[0]||d->strings[d->stringBytes-1])goto done;
 for(unsigned i=0;i<d->nProducts;i++){const uint8_t *row=d->products+i*20;if(u32(row)>3||(!u32(row+4)&&u32(row)<2)||u32(row+4)>1000000||!valid(d,u32(row+12)))goto done;if(i){const uint8_t *previous=row-20;if(u32(previous)>u32(row)||(u32(previous)==u32(row)&&u32(previous+4)>=u32(row+4)))goto done;}}
 for(unsigned i=0;i<d->nSafe;i++)if(!valid(d,u32(d->safe+i*12+8)))goto done;
 for(unsigned i=0;i<d->nJokes;i++)if(!valid(d,u32(d->jokes+i*4)))goto done;
 for(unsigned i=0;i<d->nTours;i++)if(!valid(d,u32(d->tours+i*8+4)))goto done;
 ok=true;
done:fclose(file);if(!ok){client_data_free(d);return NULL;}return d;
}
bool client_product(const ClientData *d,unsigned kind,unsigned id,Product *out){if(!d)return false;unsigned lo=0,hi=d->nProducts;while(lo<hi){unsigned mid=lo+(hi-lo)/2;const uint8_t *r=d->products+mid*20;unsigned k=u32(r),n=u32(r+4);if(k==kind&&n==id){*out=(Product){kind,id,u32(r+8),u32(r+16),(const char *)d->strings+u32(r+12)};return true;}if(k<kind||(k==kind&&n<id))lo=mid+1;else hi=mid;}return false;}
bool client_safe(const ClientData *d,unsigned id,SafeMessage *out){if(!d)return false;for(unsigned i=0;i<d->nSafe;i++){const uint8_t *r=d->safe+i*12;if(u32(r)==id){*out=(SafeMessage){id,u32(r+4),(const char *)d->strings+u32(r+8)};return true;}}return false;}
unsigned client_safe_children(const ClientData *d,unsigned parent,unsigned offset,SafeMessage *out,unsigned capacity){if(!d)return 0;unsigned count=0;for(unsigned i=0;i<d->nSafe;i++){const uint8_t *r=d->safe+i*12;if(u32(r+4)!=parent)continue;if(count>=offset&&count-offset<capacity)out[count-offset]=(SafeMessage){u32(r),parent,(const char *)d->strings+u32(r+8)};count++;}return count;}
const char *client_tour(const ClientData *d,unsigned room){if(d)for(unsigned i=0;i<d->nTours;i++){const uint8_t *r=d->tours+i*8;if(u32(r)==room)return (const char *)d->strings+u32(r+4);}return "";}
const char *client_joke(const ClientData *d,unsigned index){return d&&index<d->nJokes?(const char *)d->strings+u32(d->jokes+index*4):"";}
