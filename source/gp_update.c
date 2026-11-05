#include "gp_update.h"
#include "jobthread.h"
#include "jobevent.h"
#include "transport/ws.h"
#include "transport/download.h"
#include "resource_store.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef __3DS__
#include <3ds.h>
#endif
#ifndef GP_UPDATE_URL
#define GP_UPDATE_URL "https://gopenguin.lol/3ds/latest-v2.json"
#endif
#ifndef GOPENGUIN_VERSION
#define GOPENGUIN_VERSION GP_RELEASE_VERSION
#endif
static struct {JobMutex mutex;JobEvent wake;JobThread thread;bool initialized,started,install,repair,background,prepared,networkTransfer,networkYielded;atomic_bool quit,finished,paused,suspended;atomic_uint rate,holdUntil;u64 phaseAt,ioAt;unsigned hashDone;char launch[256];GPUpdateView view;GPUpdateManifest manifest;} U;
const char *gp_update_current(void){static const char marker[]="GOPENGUIN_RELEASE:" GOPENGUIN_VERSION;return marker+sizeof("GOPENGUIN_RELEASE:")-1;}
static bool cancelled(void *unused){
 (void)unused;
 while(U.background&&!atomic_load(&U.quit)){
  bool asleep=atomic_load(&U.paused)||atomic_load(&U.suspended);
  bool held=(int32_t)(atomic_load(&U.holdUntil)-(unsigned)now_ms())>0;
  if(!asleep&&!held)break;
  if(U.networkTransfer)U.networkYielded=true;
  if(asleep)job_event_wait(&U.wake);else job_sleep(10);
 }
 return atomic_load(&U.quit);
}
static void cooperate(unsigned bytes,void *unused){
 (void)unused;if(!U.background||cancelled(NULL))return;
 unsigned rate=atomic_load(&U.rate);if(!rate)return;
 u64 now=now_ms();if(U.ioAt<now)U.ioAt=now;
 U.ioAt+=((u64)bytes*1000+rate-1)/rate;
 while(!cancelled(NULL)&&(now=now_ms())<U.ioAt){u64 left=U.ioAt-now;job_sleep(left>10?10:(unsigned)left);}
}
void gp_update_activity(unsigned frame_ms,bool loading,bool suspended,bool saver,bool new_model){
 if(!U.initialized)return;
 if(atomic_exchange(&U.suspended,suspended)!=suspended)job_event_signal(&U.wake);
 atomic_store(&U.rate,(saver?384u:new_model?1536u:768u)*1024u);
 unsigned budget=new_model&&!saver?17:34;
 if(loading||frame_ms>budget+6)atomic_store(&U.holdUntil,(unsigned)now_ms()+(loading?200:100));
}
void gp_update_pause(bool paused){if(U.initialized){atomic_store(&U.paused,paused);job_event_signal(&U.wake);}}
bool gp_update_blocks_game(GPUpdateView view){return !view.background&&gp_update_busy(view.state)&&view.state!=GP_UPD_CHECKING;}
bool gp_update_busy(GPUpdateState s){return s==GP_UPD_CHECKING||s==GP_UPD_DOWNLOADING||s==GP_UPD_VERIFYING||s==GP_UPD_RECONSTRUCTING||s==GP_UPD_INSTALLING||s==GP_UPD_FINALIZING||s==GP_UPD_RESOURCES;}
static void state(GPUpdateState s,const char *message){job_lock(&U.mutex);GPUpdateState previous=U.view.state;u64 elapsed=U.phaseAt?now_ms()-U.phaseAt:0;unsigned done=U.view.done;U.view.state=s;U.phaseAt=now_ms();snprintf(U.view.message,sizeof U.view.message,"%s",message);job_unlock(&U.mutex);if(U.install){FILE *log=fopen(DATA_DIR "/last-update.log","a");if(log){fprintf(log,"%llu phase=%d previous=%d elapsed_ms=%llu bytes=%u %s\n",(unsigned long long)now_ms(),s,previous,(unsigned long long)elapsed,done,message);fclose(log);}}}
static void progress(unsigned done,void *unused){(void)unused;job_lock(&U.mutex);U.view.done=done;job_unlock(&U.mutex);}
static void phase(GPUpdateState s,const char *message){state(s,message);progress(0,NULL);}
GPUpdateView gp_update_view(void){GPUpdateView view={0};if(U.initialized){job_lock(&U.mutex);view=U.view;view.elapsed=(unsigned)((now_ms()-U.phaseAt)/1000);job_unlock(&U.mutex);view.paused=view.background&&atomic_load(&U.paused);}
#ifdef __3DS__
 static bool sleepBlocked,homeBlocked;bool working=gp_update_blocks_game(view);if(working!=sleepBlocked){sleepBlocked=working;aptSetSleepAllowed(!working);}bool installing=view.state==GP_UPD_INSTALLING||view.state==GP_UPD_FINALIZING;if(installing!=homeBlocked){homeBlocked=installing;aptSetHomeAllowed(!installing);}
#endif
 return view;}
