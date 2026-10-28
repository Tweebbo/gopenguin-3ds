#include "asset_cache.h"
#include "jobthread.h"
#include "garment.h"
#include "package.h"
#include "sprite.h"
#include "transport/ws.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
enum {SLOTS=256,WAITING=1,READY=2,FAILED=3,DISK_LIMIT=256*1024*1024};
typedef struct {unsigned id,state;int status;u64 used,retry;} Slot;
static struct {JobMutex mutex;JobThread thread;bool started,quit;char url[192],directory[192];Slot slots[SLOTS];unsigned pending;} A;
_Static_assert(sizeof ASSET_CACHE_REVISION<128,"Resource cache revision is too long");
static const char revisionName[]="resource-revision.txt";
static int cache_lstat(const char *path,struct stat *info){
#ifdef __3DS__
 /* Native SD archives expose files/directories, without POSIX symlink entries. */
 return stat(path,info);
#else
 return lstat(path,info);
#endif
}
static bool key_valid(unsigned id){return (id>>20)<=ASSET_PUFFLE&&(id&0xfffff)&&(id&0xfffff)<=1000000;}
static bool generated_name(const char *name,bool *temporary){
 if(strncmp(name,"clothing-",9))return false;
 char *end;errno=0;unsigned long value=strtoul(name+9,&end,10);if(errno||end==name+9||value>UINT_MAX||!key_valid((unsigned)value))return false;
 char canonical[64];snprintf(canonical,sizeof canonical,"clothing-%u.gpa",(unsigned)value);
 if(!strcmp(name,canonical)){*temporary=false;return true;}
 size_t length=strlen(canonical);if(!strncmp(name,canonical,length)&&!strcmp(name+length,".tmp")){*temporary=true;return true;}return false;
}
static bool cache_directory(const char *directory){
 char path[sizeof A.directory];size_t length=strlen(directory);if(!length||length>=sizeof path)return false;memcpy(path,directory,length+1);
 while(length&&path[length-1]=='/')path[--length]=0;
 if(!length)return false;
 size_t first=path[0]=='/'?1:0;char *colon=strchr(path,':');
 if(colon){if(colon==path||colon[1]!='/'||strchr(path,'/')!=colon+1||strchr(colon+1,':'))return false;first=(size_t)(colon-path)+2;}
 if(first>=length)return false;
 struct stat info;
 if(first){char saved=path[first];path[first]=0;bool root=cache_lstat(path,&info)==0&&S_ISDIR(info.st_mode);path[first]=saved;if(!root)return false;}
 size_t component=first;
 for(size_t i=first;i<=length;i++)if(!path[i]||path[i]=='/'){
  size_t count=i-component;if(!count||(count==1&&path[component]=='.')||(count==2&&path[component]=='.'&&path[component+1]=='.'))return false;
  char saved=path[i];path[i]=0;int result=cache_lstat(path,&info);
  if(result&&i==length&&errno==ENOENT){if(mkdir(path,0700)==0)result=cache_lstat(path,&info);}
  path[i]=saved;if(result||!S_ISDIR(info.st_mode))return false;component=i+1;
 }
 return true;
}
static bool discard_file(const char *path){
 struct stat info;if(cache_lstat(path,&info))return errno==ENOENT;
 if(!S_ISREG(info.st_mode)&&!S_ISLNK(info.st_mode))return false;
 return unlink(path)==0;
}
static FILE *temporary_file(const char *path){
 if(!discard_file(path))return NULL;
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return NULL;
 FILE *file=fdopen(fd,"wb");if(!file){close(fd);unlink(path);}return file;
}
static bool clear_generated(bool complete){
 DIR *directory=opendir(A.directory);if(!directory)return false;bool ok=true;struct dirent *entry;
 while((entry=readdir(directory))){bool temporary=false;if(!generated_name(entry->d_name,&temporary)||(!complete&&!temporary))continue;
  char path[256];int n=snprintf(path,sizeof path,"%s/%s",A.directory,entry->d_name);if(n<0||(size_t)n>=sizeof path||!discard_file(path)){ok=false;break;}
 }
 if(closedir(directory))ok=false;
 return ok;
}
static bool prepare_revision(void){
 char path[256],temporary[272];int n=snprintf(path,sizeof path,"%s/%s",A.directory,revisionName);if(n<0||(size_t)n>=sizeof path)return false;snprintf(temporary,sizeof temporary,"%s.tmp",path);
 bool current=false;struct stat info;
 if(cache_lstat(path,&info)==0){
  if(!S_ISREG(info.st_mode))return false;
  FILE *file=fopen(path,"rb");if(!file)return false;char value[128];size_t bytes=fread(value,1,sizeof value,file);bool ok=!ferror(file);if(fclose(file))ok=false;if(!ok)return false;
  size_t length=strlen(ASSET_CACHE_REVISION);current=bytes==length+1&&!memcmp(value,ASSET_CACHE_REVISION,length)&&value[length]=='\n';
 }else if(errno!=ENOENT)return false;
 if(current)return clear_generated(false);
 FILE *file=temporary_file(temporary);if(!file)return false;
 bool written=fprintf(file,"%s\n",ASSET_CACHE_REVISION)>0;if(fclose(file))written=false;
 if(!written||!clear_generated(true)||rename(temporary,path)){discard_file(temporary);return false;}return true;
}
static bool quitting(void){job_lock(&A.mutex);bool quit=A.quit;job_unlock(&A.mutex);return quit;}
static bool cancelled(void *unused){(void)unused;return quitting();}
static bool path_for(unsigned id,char *path,size_t cap){int n=snprintf(path,cap,"%s/clothing-%u.gpa",A.directory,id);return n>0&&(size_t)n<cap;}
typedef struct {FILE *file;size_t bytes,limit;} Download;
static bool save(const u8 *data,int bytes,void *user){Download *download=user;if(quitting()||bytes<0||(size_t)bytes>download->limit-download->bytes)return false;download->bytes+=(size_t)bytes;return fwrite(data,1,(size_t)bytes,download->file)==(size_t)bytes;}
static bool valid(const char *path,unsigned key){struct stat info;if(cache_lstat(path,&info)||!S_ISREG(info.st_mode))return false;unsigned kind=key>>20;GPPackage package;if(package_open(&package,path))return true;if(kind==ASSET_BOOK||kind==ASSET_MOVIE)return false;if(kind>=ASSET_FURNITURE){Sprite *sprite=sprite_load(path);if(!sprite)return false;sprite_free(sprite);return true;}Garment garment;if(!garment_load(&garment,path))return false;garment_free(&garment);return true;}
static bool evict(size_t incoming,const char *keep){
 for(unsigned pass=0;pass<8192&&!quitting();pass++){
  DIR *directory=opendir(A.directory);if(!directory)return false;struct dirent *entry;size_t total=0;time_t oldest=0;char victim[256]="";
  while((entry=readdir(directory))){bool temporary;unsigned id;if(!generated_name(entry->d_name,&temporary)||temporary||sscanf(entry->d_name,"clothing-%u.gpa",&id)!=1)continue;char path[256];if(!path_for(id,path,sizeof path))continue;struct stat info;if(cache_lstat(path,&info)||!S_ISREG(info.st_mode))continue;total+=(size_t)info.st_size;if(strcmp(path,keep)&&(!*victim||info.st_mtime<oldest)){oldest=info.st_mtime;snprintf(victim,sizeof victim,"%s",path);}}
  closedir(directory);if(total+incoming<=DISK_LIMIT)return true;if(!*victim||remove(victim))return false;
  unsigned removed=0;const char *name=strrchr(victim,'/');if(name&&sscanf(name+1,"clothing-%u.gpa",&removed)==1){job_lock(&A.mutex);for(unsigned i=0;i<SLOTS;i++)if(A.slots[i].id==removed&&A.slots[i].state==READY){A.slots[i].state=FAILED;A.slots[i].retry=0;}job_unlock(&A.mutex);}
 }
 return false;
}
static void worker(void *unused){
 (void)unused;char error[160];bool sockets=false;evict(0,"");
 while(!quitting()){
  unsigned index=SLOTS,id=0;job_lock(&A.mutex);u64 oldest=0;for(unsigned i=0;i<SLOTS;i++)if(A.slots[i].state==WAITING&&(index==SLOTS||A.slots[i].used<oldest)){index=i;oldest=A.slots[i].used;id=A.slots[i].id;}job_unlock(&A.mutex);
  if(index==SLOTS){job_sleep(20);continue;}
  char path[256],temporary[272],url[256];int status=0;bool ok=path_for(id,path,sizeof path)&&valid(path,id);
  if(ok){utime(path,NULL);}else if(!quitting()&&(sockets||(sockets=conn_global_init(error,sizeof error)))){
   const char *kind[]={"clothing","paper","icons","photos","books","movies","furniture","furniture_icons","flooring","puffles"};unsigned type=id>>20;bool book=type==ASSET_BOOK||type==ASSET_MOVIE;int n=snprintf(url,sizeof url,"%s/%s/%u.%s",A.url,kind[type],id&0xfffff,book?"gpk":"gpa");snprintf(temporary,sizeof temporary,"%s.tmp",path);
   if(n>0&&n<(int)sizeof url){FILE *file=temporary_file(temporary);if(file){Download download={.file=file,.limit=GP_PACKAGE_MAX};long total;status=http_download_cancel(url,save,&download,&total,error,sizeof error,&(ConnCancel){cancelled,NULL});bool written=fclose(file)==0;ok=status==200&&written&&!quitting()&&valid(temporary,id)&&evict(download.bytes,path)&&rename(temporary,path)==0;if(!ok)discard_file(temporary);}}
  }
  job_lock(&A.mutex);A.slots[index].state=ok?READY:FAILED;A.slots[index].status=ok?200:status;A.slots[index].retry=now_ms()+5000;if(A.pending)A.pending--;job_unlock(&A.mutex);
 }
 if(sockets)conn_global_exit();
}
#ifndef __3DS__
static void *trampoline(void *p){worker(p);return NULL;}
#endif
bool asset_cache_start(const char *url,const char *directory){
 Endpoint endpoint;if(!url||!directory||strlen(url)>=sizeof A.url||strlen(directory)>=sizeof A.directory||!endpoint_parse(url,&endpoint))return false;
#ifdef GP_LAN_ONLY
 if(!endpoint_is_lan(&endpoint))return false;
#endif
 asset_cache_stop();memset(&A,0,sizeof A);job_mutex_init(&A.mutex);snprintf(A.url,sizeof A.url,"%s",url);size_t n=strlen(A.url);while(n&&A.url[n-1]=='/')A.url[--n]=0;snprintf(A.directory,sizeof A.directory,"%s",directory);
 if(!cache_directory(A.directory)||!prepare_revision()){job_mutex_free(&A.mutex);return false;}A.started=true;
#ifdef __3DS__
 A.thread=threadCreate(worker,NULL,96*1024,0x32,-2,false);if(!A.thread){A.started=false;job_mutex_free(&A.mutex);return false;}
#else
 if(pthread_create(&A.thread,NULL,trampoline,NULL)){A.started=false;job_mutex_free(&A.mutex);return false;}
#endif
 return true;
}
void asset_cache_request_stop(void){if(!A.started)return;job_lock(&A.mutex);A.quit=true;job_unlock(&A.mutex);}
bool asset_cache_stop_within(unsigned ms){
 if(!A.started)return true;
 asset_cache_request_stop();
#ifdef __3DS__
 if(R_FAILED(threadJoin(A.thread,(u64)ms*1000000ULL)))return false;
#else
 (void)ms;
#endif
 asset_cache_stop();return true;
}
void asset_cache_stop(void){if(!A.started)return;asset_cache_request_stop();
#ifdef __3DS__
 threadJoin(A.thread,U64_MAX);threadFree(A.thread);
#else
 pthread_join(A.thread,NULL);
#endif
 job_mutex_free(&A.mutex);A.started=false;}
