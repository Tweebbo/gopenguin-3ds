#include "gp_update.h"
#include "jobthread.h"
#include "transport/download.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *scenario;
static unsigned starts,writes,finishes,cancels,queries,downloads,bytes;
static bool sleepAllowed=true,homeAllowed=true;
struct TestThread {pthread_t id;void (*entry)(void *);void *arg;};
static void *entry(void *arg){Thread t=arg;t->entry(t->arg);return NULL;}
Thread threadCreate(void (*fn)(void *),void *arg,size_t stack,s32 priority,int core,bool detached){
 (void)stack;(void)priority;(void)core;assert(!detached);Thread t=calloc(1,sizeof *t);assert(t);t->entry=fn;t->arg=arg;assert(!pthread_create(&t->id,NULL,entry,t));return t;
}
Result threadJoin(Thread t,u64 timeout){(void)timeout;return pthread_join(t->id,NULL);}
void threadFree(Thread t){free(t);}
Result svcGetThreadPriority(s32 *priority,Handle handle){(void)handle;*priority=0x30;return 0;}
void svcSleepThread(s64 ns){struct timespec t={ns/1000000000,ns%1000000000};nanosleep(&t,NULL);}
void aptSetSleepAllowed(bool allowed){sleepAllowed=allowed;}
void aptSetHomeAllowed(bool allowed){homeAllowed=allowed;}
bool conn_global_init(char *error,int cap){(void)error;(void)cap;return true;}
void conn_global_exit(void){}
static bool is(const char *s){return !strcmp(scenario,s);}
Result amInit(void){
 GPUpdateView v=gp_update_view();assert(v.state==GP_UPD_INSTALLING&&v.done==0&&v.total==bytes);assert(!sleepAllowed&&!homeAllowed);
 if(is("truncated"))assert(!truncate(DATA_DIR "/update.cia.part",bytes-1));
 return is("am-fail")?-10:0;
}
void amExit(void){}
Result AM_StartCiaInstall(int media,Handle *handle){assert(media==MEDIATYPE_SD);starts++;if(is("start-fail")||(is("retry")&&starts==1))return -11;*handle=42;return 0;}
Result FSFILE_Write(Handle handle,u32 *written,u64 offset,const void *data,u32 size,u32 flags){
 (void)data;assert(handle==42&&!flags);GPUpdateView v=gp_update_view();assert(v.state==GP_UPD_INSTALLING&&v.done==offset);assert(!gp_update_start()&&!gp_update_check());writes++;job_sleep(2);
 *written=is("short-write")?size-1:size;if(is("cancel"))gp_update_request_stop();return is("write-fail")?-12:0;
}
Result AM_FinishCiaInstall(Handle handle){assert(handle==42);GPUpdateView v=gp_update_view();assert(v.state==GP_UPD_FINALIZING&&v.done==bytes);assert(!gp_update_check()&&!gp_update_start());if(is("slow-finish")){job_sleep(1100);v=gp_update_view();assert(v.state==GP_UPD_FINALIZING&&v.elapsed>=1&&!gp_update_check());}finishes++;return is("finish-fail")?-13:0;}
Result AM_CancelCIAInstall(Handle handle){assert(handle==42);cancels++;return 0;}
Result AM_GetTitleInfo(int media,u32 count,u64 *titles,AM_TitleInfo *info){
 assert(media==MEDIATYPE_SD&&count==1&&*titles==0x0004000007E90200ull);assert(gp_update_view().state==GP_UPD_FINALIZING);queries++;info->titleID=is("wrong-title")?0:*titles;info->version=is("wrong-version")?1044:1045;return is("query-fail")?-14:0;
}
int http_download_cancel(const char *url,HttpSink sink,void *ctx,long *total,char *err,int cap,const ConnCancel *cancel){
 (void)url;(void)err;(void)cap;(void)cancel;FILE *f=fopen("manifest.json","rb");assert(f);u8 buf[16384];size_t n=fread(buf,1,sizeof buf,f);fclose(f);*total=n;assert(sink(buf,(int)n,ctx));return 200;
}
int http_download_optimized_from_cancel(const char *url,long from,int connections,HttpSink sink,void *ctx,long *total,char *err,int cap,const ConnCancel *cancel){
 (void)url;(void)err;(void)cap;(void)cancel;assert(!from&&connections==1);downloads++;FILE *f=fopen("payload.cia","rb");assert(f);u8 buf[16384];size_t n;unsigned offset=0;
 while((n=fread(buf,1,sizeof buf,f))){if(is("checksum")&&!offset)buf[100]^=1;assert(sink(buf,(int)n,ctx));offset+=(unsigned)n;}
 fclose(f);bytes=offset;*total=offset;return 200;
}
static GPUpdateView wait_worker(void){
 u64 end=now_ms()+5000;GPUpdateView v;do{job_sleep(2);v=gp_update_view();}while(gp_update_busy(v.state)&&now_ms()<end);assert(!gp_update_busy(v.state));return v;
}
int main(int argc,char **argv){
 assert(argc==2);scenario=argv[1];gp_update_init(NULL);assert(gp_update_check());assert(wait_worker().state==GP_UPD_AVAILABLE);assert(gp_update_start());GPUpdateView v=wait_worker();
 if(is("retry")){assert(v.state==GP_UPD_ERROR&&starts==1&&downloads==1);while(!gp_update_start())job_sleep(1);v=wait_worker();}
 bool success=is("success")||is("retry")||is("slow-finish");assert(v.state==(success?GP_UPD_INSTALLED:GP_UPD_ERROR));assert(sleepAllowed&&homeAllowed);assert(downloads==1);
 if(success){assert(queries==1&&finishes==1&&writes==4);job_sleep(20);assert(!gp_update_check()&&!gp_update_start());assert(gp_update_view().state==GP_UPD_INSTALLED);assert(access(DATA_DIR "/update.cia.part",F_OK));}
 else{assert(!strstr(v.message,"Update installed"));assert(access(DATA_DIR "/update.cia.part",F_OK)==(is("checksum")?-1:0));}
 if(is("checksum"))assert(!starts&&!writes&&!finishes&&!queries);
 if(is("write-fail")||is("short-write")||is("cancel")||is("truncated"))assert(cancels==1&&!finishes&&!queries);
 if(is("finish-fail"))assert(finishes==1&&!queries&&!cancels);
 assert(gp_update_stop_within(1000));printf("PASS CIA worker %s: writes=%u finish=%u cancel=%u confirm=%u\n",scenario,writes,finishes,cancels,queries);return 0;
}