void gp_update_init(const char *launch){if(U.initialized)return;memset(&U,0,sizeof U);job_mutex_init(&U.mutex);job_event_init(&U.wake);U.initialized=true;atomic_init(&U.quit,false);atomic_init(&U.finished,true);atomic_init(&U.paused,false);atomic_init(&U.suspended,false);atomic_init(&U.rate,0);atomic_init(&U.holdUntil,(unsigned)now_ms());if(launch&&strlen(launch)>5&&!strcmp(launch+strlen(launch)-5,".3dsx")&&strlen(launch)<sizeof U.launch&&!strstr(launch,".."))snprintf(U.launch,sizeof U.launch,"%s",launch);}
typedef struct {char data[16385];size_t length;} Memory;
static bool memory_sink(const u8 *data,int bytes,void *user){Memory *m=user;if(bytes<0||(size_t)bytes>16384-m->length||cancelled(NULL))return false;memcpy(m->data+m->length,data,(size_t)bytes);m->length+=(size_t)bytes;m->data[m->length]=0;return true;}
static bool check(void){
 Memory *m=calloc(1,sizeof *m);if(!m){state(GP_UPD_ERROR,"Not enough memory to check for updates.");return false;}
 long total;char error[160]="";int status=http_download_cancel(GP_UPDATE_URL,memory_sink,m,&total,error,sizeof error,&(ConnCancel){cancelled,NULL});
 GPUpdateManifest manifest;bool ok=status==200&&gp_update_manifest_for_version(m->data,m->length,gp_update_current(),&manifest);free(m);
 if(!ok){state(status==404?GP_UPD_CURRENT:GP_UPD_ERROR,status==404?"No GoPenguin updates have been published yet.":"Could not verify the GoPenguin update list. Try again later.");return false;}
 if(!gp_version_valid(gp_update_current())){state(GP_UPD_ERROR,"This development build does not install public updates.");return false;}
 if(gp_version_compare(manifest.version,gp_update_current())<=0){state(GP_UPD_CURRENT,"You have the latest GoPenguin version.");return false;}
 job_lock(&U.mutex);
 bool same=!strcmp(U.manifest.version,manifest.version)&&!strcmp(U.manifest.cia.sha,manifest.cia.sha)&&!strcmp(U.manifest.homebrew.sha,manifest.homebrew.sha);
 if(!same){U.view.fullFallback=U.view.resourceFullFallback=false;U.prepared=false;}
 U.manifest=manifest;const GPUpdateArtifact *full=U.launch[0]?&manifest.homebrew:&manifest.cia;const IncrementalOffer *small=U.launch[0]?&manifest.smallHomebrew:&manifest.smallCia;
 U.view.small=small->bytes&&small->bytes<full->bytes;U.view.fullBytes=full->bytes;U.view.downloadBytes=U.view.small&&!U.view.fullFallback?small->bytes:full->bytes;
 U.view.resourcesOnly=false;
 if(manifest.resources.present){const GPUpdateArtifact *bundle=&manifest.resources.bundle;for(unsigned n=0;n<manifest.resources.count;n++)if(!strcmp(manifest.resources.deltas[n].from,gp_update_current()))bundle=&manifest.resources.deltas[n].bundle;U.view.downloadBytes+=manifest.resources.index.bytes+bundle->bytes;}
 snprintf(U.view.version,sizeof U.view.version,"%s",manifest.version);job_unlock(&U.mutex);
 state(GP_UPD_AVAILABLE,U.view.small?"A smaller update is ready. Existing game files will be reused. Your penguin is kept.":"A new GoPenguin version is ready. Your penguin is kept.");return true;
}
typedef struct {FILE *file;unsigned done,total;} Sink;
static bool file_sink(const u8 *data,int bytes,void *user){Sink *s=user;if(bytes<0||(unsigned)bytes>s->total-s->done||cancelled(NULL)||fwrite(data,1,(size_t)bytes,s->file)!=(size_t)bytes)return false;s->done+=(unsigned)bytes;job_lock(&U.mutex);U.view.done=s->done;job_unlock(&U.mutex);cooperate((unsigned)bytes,NULL);return !atomic_load(&U.quit);}
static void verify_progress(unsigned done,void *unused){progress(done,unused);cooperate(done-U.hashDone,NULL);U.hashDone=done;}
static bool install_cia(const char *path,const GPUpdateArtifact *artifact){
#ifdef __3DS__
 phase(GP_UPD_INSTALLING,"Installing on SD. Keep GoPenguin open and the console awake.");
 Result r=amInit();if(R_FAILED(r)){state(GP_UPD_ERROR,"Could not open the installer. The verified CIA is kept on your SD card.");return false;}
 const char *operation="open update";
 FILE *file=fopen(path,"rb");Handle handle=0;unsigned char *buffer=malloc(128*1024);if(!file||!buffer)r=-1;
 if(R_SUCCEEDED(r)){operation="start install";r=AM_StartCiaInstall(MEDIATYPE_SD,&handle);}
 u64 offset=0;
 while(R_SUCCEEDED(r)&&!cancelled(NULL)){
  size_t bytes=fread(buffer,1,128*1024,file);if(!bytes){if(ferror(file))r=-1;break;}
  operation="write update";u32 written=0;r=FSFILE_Write(handle,&written,offset,buffer,(u32)bytes,0);if(R_SUCCEEDED(r)&&written!=bytes)r=-1;offset+=written;progress((unsigned)offset,NULL);
 }
 if(R_SUCCEEDED(r)&&(cancelled(NULL)||offset!=artifact->bytes)){operation="incomplete install";r=-1;}
 if(handle){
  if(R_SUCCEEDED(r)){state(GP_UPD_FINALIZING,"Finishing installation. Keep GoPenguin open; this can take a while.");operation="finish install";r=AM_FinishCiaInstall(handle);}
  else AM_CancelCIAInstall(handle);
 }
 if(R_SUCCEEDED(r)){
  state(GP_UPD_FINALIZING,"Confirming the installed GoPenguin version...");
  u64 title=strtoull(GP_RELEASE_TITLE_ID,NULL,16);AM_TitleInfo info={0};operation="confirm version";r=AM_GetTitleInfo(MEDIATYPE_SD,1,&title,&info);
  if(R_SUCCEEDED(r)&&(info.titleID!=title||info.version!=artifact->titleVersion))r=-1;
 }
 free(buffer);if(file)fclose(file);amExit();
 if(R_FAILED(r)){char error[200];snprintf(error,sizeof error,"Could not %s (0x%08lX). Verified CIA kept at SD:/3ds/gopenguin/update.cia.part. Retry or use FBI.",operation,(unsigned long)(u32)r);state(GP_UPD_ERROR,error);}
 return R_SUCCEEDED(r);
#else
 (void)path;(void)artifact;state(GP_UPD_ERROR,"CIA installation requires a Nintendo 3DS.");return false;
#endif
}
static bool fetch_artifact(const GPUpdateArtifact *a,const char *path){
 char side[320],previous[65]="",error[160]="";snprintf(side,sizeof side,"%s.sha256",path);
 FILE *f=fopen(side,"r");if(f){fread(previous,1,64,f);fclose(f);}if(strcmp(previous,a->sha)){remove(path);f=fopen(side,"w");if(!f||fputs(a->sha,f)<0){if(f)fclose(f);state(GP_UPD_ERROR,"Could not save the update on your SD card.");return false;}if(fclose(f)){state(GP_UPD_ERROR,"Could not save the update on your SD card.");return false;}}
 struct stat info;unsigned offset=!stat(path,&info)&&info.st_size>=0&&info.st_size<=a->bytes?(unsigned)info.st_size:0;
 if(!offset)remove(path);
 Sink sink={.file=fopen(path,offset?"ab":"wb"),.done=offset,.total=a->bytes};if(!sink.file){state(GP_UPD_ERROR,"Could not save the update. Check the SD card's free space.");return false;}
 job_lock(&U.mutex);U.view.total=a->bytes;job_unlock(&U.mutex);phase(GP_UPD_DOWNLOADING,U.background?"Downloading in the background. You can keep playing.":"Downloading verified update files. Keep the console awake.");progress(offset,NULL);
 bool received=offset==a->bytes;long total=0;
 for(unsigned attempt=0;!received&&attempt<2;attempt++){
  int connections=a->bytes>=1048576?2:1;U.networkYielded=false;U.networkTransfer=true;
  int status=http_download_optimized_from_cancel(a->url,(long)sink.done,connections,file_sink,&sink,&total,error,sizeof error,&(ConnCancel){cancelled,NULL});
  U.networkTransfer=false;received=(status==200||status==206)&&sink.done==a->bytes&&total==(long)a->bytes;
  if(!received&&(!U.networkYielded||atomic_load(&U.quit)))break;
 }
 if(fclose(sink.file))received=false;
 if(!received){state(GP_UPD_ERROR,"Download interrupted. Choose Install again to resume; your current app is kept.");return false;}
 return true;
}
static bool prepare_resources(const GPResourceOffer *offer,bool allow_full){
 if(!offer->present)return true;
 const char *directory=DATA_DIR "/resources",*indexPath=DATA_DIR "/resources/target.idx.part",*bundlePath=DATA_DIR "/resources/update.gpb.part";
 struct stat info;if(stat(directory,&info)){if(mkdir(directory,0700)){state(GP_UPD_ERROR,"Could not create the resource folder. Check your SD card.");return false;}}else if(!S_ISDIR(info.st_mode)){state(GP_UPD_ERROR,"The resource folder could not be opened.");return false;}
 if(!fetch_artifact(&offer->index,indexPath))return false;
 IncrementalHooks hooks={.cancel=cancelled,.progress=progress,.cooperate=cooperate};
 if(!incremental_verify(indexPath,offer->index.sha,offer->index.bytes,&hooks)){remove(indexPath);state(GP_UPD_ERROR,"The resource list failed verification. Try again.");return false;}
 ResourceIndex *index=resource_index_load(indexPath);if(!index){state(GP_UPD_ERROR,"The resource list is not supported.");return false;}
 job_lock(&U.mutex);U.view.total=(unsigned)resource_index_bytes(index);job_unlock(&U.mutex);phase(GP_UPD_RESOURCES,U.background?"Preparing game resources in the background. You can keep playing.":"Checking and reusing installed resources on SD...");
 ResourceStats stats={0};bool ok=U.repair?resource_store_reuse(index,directory,"romfs:/",&hooks,&stats):resource_store_reuse_cached(index,directory,"romfs:/",&hooks,&stats);
 if(ok&&!resource_store_ready(index,directory)){
  const GPUpdateArtifact *bundle=&offer->bundle;bool delta=false;
  if(!allow_full)for(unsigned n=0;n<offer->count;n++)if(!strcmp(offer->deltas[n].from,gp_update_current())){bundle=&offer->deltas[n].bundle;delta=true;break;}
  ok=fetch_artifact(bundle,bundlePath);
  if(ok){job_lock(&U.mutex);U.view.total=bundle->bytes;job_unlock(&U.mutex);phase(GP_UPD_RESOURCES,"Saving and verifying the changed resource files...");ok=resource_store_bundle(index,directory,bundlePath,bundle->sha,bundle->bytes,&hooks,&stats);}
  if(ok&&!resource_store_ready(index,directory)){
   if(delta){job_lock(&U.mutex);U.view.resourceFullFallback=true;const GPUpdateArtifact *app=U.launch[0]?&U.manifest.homebrew:&U.manifest.cia;const IncrementalOffer *small=U.launch[0]?&U.manifest.smallHomebrew:&U.manifest.smallCia;U.view.downloadBytes=offer->bundle.bytes+offer->index.bytes+(U.view.small&&!U.view.fullFallback?small->bytes:app->bytes);job_unlock(&U.mutex);state(GP_UPD_ERROR,"Some resources are missing. Choose Full repair to download the complete resource set.");}
   else state(GP_UPD_ERROR,"The resource set is incomplete. Retry preparation.");
   ok=false;
  }else if(!ok&&!cancelled(NULL)&&gp_update_view().state==GP_UPD_RESOURCES)state(GP_UPD_ERROR,"Resource verification failed. Retry preparation; your current application is kept.");
 }
 if(!ok&&gp_update_view().state==GP_UPD_RESOURCES)state(GP_UPD_ERROR,cancelled(NULL)?"Resource preparation stopped. Choose Install again to resume.":"Resources could not be saved. Check your SD card's free space and try again.");
 resource_index_free(index);
 if(ok){FILE *log=fopen(DATA_DIR "/last-update.log","a");if(log){fprintf(log,"resources reused=%llu copied=%llu changed=%llu verified=%llu cached=%llu files=%u\n",(unsigned long long)stats.existing,(unsigned long long)stats.copied,(unsigned long long)stats.written,(unsigned long long)stats.verified,(unsigned long long)stats.cached,stats.files);fclose(log);}if(!U.background)remove(indexPath);remove(bundlePath);}
 return ok;
}
static bool prepared_resources_present(void){
 const GPResourceOffer *offer=&U.manifest.resources;if(!offer->present)return true;
 const char *path=DATA_DIR "/resources/target.idx.part";IncrementalHooks hooks={.cancel=cancelled};
 if(!incremental_verify(path,offer->index.sha,offer->index.bytes,&hooks))return false;
 ResourceIndex *index=resource_index_load(path);bool ok=index&&resource_store_ready(index,DATA_DIR "/resources");resource_index_free(index);return ok;
}
static void install(void){
 const bool homebrew=U.launch[0]!=0;const GPUpdateArtifact *a=homebrew?&U.manifest.homebrew:&U.manifest.cia;
 const IncrementalOffer *small=homebrew?&U.manifest.smallHomebrew:&U.manifest.smallCia;
 if(U.prepared&&!prepared_resources_present()){U.prepared=false;state(GP_UPD_ERROR,"Prepared resources are incomplete. Choose Prepare / resume to check them again.");return;}
 if(!U.prepared&&!prepare_resources(&U.manifest.resources,U.view.resourceFullFallback))return;
 char path[288],side[320],patch[288],building[320],error[200]="";
 snprintf(path,sizeof path,"%s",DATA_DIR "/update.cia.part");if(homebrew)snprintf(path,sizeof path,"%s.part",U.launch);
 snprintf(side,sizeof side,"%s.sha256",path);snprintf(patch,sizeof patch,DATA_DIR "/update-%s.tupd.part",homebrew?"3dsx":"cia");snprintf(building,sizeof building,"%s.rebuilding",path);
 char existing[65]="";FILE *saved=fopen(side,"r");if(saved){fread(existing,1,64,saved);fclose(saved);}struct stat info;
 bool complete=!strcmp(existing,a->sha)&&!stat(path,&info)&&info.st_size==(long)a->bytes;
 if(!complete&&U.view.small&&!U.view.fullFallback){
  GPUpdateArtifact download={.bytes=small->bytes};snprintf(download.url,sizeof download.url,"%s",small->url);snprintf(download.sha,sizeof download.sha,"%s",small->sha256);
  state(GP_UPD_DOWNLOADING,"Downloading the smaller update. Keep the console awake.");if(!fetch_artifact(&download,patch))return;
  job_lock(&U.mutex);U.view.total=a->bytes;job_unlock(&U.mutex);phase(GP_UPD_RECONSTRUCTING,"Preparing the update from installed files. Keep GoPenguin open.");
  IncrementalTarget target={GP_RELEASE_TITLE_ID,gp_update_current(),U.manifest.version,a->sha,a->bytes,homebrew};IncrementalHooks hooks={.cancel=cancelled,.progress=progress,.cooperate=cooperate};
  IncrementalResult result=incremental_apply(patch,building,"romfs:/",small,&target,&hooks,error,sizeof error);
  if(result!=INCREMENTAL_OK){
   if(result!=INCREMENTAL_CANCELLED&&result!=INCREMENTAL_IO_ERROR){
    job_lock(&U.mutex);U.view.fullFallback=true;U.view.downloadBytes=a->bytes;job_unlock(&U.mutex);
    snprintf(error,sizeof error,"Small update unavailable. Full download: %.1f MB. Select Full download to continue. Your penguin is kept.",a->bytes/1000000.0);
   }
   state(GP_UPD_ERROR,error);return;
  }
  if(rename(building,path)){remove(path);if(rename(building,path)){state(GP_UPD_ERROR,"Could not save the prepared update on SD. Your current app is kept.");return;}}
  FILE *sha=fopen(side,"w");bool ok=sha&&fputs(a->sha,sha)>=0;if(sha&&fclose(sha))ok=false;
  if(!ok){state(GP_UPD_ERROR,"Could not save the prepared update checksum. Check the SD card.");return;}
 }else if(!fetch_artifact(a,path))return;
 job_lock(&U.mutex);U.view.total=a->bytes;job_unlock(&U.mutex);
 phase(GP_UPD_VERIFYING,U.background?"Checking the update in the background. You can keep playing.":"Download complete. Verifying the update on SD...");
 U.hashDone=0;
 if(!gp_update_file_progress(path,a,homebrew,cancelled,verify_progress,NULL)){U.prepared=false;if(!cancelled(NULL)){remove(path);remove(side);}state(GP_UPD_ERROR,cancelled(NULL)?"Verification stopped. Choose Prepare again to retry.":"The update failed its checksum or package identity check. Your current app is kept.");return;}
 if(U.background){U.prepared=true;state(GP_UPD_READY,"Update ready. Keep playing, or open Updates and choose Install now when you are ready to stop.");return;}
 bool ok;
 if(homebrew){
  state(GP_UPD_FINALIZING,"Saving the verified app. Keep GoPenguin open...");
  char backup[288];snprintf(backup,sizeof backup,"%s.bak",U.launch);ok=rename(path,U.launch)==0;
  if(!ok){remove(backup);ok=rename(U.launch,backup)==0;if(ok&&rename(path,U.launch)){rename(backup,U.launch);ok=false;}}
  if(!ok)state(GP_UPD_ERROR,"Could not replace the app. Your previous .3dsx is kept.");
 }else ok=install_cia(path,a);
 if(ok){remove(path);remove(side);remove(patch);char patchSide[320];snprintf(patchSide,sizeof patchSide,"%s.sha256",patch);remove(patchSide);if(U.manifest.resources.present){remove(DATA_DIR "/resources/target.idx.part");remove(DATA_DIR "/resources/target.idx.part.sha256");}state(GP_UPD_INSTALLED,"Update installed. Press START to close, then reopen GoPenguin. Your penguin is kept.");}
}
static void worker(void *unused){
 (void)unused;char error[160];if(conn_global_init(error,sizeof error)){
  if(U.install){FILE *log=fopen(DATA_DIR "/last-update.log","w");if(log){fprintf(log,"running=%s target=%s\n",gp_update_current(),U.manifest.version);fclose(log);}state(GP_UPD_DOWNLOADING,"Preparing GoPenguin. Keep the console awake.");if(U.repair){if(prepare_resources(&U.manifest.resources,true))state(GP_UPD_INSTALLED,"Resources are ready. Press START to close, then reopen GoPenguin.");}else install();}else check();
  conn_global_exit();
 }else state(GP_UPD_ERROR,"Could not initialize the secure connection.");
 atomic_store(&U.finished,true);
}
#ifndef __3DS__
static void *host_worker(void *unused){worker(unused);return NULL;}
#endif
static bool start(bool install){
#ifdef GP_DEBUG_GAMES
 (void)install;
 if(U.initialized)state(GP_UPD_CURRENT,"Updates are disabled in this debug build.");
 return false;
#endif
 if(!U.initialized||atomic_load(&U.quit)||!atomic_load(&U.finished))return false;
 if(U.started){
#ifdef __3DS__
 threadJoin(U.thread,U64_MAX);threadFree(U.thread);
#else
 pthread_join(U.thread,NULL);
#endif
 U.started=false;}
 U.install=install;U.ioAt=now_ms();atomic_store(&U.paused,false);job_lock(&U.mutex);U.view.background=U.background;job_unlock(&U.mutex);atomic_store(&U.finished,false);state(install?GP_UPD_DOWNLOADING:GP_UPD_CHECKING,install?"Preparing the verified download...":"Checking for GoPenguin updates...");
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);U.thread=threadCreate(worker,NULL,48*1024,priority<0x3f?priority+1:0x3f,-2,false);bool ok=U.thread!=NULL;
#else
 bool ok=pthread_create(&U.thread,NULL,host_worker,NULL)==0;