bool asset_cache_request(unsigned id,char *path,size_t cap){
 if(!A.started||!key_valid(id))return false;
 job_lock(&A.mutex);Slot *slot=NULL,*oldest=NULL;for(unsigned i=0;i<SLOTS;i++){Slot *s=&A.slots[i];if(s->id==id){slot=s;break;}if(s->state!=WAITING&&(!oldest||s->used<oldest->used))oldest=s;}
 u64 now=now_ms();if(!slot&&A.pending<32&&oldest){slot=oldest;*slot=(Slot){.id=id,.state=WAITING};A.pending++;}
 bool ready=slot&&slot->state==READY;if(slot){slot->used=now;if(slot->state==FAILED&&now>=slot->retry&&A.pending<32){slot->state=WAITING;A.pending++;}}
 job_unlock(&A.mutex);return ready&&path_for(id,path,cap);
}
unsigned asset_cache_pending(void){if(!A.started)return 0;job_lock(&A.mutex);unsigned count=A.pending;job_unlock(&A.mutex);return count;}
unsigned asset_cache_status(unsigned key){if(!A.started)return ASSET_DISABLED;unsigned result=ASSET_QUEUED;job_lock(&A.mutex);for(unsigned i=0;i<SLOTS;i++)if(A.slots[i].id==key){Slot *s=&A.slots[i];result=s->state==READY?ASSET_READY:s->state==WAITING||s->status==202?ASSET_QUEUED:s->status==404?ASSET_UNAVAILABLE:ASSET_RETRY;break;}job_unlock(&A.mutex);return result;}
