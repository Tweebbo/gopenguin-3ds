#include "wear_loader.h"
#include "wear_budget.h"
#include "asset_cache.h"
#include "hash.h"
#include <3ds/util/decompress.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static pthread_t mainThread;
static pthread_mutex_t gateMutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gateCond=PTHREAD_COND_INITIALIZER;
static bool blockDecode,decodeEntered,allowMain,denyThreads;
static unsigned hashes,decodes;
static void pause_ms(unsigned ms){struct timespec t={ms/1000,(long)(ms%1000)*1000000};nanosleep(&t,NULL);}
static unsigned read32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int __real_pthread_create(pthread_t *,const pthread_attr_t *,void *(*)(void *),void *);
int __wrap_pthread_create(pthread_t *thread,const pthread_attr_t *attrs,void *(*fn)(void *),void *user){return denyThreads?EAGAIN:__real_pthread_create(thread,attrs,fn,user);}

bool __real_hash_file_cancel(FILE *,const unsigned char[32],bool (*)(void *),void *);
bool __wrap_hash_file_cancel(FILE *f,const unsigned char sha[32],bool (*cancel)(void *),void *user){
 assert(allowMain||!pthread_equal(pthread_self(),mainThread));
 hashes++;return __real_hash_file_cancel(f,sha,cancel,user);
}
ssize_t decompressHeader(decompressType *type,size_t *out,decompressCallback cb,void *data,size_t bytes){
 assert(!cb);if(bytes<4)return -1;const unsigned char *p=data;if(p[0])return -1;*type=DECOMPRESS_DUMMY;*out=read32(p)>>8;return 4;
}
bool decompressV(const decompressIOVec *out,size_t count,decompressCallback cb,void *data,size_t bytes){
 assert(allowMain||!pthread_equal(pthread_self(),mainThread));assert(count==1&&!cb);
 pthread_mutex_lock(&gateMutex);decodeEntered=true;pthread_cond_broadcast(&gateCond);
 while(blockDecode)pthread_cond_wait(&gateCond,&gateMutex);
 pthread_mutex_unlock(&gateMutex);
 decodes++;const unsigned char *p=data;
 if(bytes<4||p[0]||read32(p)>>8!=out->size||bytes-4!=out->size)return false;
 memcpy(out->data,p+4,out->size);return true;
}
bool asset_cache_request(unsigned id,char *path,size_t cap){
 assert(allowMain||!pthread_equal(pthread_self(),mainThread));
 const char *file=id==600||id==asset_key(ASSET_FURNITURE,1)?"assets/sprite.gpa":id==asset_key(ASSET_MOVIE,1)?"assets/movie.gpk":NULL;
 if(!file)return false;
 snprintf(path,cap,"%s",file);return true;
}
static WearLoadResult receive(void){
 WearLoadResult r={0};for(unsigned i=0;i<2000;i++){if(wear_loader_take(&r))return r;pause_ms(1);}assert(!"Worker did not finish");return r;
}
static bool stop_now(void *unused){(void)unused;return true;}
static void check_budget(void){
 const size_t m=1024u*1024u;
 assert(wear_budget_limit(0,7*m)==0);
 assert(wear_budget_limit(2*m,14*m)==8*m);
 assert(wear_budget_limit(2*m,18*m)==12*m);
 assert(wear_budget_limit(12*m,2*m)==6*m);
 assert(wear_budget_limit(0,40*m)==WEAR_BUDGET_MAX);
 assert(wear_budget_limit(SIZE_MAX,SIZE_MAX)==WEAR_BUDGET_MAX);
}
static void check_texture_bounds(void){
 unsigned char data[277];FILE *f=fopen("assets/texture.t3x","rb");assert(f&&fread(data,1,sizeof data,f)==sizeof data);fclose(f);
 WearTexture t;allowMain=true;assert(wear_texture_decode(&t,data,sizeof data,8,8));assert(t.bytes==256);wear_texture_free(&t);
 for(unsigned i=0;i<17;i++){
  unsigned char bad[277];memcpy(bad,data,sizeof bad);bad[i]^=0x80;
  if(wear_texture_decode(&t,bad,sizeof bad,8,8))wear_texture_free(&t);
 }
 assert(!wear_texture_decode(&t,data,16,8,8));
 assert(!wear_texture_decode(&t,data,sizeof data-1,8,8));
 assert(!wear_texture_decode(&t,data,sizeof data,16,8));
 data[3]=4;assert(!wear_texture_decode(&t,data,sizeof data,8,8));data[3]=0;
 data[17]=0xff;assert(!wear_texture_decode(&t,data,sizeof data,8,8));
 allowMain=false;
}
int main(void){
 mainThread=pthread_self();check_budget();check_texture_bounds();
 assert(wear_loader_core()==-3);assert(wear_loader_start());assert(wear_loader_core()==-2);
 pthread_mutex_lock(&gateMutex);blockDecode=true;decodeEntered=false;pthread_mutex_unlock(&gateMutex);
 assert(wear_loader_request((WearLoadRequest){429,1,1}));
 pthread_mutex_lock(&gateMutex);while(!decodeEntered)pthread_cond_wait(&gateCond,&gateMutex);pthread_mutex_unlock(&gateMutex);
 WearLoadResult result={0};
 for(unsigned i=0;i<1000;i++){assert(wear_loader_busy());assert(!wear_loader_take(&result));assert(!wear_loader_request((WearLoadRequest){430,1,1}));}
 wear_loader_cancel();
 pthread_mutex_lock(&gateMutex);blockDecode=false;pthread_cond_broadcast(&gateCond);pthread_mutex_unlock(&gateMutex);
 for(unsigned i=0;wear_loader_busy()&&i<2000;i++)pause_ms(1);
 assert(!wear_loader_busy()&&!wear_loader_take(&result));
 assert(wear_loader_request((WearLoadRequest){430,30,1}));result=receive();
 assert(result.status==WEAR_LOAD_TEXTURE&&result.garment.frames&&result.texture.bytes==256);
 for(unsigned i=0;i<256;i++)assert(((unsigned char *)result.texture.pixels)[i]==0x5a);
 wear_loader_result_free(&result);
 assert(wear_loader_request((WearLoadRequest){431,1,1}));result=receive();
 assert(result.status==WEAR_LOAD_FAILED&&!result.texture.pixels);wear_loader_result_free(&result);
 assert(wear_loader_request((WearLoadRequest){600,2,1}));result=receive();
 assert(result.status==WEAR_LOAD_TEXTURE&&result.sprite&&result.page==1);
 for(unsigned i=0;i<256;i++)assert(((unsigned char *)result.texture.pixels)[i]==0xa5);
 wear_loader_result_free(&result);
 assert(wear_loader_request((WearLoadRequest){asset_key(ASSET_FURNITURE,1),1,1}));result=receive();
 assert(result.status==WEAR_LOAD_TEXTURE&&result.page==0);wear_loader_result_free(&result);
 assert(wear_loader_request((WearLoadRequest){asset_key(ASSET_MOVIE,1),1,1}));result=receive();
 assert(result.status==WEAR_LOAD_MOVIE&&!result.texture.pixels);wear_loader_result_free(&result);
 assert(wear_loader_request((WearLoadRequest){999,1,1}));result=receive();
 assert(result.status==WEAR_LOAD_WAITING);wear_loader_result_free(&result);
 wear_loader_request_stop();assert(wear_loader_stop_within(1500));assert(wear_loader_core()==-3);assert(wear_loader_stop_within(0));
 denyThreads=true;assert(!wear_loader_start());
 allowMain=true;wear_loader_prepare(&result,(WearLoadRequest){430,1,1},NULL,NULL);
 assert(result.status==WEAR_LOAD_TEXTURE);wear_loader_result_free(&result);
 wear_loader_prepare(&result,(WearLoadRequest){430,1,1},stop_now,NULL);assert(result.status==WEAR_LOAD_FAILED);wear_loader_result_free(&result);
 Garment garment;assert(!garment_load_cancel(&garment,"romfs:/clothing/430.gpa",stop_now,NULL));
 denyThreads=false;assert(wear_loader_start());assert(wear_loader_stop_within(1500));
 printf("PASS: asynchronous hash/decode, nonblocking requests, stale-result cancellation, corrupt files, GPA1/GPA2 pages, movie handoff, restart/fallback and adaptive budgets (%u hashes, %u decodes).\n",hashes,decodes);
 return 0;
}
