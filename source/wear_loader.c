#include "wear_loader.h"
#include "asset_cache.h"
#include "package.h"
#include "jobthread.h"
#include <stdlib.h>
#include <string.h>

static struct {
 JobMutex mutex;
 JobThread thread;
#ifdef __3DS__
 LightEvent wake;
 u32 previousCpuLimit;
 bool restoreCpuLimit;
#else
 pthread_cond_t wake;
#endif
 bool started,quit,busy,pending,ready;
 unsigned generation;
 int core;
 WearLoadRequest request;
 WearLoadResult result;
} L;

void wear_loader_result_free(WearLoadResult *result){
 garment_free(&result->garment);sprite_free(result->sprite);wear_texture_free(&result->texture);memset(result,0,sizeof *result);
}

void wear_loader_prepare(WearLoadResult *result,WearLoadRequest request,bool (*cancel)(void *),void *user){
 memset(result,0,sizeof *result);result->request=request;
 if(cancel&&cancel(user))return;
 FILE *file=NULL;
 unsigned kind=request.id>>20;
 if(kind==ASSET_AVATAR||kind==ASSET_OUTFIT){snprintf(result->path,sizeof result->path,"romfs:/%s/%u.gpa",kind==ASSET_AVATAR?"avatars":"outfits",request.id&0xfffff);file=fopen(result->path,"rb");if(!file)return;}
 else if(request.id<0x100000){snprintf(result->path,sizeof result->path,"romfs:/clothing/%u.gpa",request.id);file=fopen(result->path,"rb");}
 if(file)fclose(file);
 else if(!asset_cache_request(request.id,result->path,sizeof result->path)){result->status=WEAR_LOAD_WAITING;return;}
 if(cancel&&cancel(user))return;
 GPPackage package;
 if(package_open(&package,result->path)){result->status=WEAR_LOAD_MOVIE;return;}
 unsigned char magic[4];file=fopen(result->path,"rb");
 if(!file)return;
 bool spriteFormat=fread(magic,1,4,file)==4&&!memcmp(magic,"GPA2",4);fclose(file);
 unsigned width,height;void *encoded=NULL;size_t bytes=0;
 if(spriteFormat||(request.id>>20)>=ASSET_FURNITURE){
  Sprite *s=sprite_load_cancel(result->path,cancel,user);result->sprite=s;if(!s)return;
  const SpriteSequence *pose=sprite_pose(s,request.rotation,request.variation);if(!pose)return;
  result->page=pose->page;width=s->textures[pose->page].width;height=s->textures[pose->page].height;
  if(cancel&&cancel(user))return;
  encoded=sprite_texture_read(s,pose->page,&bytes);
 }else{
  if(!garment_load_cancel(&result->garment,result->path,cancel,user))return;
  Garment *g=&result->garment;width=g->width;height=g->height;
  if(cancel&&cancel(user))return;
  file=fopen(result->path,"rb");if(!file)return;
  bytes=g->textureBytes;encoded=malloc(bytes);
  if(!encoded||fseek(file,(long)g->textureOffset,SEEK_SET)||fread(encoded,1,bytes,file)!=bytes){free(encoded);encoded=NULL;}
  fclose(file);
 }
 if(encoded&&(!cancel||!cancel(user))&&wear_texture_decode(&result->texture,encoded,bytes,width,height))result->status=WEAR_LOAD_TEXTURE;
 free(encoded);
}

