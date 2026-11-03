#include "asset_cache.h"
#include "jobthread.h"
#include "package.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void pending(unsigned maximum){u64 until=now_ms()+maximum;while(asset_cache_pending()&&now_ms()<until)job_sleep(5);assert(!asset_cache_pending());}
int main(int argc,char **argv){
 assert(argc==4);char path[256];bool priority=!strcmp(argv[3],"priority");
 assert(asset_cache_start("http://127.0.0.1:18896",argv[1]));asset_cache_pause(true);
 for(unsigned i=1;i<=32;i++)assert(!asset_cache_request(asset_key(ASSET_BOOK,i),path,sizeof path));
 assert(asset_cache_pending()==32);
 /* Polling a visible resource must not move it to the back of the queue. */
 for(unsigned i=32;i;i--)assert(!asset_cache_request(asset_key(ASSET_BOOK,i),path,sizeof path));
 asset_cache_pause(false);
 if(priority){
  u64 until=now_ms()+3000;while(access(argv[2],F_OK)&&now_ms()<until)job_sleep(5);assert(!access(argv[2],F_OK));
  unsigned key=asset_key(ASSET_MOVIE,200002);u64 start=now_ms();
  assert(!asset_cache_request_urgent(key,path,sizeof path));
  while(!asset_cache_request_urgent(key,path,sizeof path)&&now_ms()-start<1500)job_sleep(5);
  assert(asset_cache_status(key)==ASSET_READY);GPPackage package;assert(package_open(&package,path));
  printf("Igloo dependency ready in %llu ms with a stalled request and 32 queued resources.\n",(unsigned long long)(now_ms()-start));
 }
 pending(5000);
 for(unsigned i=1;i<=32;i++)assert(asset_cache_request(asset_key(ASSET_BOOK,i),path,sizeof path));
 for(unsigned i=200003;i<=200004;i++){
  unsigned key=asset_key(ASSET_MOVIE,i);assert(!asset_cache_request_urgent(key,path,sizeof path));pending(2000);
  assert(asset_cache_status(key)==(i==200003?ASSET_RETRY:ASSET_UNAVAILABLE));
  assert(!asset_cache_request_urgent(key,path,sizeof path));assert(!asset_cache_pending());
 }
 assert(!asset_cache_request_urgent(0,path,sizeof path));asset_cache_stop();
 assert(asset_cache_start("http://127.0.0.1:18896",argv[1]));
 unsigned key=asset_key(ASSET_MOVIE,200002);asset_cache_request_urgent(key,path,sizeof path);pending(2000);
 assert(asset_cache_request_urgent(key,path,sizeof path));asset_cache_stop();
 puts("PASS: stable FIFO, reserved foreground slot, preemption recovery, package validation, missing resources, retry cooldown and disk cache reuse.");
}
