#include "wear.h"
#include "wear_loader.h"
#include "wear_budget.h"
#include "asset_cache.h"
#include "movie_bank.h"
#include <3ds/util/decompress.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MIB (1024u*1024u)
static pthread_t mainThread;
static size_t allocated,other;
static u64 clockMS;
static void pause_ms(unsigned ms){struct timespec t={ms/1000,(long)(ms%1000)*1000000};nanosleep(&t,NULL);}
u64 now_ms(void){return clockMS;}
u64 osGetTime(void){return clockMS;}
u32 linearSpaceFree(void){assert(allocated+other<=28*MIB);return (u32)(28*MIB-allocated-other);}
bool C3D_TexInit(C3D_Tex *t,unsigned w,unsigned h,int format){
 assert(pthread_equal(pthread_self(),mainThread)&&format==GPU_RGBA8);
 size_t bytes=(size_t)w*h*4;if(bytes>linearSpaceFree())return false;
 void *data=malloc(bytes);if(!data)return false;
 *t=(C3D_Tex){data,w,h,(unsigned)bytes};allocated+=bytes;return true;
}
void C3D_TexDelete(C3D_Tex *t){assert(pthread_equal(pthread_self(),mainThread));allocated-=t->size;free(t->data);memset(t,0,sizeof *t);}
void C3D_TexFlush(C3D_Tex *t){assert(pthread_equal(pthread_self(),mainThread)&&t->data);}
void C3D_TexSetFilter(C3D_Tex *t,int a,int b){(void)t;(void)a;(void)b;}
void C3D_TexSetWrap(C3D_Tex *t,int a,int b){(void)t;(void)a;(void)b;}
bool asset_cache_request(unsigned id,char *path,size_t cap){(void)id;(void)path;(void)cap;return false;}
static unsigned read32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
ssize_t decompressHeader(decompressType *type,size_t *out,decompressCallback cb,void *data,size_t bytes){
 assert(!cb);if(bytes<4)return -1;const unsigned char *p=data;if(p[0])return -1;*type=DECOMPRESS_DUMMY;*out=read32(p)>>8;return 4;
}
bool decompressV(const decompressIOVec *out,size_t count,decompressCallback cb,void *data,size_t bytes){
 assert(!pthread_equal(pthread_self(),mainThread)&&count==1&&!cb);
 const unsigned char *p=data;if(bytes<4||p[0]||read32(p)>>8!=out->size||bytes-4!=out->size)return false;
 memcpy(out->data,p+4,out->size);return true;
}
MovieBank *movie_bank_load(const char *p,size_t bytes,const char **error){(void)p;(void)bytes;(void)error;return NULL;}
void movie_bank_free(MovieBank *b){(void)b;}
size_t movie_bank_bytes(const MovieBank *b){(void)b;return 0;}
SWLScene *movie_bank_scene(MovieBank *b){(void)b;return NULL;}
float swl_rate(const SWLScene *s){(void)s;return 24;}
bool swl_tick(SWLScene *s){(void)s;return true;}
unsigned swl_frames(SWLScene *s,const char *p){(void)s;(void)p;return 0;}
bool swl_target(SWLScene *s,const char *p,unsigned f,const char *l,bool play){(void)s;(void)p;(void)f;(void)l;(void)play;return false;}

static void step(unsigned first,unsigned count){
 clockMS+=33;wear_begin();for(unsigned i=0;i<count;i++)wear_want(first+i);wear_update();pause_ms(1);
 assert(wear_bytes()==allocated&&wear_bytes()<=wear_budget());
 assert(linearSpaceFree()>=WEAR_LINEAR_RESERVE);
}
static void fill(unsigned first,unsigned count,unsigned resident){
 for(unsigned frame=0;frame<2000;frame++){
  step(first,count);
  unsigned loaded=0;for(unsigned i=0;i<count;i++)loaded+=wear_pose_ready(first+i,1,1);
  if(loaded==resident)return;
 }
 assert(!"Cache did not settle");
}
int main(void){
 mainThread=pthread_self();fill(1001,24,16);
 assert(wear_bytes()==16*MIB&&wear_budget()==16*MIB);
 for(unsigned i=0;i<16;i++)assert(wear_pose_ready(1001+i,1,1));
 assert(!wear_pose_ready(1017,1,1));
 wear_trim_unwanted();assert(wear_bytes()==16*MIB);
 for(unsigned i=0;i<20;i++)step(1001,6);
 assert(wear_bytes()==6*MIB);
 for(unsigned i=0;i<6;i++)assert(wear_pose_ready(1001+i,1,1));
 assert(!wear_pose_ready(1007,1,1));
 fill(1001,24,16);
 other=8*MIB;step(1001,24);assert(wear_bytes()==12*MIB&&wear_budget()==12*MIB);
 assert(wear_pose_ready(1001,1,1));
 fill(1019,6,6);for(unsigned i=1019;i<=1024;i++)assert(wear_pose_ready(i,1,1));
 wear_free();assert(!wear_bytes()&&!allocated);
 other=26*MIB;
 for(unsigned i=0;i<20;i++){clockMS+=33;wear_begin();wear_update();pause_ms(1);assert(!wear_bytes()&&!allocated);}
 other=0;fill(1001,24,16);assert(wear_budget()==16*MIB);
 wear_request_stop();assert(wear_stop_within(1500));wear_free();assert(!allocated);
 puts("PASS: actual cache admits 16 MiB, trims hidden clothing at the next update while preserving wanted items, shrinks with 8 MiB reserved, loads catalogue pages and releases/restores textures around a minigame.");
 return 0;
}