#endif
 U.started=ok;if(!ok){atomic_store(&U.finished,true);state(GP_UPD_ERROR,"Could not start the update worker.");}return ok;
}
bool gp_update_check(void){GPUpdateState s=gp_update_view().state;if(s==GP_UPD_INSTALLED||s==GP_UPD_READY||!atomic_load(&U.finished))return false;U.background=false;return start(false);}
bool gp_update_start(void){GPUpdateView view=gp_update_view();if(!atomic_load(&U.finished)||(view.state!=GP_UPD_READY&&view.state!=GP_UPD_AVAILABLE&&!(view.state==GP_UPD_ERROR&&U.manifest.version[0])))return false;U.repair=view.resourcesOnly;U.background=false;return start(true);}
bool gp_update_prepare(void){GPUpdateView view=gp_update_view();if(!atomic_load(&U.finished)||(view.state!=GP_UPD_AVAILABLE&&!(view.state==GP_UPD_ERROR&&U.manifest.version[0])))return false;U.repair=view.resourcesOnly;U.background=!U.repair;return start(true);}
bool gp_update_use_full(bool full){
 if(!U.initialized||!atomic_load(&U.finished))return false;
 job_lock(&U.mutex);
 bool allowed=(U.view.state==GP_UPD_AVAILABLE||U.view.state==GP_UPD_ERROR)&&U.view.small&&!U.view.resourcesOnly;
 if(allowed){
  const IncrementalOffer *small=U.launch[0]?&U.manifest.smallHomebrew:&U.manifest.smallCia;
  unsigned before=U.view.fullFallback?U.view.fullBytes:small->bytes;
  unsigned after=full?U.view.fullBytes:small->bytes;
  U.view.downloadBytes=U.view.downloadBytes-before+after;U.view.fullFallback=full;
 }
 job_unlock(&U.mutex);return allowed;
}
bool gp_update_repair_resources(void){
 if(!U.initialized||!atomic_load(&U.finished)||gp_update_busy(gp_update_view().state))return false;
 FILE *f=fopen("romfs:/data/resource-offer.json","rb");if(!f)return false;char *text=malloc(16385);if(!text){fclose(f);return false;}size_t bytes=fread(text,1,16385,f);bool ok=!ferror(f)&&bytes<=16384&&gp_resource_offer(text,bytes,&U.manifest.resources);fclose(f);free(text);if(!ok)return false;
 U.repair=true;U.background=false;U.prepared=false;U.view.resourcesOnly=true;U.view.resourceFullFallback=true;U.view.downloadBytes=U.manifest.resources.index.bytes+U.manifest.resources.bundle.bytes;snprintf(U.manifest.version,sizeof U.manifest.version,"%s",gp_update_current());snprintf(U.view.version,sizeof U.view.version,"%s",gp_update_current());
 if(start(true))return true;
 U.repair=false;return false;
}
unsigned gp_update_resource_bytes(void){
 static bool checked;static unsigned bytes;if(checked)return bytes;checked=true;
 FILE *f=fopen("romfs:/data/resource-offer.json","rb");if(!f)return 0;char *text=malloc(16385);if(!text){fclose(f);checked=false;return 0;}size_t n=fread(text,1,16385,f);GPResourceOffer offer;bool ok=!ferror(f)&&n<=16384&&gp_resource_offer(text,n,&offer);fclose(f);free(text);if(ok)bytes=offer.index.bytes+offer.bundle.bytes;return bytes;
}
void gp_update_request_stop(void){if(U.initialized){atomic_store(&U.quit,true);job_event_signal(&U.wake);}}
bool gp_update_stop_within(unsigned ms){
 gp_update_request_stop();if(!U.initialized||!U.started)return true;u64 until=now_ms()+ms;while(!atomic_load(&U.finished)&&now_ms()<until)job_sleep(5);if(!atomic_load(&U.finished))return false;
#ifdef __3DS__
 threadJoin(U.thread,U64_MAX);threadFree(U.thread);
#else
 pthread_join(U.thread,NULL);
#endif
 U.started=false;return true;
}