static void signal_work(void){
#ifdef __3DS__
 LightEvent_Signal(&L.wake);
#else
 pthread_cond_signal(&L.wake);
#endif
}
static bool cancelled(void *user){
 unsigned generation=*(unsigned *)user;job_lock(&L.mutex);bool stop=L.quit||generation!=L.generation;job_unlock(&L.mutex);return stop;
}
static void worker(void *unused){
 (void)unused;
 for(;;){
  job_lock(&L.mutex);
  while(!L.quit&&!L.pending){
#ifdef __3DS__
   job_unlock(&L.mutex);LightEvent_Wait(&L.wake);job_lock(&L.mutex);
#else
   pthread_cond_wait(&L.wake,&L.mutex);
#endif
  }
  if(L.quit){job_unlock(&L.mutex);break;}
  WearLoadRequest request=L.request;unsigned generation=L.generation;L.pending=false;job_unlock(&L.mutex);
  WearLoadResult result;wear_loader_prepare(&result,request,cancelled,&generation);
  job_lock(&L.mutex);L.busy=false;
  if(L.quit||generation!=L.generation){job_unlock(&L.mutex);wear_loader_result_free(&result);continue;}
  L.result=result;L.ready=true;job_unlock(&L.mutex);
 }
}
#ifndef __3DS__
static void *trampoline(void *unused){worker(unused);return NULL;}
#endif

bool wear_loader_start(void){
 if(L.started)return true;
 memset(&L,0,sizeof L);job_mutex_init(&L.mutex);
 L.core=-2;
#ifdef __3DS__
 LightEvent_Init(&L.wake,RESET_ONESHOT);
#ifdef GP_WEAR_AUX_CORE
 if(R_SUCCEEDED(APT_GetAppCpuTimeLimit(&L.previousCpuLimit))&&R_SUCCEEDED(APT_SetAppCpuTimeLimit(5))){
  L.restoreCpuLimit=true;L.thread=threadCreate(worker,NULL,96*1024,0x32,1,false);
  if(L.thread)L.core=1;
  else{APT_SetAppCpuTimeLimit(L.previousCpuLimit);L.restoreCpuLimit=false;}
 }
#endif
 if(!L.thread)L.thread=threadCreate(worker,NULL,96*1024,0x32,-2,false);
 if(!L.thread){job_mutex_free(&L.mutex);return false;}
#else
 if(pthread_cond_init(&L.wake,NULL)){job_mutex_free(&L.mutex);return false;}
 if(pthread_create(&L.thread,NULL,trampoline,NULL)){pthread_cond_destroy(&L.wake);job_mutex_free(&L.mutex);return false;}
#endif
 L.started=true;return true;
}
bool wear_loader_busy(void){if(!L.started)return false;job_lock(&L.mutex);bool busy=L.busy||L.ready;job_unlock(&L.mutex);return busy;}
int wear_loader_core(void){return L.started?L.core:-3;}
bool wear_loader_request(WearLoadRequest request){
 if(!L.started)return false;
 job_lock(&L.mutex);bool accept=!L.quit&&!L.busy&&!L.ready;
 if(accept){L.request=request;L.pending=L.busy=true;signal_work();}
 job_unlock(&L.mutex);return accept;
}
bool wear_loader_take(WearLoadResult *result){
 if(!L.started)return false;
 job_lock(&L.mutex);bool ready=L.ready;
 if(ready){*result=L.result;memset(&L.result,0,sizeof L.result);L.ready=false;}
 job_unlock(&L.mutex);return ready;
}
void wear_loader_cancel(void){
 if(!L.started)return;
 WearLoadResult old={0};job_lock(&L.mutex);L.generation++;if(L.pending){L.pending=L.busy=false;}if(L.ready){old=L.result;memset(&L.result,0,sizeof L.result);L.ready=false;}job_unlock(&L.mutex);wear_loader_result_free(&old);
}
void wear_loader_request_stop(void){if(!L.started)return;job_lock(&L.mutex);L.quit=true;signal_work();job_unlock(&L.mutex);}
bool wear_loader_stop_within(unsigned ms){
 if(!L.started)return true;
 wear_loader_request_stop();
#ifdef __3DS__
 if(R_FAILED(threadJoin(L.thread,(u64)ms*1000000)))return false;
 threadFree(L.thread);
 if(L.restoreCpuLimit)APT_SetAppCpuTimeLimit(L.previousCpuLimit);
#else
 (void)ms;pthread_join(L.thread,NULL);pthread_cond_destroy(&L.wake);
#endif
 wear_loader_result_free(&L.result);job_mutex_free(&L.mutex);memset(&L,0,sizeof L);return true;
}
