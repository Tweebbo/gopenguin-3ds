#include "resource_store.h"
#include "hash.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __3DS__
#include <3ds.h>
#include <mbedtls/sha256.h>
typedef mbedtls_sha256_context Digest;
static void begin(Digest *d){mbedtls_sha256_init(d);mbedtls_sha256_starts_ret(d,0);}
static bool add(Digest *d,const void *p,size_t n){return !mbedtls_sha256_update_ret(d,p,n);}
static bool end(Digest *d,unsigned char out[32]){return !mbedtls_sha256_finish_ret(d,out);}
static void release(Digest *d){mbedtls_sha256_free(d);}
#else
#include <openssl/evp.h>
typedef EVP_MD_CTX *Digest;
static void begin(Digest *d){*d=EVP_MD_CTX_new();if(*d)EVP_DigestInit_ex(*d,EVP_sha256(),NULL);}
static bool add(Digest *d,const void *p,size_t n){return *d&&EVP_DigestUpdate(*d,p,n)==1;}
static bool end(Digest *d,unsigned char out[32]){return *d&&EVP_DigestFinal_ex(*d,out,NULL)==1;}
static void release(Digest *d){EVP_MD_CTX_free(*d);}
#endif
enum {PATH_BYTES=128,RECORD_BYTES=164,MAX_FILES=4096,IO_BYTES=131072};
struct ResourceIndex {unsigned count,total;unsigned char *entries;};
static struct {ResourceIndex *index;char directory[256];bool complete;} R;
static unsigned le32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static bool stopped(const IncrementalHooks *h){return h&&h->cancel&&h->cancel(h->user);}
static void progress(const IncrementalHooks *h,unsigned n){if(h&&h->progress)h->progress(n,h->user);}
typedef struct {const IncrementalHooks *hooks;unsigned last;} HashPace;
static bool hash_stopped(void *user){HashPace *p=user;return stopped(p->hooks);}
static void hash_pace(unsigned done,void *user){HashPace *p=user;if(p->hooks&&p->hooks->cooperate)p->hooks->cooperate(done-p->last,p->hooks->user);p->last=done;}
static const unsigned char *row(const ResourceIndex *i,unsigned n){return i->entries+(size_t)n*RECORD_BYTES;}
static bool safe(const char *p){
 if(!p||!*p||p[0]=='/'||strlen(p)>=PATH_BYTES)return false;
 const char *start=p;
 for(const char *q=p;;q++){
  if(*q&&!( (*q>='a'&&*q<='z')||(*q>='A'&&*q<='Z')||(*q>='0'&&*q<='9')||*q=='/'||*q=='.'||*q=='-'||*q=='_'))return false;
  if(!*q||*q=='/'){size_t n=(size_t)(q-start);if(!n||(n==1&&*start=='.')||(n==2&&!memcmp(start,"..",2)))return false;start=q+1;}
  if(!*q)break;
 }return true;
}
ResourceIndex *resource_index_load(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 unsigned char h[16];ResourceIndex *i=calloc(1,sizeof *i);
 bool ok=i&&fread(h,1,sizeof h,f)==sizeof h&&!memcmp(h,"GPRIDX1",8);
 if(ok){i->count=le32(h+8);i->total=le32(h+12);ok=i->count&&i->count<=MAX_FILES&&i->total<=INCREMENTAL_MAX_BYTES;}
 if(ok){i->entries=malloc((size_t)i->count*RECORD_BYTES);ok=i->entries&&fread(i->entries,RECORD_BYTES,i->count,f)==i->count&&fgetc(f)==EOF&&!ferror(f);}
 uint64_t total=0;
 for(unsigned n=0;ok&&n<i->count;n++){
  const unsigned char *r=row(i,n);ok=memchr(r,0,PATH_BYTES)&&safe((const char *)r)&&(!n||strcmp((const char *)row(i,n-1),(const char *)r)<0);
  total+=le32(r+160);if(total>INCREMENTAL_MAX_BYTES)ok=false;
 }
 ok=ok&&total==i->total;if(fclose(f))ok=false;
 if(!ok){resource_index_free(i);return NULL;}return i;
}
void resource_index_free(ResourceIndex *i){if(i){free(i->entries);free(i);}}
unsigned resource_index_count(const ResourceIndex *i){return i?i->count:0;}
uint64_t resource_index_bytes(const ResourceIndex *i){return i?i->total:0;}
static void hex(const unsigned char *sha,char out[65]){static const char digits[]="0123456789abcdef";for(unsigned n=0;n<32;n++){out[2*n]=digits[sha[n]>>4];out[2*n+1]=digits[sha[n]&15];}out[64]=0;}
static bool object_path(const char *directory,const unsigned char *sha,char *out,size_t capacity){char digest[65];hex(sha,digest);int n=snprintf(out,capacity,"%s/%c/%s.bin",directory,digest[0],digest);return n>0&&(size_t)n<capacity;}
static bool regular(const char *path,unsigned bytes){struct stat s;
#ifdef __3DS__
 return !stat(path,&s)&&S_ISREG(s.st_mode)&&s.st_size==(long)bytes;
#else
 return !lstat(path,&s)&&S_ISREG(s.st_mode)&&s.st_size==(long)bytes;
#endif
}
static bool folder(const char *path){struct stat s;
#ifdef __3DS__
 int result=stat(path,&s);
#else
 int result=lstat(path,&s);
#endif
 if(!result)return S_ISDIR(s.st_mode);
 return errno==ENOENT&&!mkdir(path,0700);
}
static bool commit_object(const char *temporary,const char *target){
 if(!rename(temporary,target))return true;
#ifdef __3DS__
 struct stat info;if(!stat(target,&info)&&S_ISREG(info.st_mode)&&!remove(target))return !rename(temporary,target);
#endif
 return false;
}
static bool object_folder(const char *target){char parent[384];if(strlen(target)>=sizeof parent)return false;strcpy(parent,target);char *slash=strrchr(parent,'/');if(!slash)return false;*slash=0;return folder(parent);}
enum {RECEIPT_BYTES=72};
static void put64(unsigned char *p,uint64_t n){for(unsigned k=0;k<8;k++)p[k]=(unsigned char)(n>>(k*8));}
static bool receipt_value(const char *path,const unsigned char *r,unsigned char value[RECEIPT_BYTES]){
 struct stat s;
#ifdef __3DS__
 if(stat(path,&s))return false;
#else
 if(lstat(path,&s))return false;
#endif
 if(!S_ISREG(s.st_mode)||s.st_size!=(long)le32(r+160))return false;
 memset(value,0,RECEIPT_BYTES);memcpy(value,"GPRVFY1",8);memcpy(value+8,r+128,36);
 put64(value+44,(uint64_t)s.st_mtime);put64(value+52,(uint64_t)s.st_ctime);
#ifndef __3DS__
 put64(value+60,(uint64_t)s.st_mtim.tv_nsec);value[68]=(unsigned char)s.st_ctim.tv_nsec;value[69]=(unsigned char)(s.st_ctim.tv_nsec>>8);value[70]=(unsigned char)(s.st_ctim.tv_nsec>>16);value[71]=(unsigned char)(s.st_ctim.tv_nsec>>24);
#endif
 return true;
}
static bool cached_object(const char *path,const unsigned char *r){
 unsigned char actual[RECEIPT_BYTES],saved[RECEIPT_BYTES];char receipt[416];
 if(!receipt_value(path,r,actual)||snprintf(receipt,sizeof receipt,"%s.verified",path)>=(int)sizeof receipt)return false;
 FILE *f=fopen(receipt,"rb");if(!f)return false;
 bool ok=fread(saved,1,sizeof saved,f)==sizeof saved&&fgetc(f)==EOF&&!ferror(f)&&!memcmp(saved,actual,sizeof saved);
 if(fclose(f))ok=false;
 return ok;
}
static void remember_object(const char *path,const unsigned char *r){
 unsigned char value[RECEIPT_BYTES];char receipt[416],temporary[432];
 if(!receipt_value(path,r,value)||snprintf(receipt,sizeof receipt,"%s.verified",path)>=(int)sizeof receipt||snprintf(temporary,sizeof temporary,"%s.part",receipt)>=(int)sizeof temporary)return;
 FILE *f=fopen(temporary,"wb");if(!f)return;
 bool ok=fwrite(value,1,sizeof value,f)==sizeof value;if(fclose(f))ok=false;
 if(ok&&rename(temporary,receipt)){
#ifdef __3DS__
  remove(receipt);ok=!rename(temporary,receipt);
#else
  ok=false;
#endif
 }
 if(!ok)remove(temporary);
}
static bool verified(const char *path,const unsigned char *r,const IncrementalHooks *h){
 if(!regular(path,le32(r+160)))return false;
 FILE *f=fopen(path,"rb");if(!f)return false;HashPace pace={.hooks=h};bool ok=hash_file_progress(f,r+128,hash_stopped,hash_pace,&pace);if(fclose(f))ok=false;return ok;
}
bool resource_store_ready(const ResourceIndex *i,const char *directory){
 if(!i)return false;
#ifdef __3DS__
 /* Directory entries include sizes, avoiding a separate SD open/stat for every object. */
 if(strncmp(directory,"sdmc:/",6))return false;
 FS_Archive archive;if(R_FAILED(FSUSER_OpenArchive(&archive,ARCHIVE_SDMC,fsMakePath(PATH_EMPTY,""))))return false;
 FS_DirectoryEntry *entries=calloc(64,sizeof *entries);bool *seen=calloc(i->count,sizeof *seen);bool ok=entries&&seen;
 static const char digits[]="0123456789abcdef";
 for(unsigned shard=0;ok&&shard<16;shard++){
  bool needed=false;for(unsigned n=0;n<i->count;n++)if((row(i,n)[128]>>4)==shard){needed=true;break;}if(!needed)continue;
  char path[384];int length=snprintf(path,sizeof path,"%s/%c",directory+5,digits[shard]);Handle handle;
  if(length<0||(size_t)length>=sizeof path||R_FAILED(FSUSER_OpenDirectory(&handle,archive,fsMakePath(PATH_ASCII,path)))){ok=false;break;}
  u32 count=0;
  do{
   if(R_FAILED(FSDIR_Read(handle,&count,64,entries))){ok=false;break;}
   for(unsigned item=0;item<count;item++){
    FS_DirectoryEntry *entry=&entries[item];if(entry->attributes&FS_ATTRIBUTE_DIRECTORY)continue;
    char name[69];unsigned k=0;for(;k<68&&entry->name[k]&&entry->name[k]<128;k++)name[k]=(char)entry->name[k];name[k]=0;
    if(k!=68||entry->name[k]||strcmp(name+64,".bin"))continue;
    for(unsigned n=0;n<i->count;n++){
     const unsigned char *r=row(i,n);if((r[128]>>4)!=shard||entry->fileSize!=le32(r+160))continue;
     char digest[65];hex(r+128,digest);if(!memcmp(name,digest,64))seen[n]=true;
    }
   }
  }while(count);
  if(R_FAILED(FSDIR_Close(handle)))ok=false;
 }
 for(unsigned n=0;ok&&n<i->count;n++)if(!seen[n])ok=false;
 free(entries);free(seen);if(R_FAILED(FSUSER_CloseArchive(archive)))ok=false;return ok;
#else
 for(unsigned n=0;n<i->count;n++){const unsigned char *r=row(i,n);char path[384];if(!object_path(directory,r+128,path,sizeof path)||!regular(path,le32(r+160)))return false;}return true;
#endif
}
static bool reuse(const ResourceIndex *i,const char *directory,const char *embedded,const IncrementalHooks *h,ResourceStats *stats,bool cached){
 if(!i||!folder(directory))return false;
 unsigned char *buffer=malloc(IO_BYTES);if(!buffer)return false;
 bool ok=true;unsigned done=0;
 for(unsigned n=0;ok&&n<i->count;n++){
  const unsigned char *r=row(i,n);unsigned bytes=le32(r+160);char target[384],source[512],temporary[400];
  if(stopped(h)||!object_path(directory,r+128,target,sizeof target)){ok=false;break;}
  bool known=cached&&cached_object(target,r);
  if(known||verified(target,r,h)){if(stats){stats->existing+=bytes;if(known)stats->cached+=bytes;else stats->verified+=bytes;stats->files++;}if(!known)remember_object(target,r);done+=bytes;progress(h,done);continue;}
  if(stopped(h)){ok=false;break;}
  if(regular(target,bytes)){
   char receipt[416];snprintf(receipt,sizeof receipt,"%s.verified",target);remove(receipt);
   if(remove(target)){ok=false;break;}
  }
  if(!object_folder(target)){ok=false;break;}
  int used=snprintf(source,sizeof source,"%s%s%s",embedded,embedded[strlen(embedded)-1]=='/'?"":"/",r);
  if(used<0||(size_t)used>=sizeof source){ok=false;break;}
  FILE *input=resource_store_open(source,"rb");if(!input)continue;
  if(fseek(input,0,SEEK_END)||ftell(input)!=(long)bytes||fseek(input,0,SEEK_SET)){fclose(input);continue;}
  snprintf(temporary,sizeof temporary,"%s.part",target);remove(temporary);FILE *output=fopen(temporary,"wb");
  if(!output){fclose(input);ok=false;break;}
  Digest digest;begin(&digest);bool copied=true,ioError=false;unsigned left=bytes;
  while(left&&!stopped(h)){unsigned take=left<IO_BYTES?left:IO_BYTES;if(fread(buffer,1,take,input)!=take||fwrite(buffer,1,take,output)!=take||!add(&digest,buffer,take)){copied=false;ioError=true;break;}left-=take;progress(h,done+bytes-left);if(h&&h->cooperate)h->cooperate(take,h->user);}
  unsigned char actual[32];copied=copied&&!left&&end(&digest,actual)&&!memcmp(actual,r+128,32);release(&digest);
  if(fclose(input))copied=false,ioError=true;
  if(fclose(output))copied=false,ioError=true;
  if(copied&&!commit_object(temporary,target))copied=false,ioError=true;
  if(!copied)remove(temporary);else{remember_object(target,r);if(stats){stats->copied+=bytes;stats->verified+=bytes;stats->files++;}done+=bytes;}
  if(stopped(h)||ioError)ok=false;
 }
 free(buffer);return ok;
}
bool resource_store_reuse(const ResourceIndex *i,const char *directory,const char *embedded,const IncrementalHooks *h,ResourceStats *stats){return reuse(i,directory,embedded,h,stats,false);}
bool resource_store_reuse_cached(const ResourceIndex *i,const char *directory,const char *embedded,const IncrementalHooks *h,ResourceStats *stats){return reuse(i,directory,embedded,h,stats,true);}
static const unsigned char *find_object(const ResourceIndex *i,const unsigned char *sha,unsigned bytes){for(unsigned n=0;n<i->count;n++){const unsigned char *r=row(i,n);if(le32(r+160)==bytes&&!memcmp(r+128,sha,32))return r;}return NULL;}
bool resource_store_bundle(const ResourceIndex *i,const char *directory,const char *bundle,const char *sha,unsigned bytes,const IncrementalHooks *h,ResourceStats *stats){
 if(!i||!sha||strlen(sha)!=64||!folder(directory)||!regular(bundle,bytes))return false;
 FILE *f=fopen(bundle,"rb");if(!f)return false;
 unsigned char *buffer=malloc(IO_BYTES),header[12],actual[32]={0};Digest whole;begin(&whole);
 bool ok=buffer&&fread(header,1,sizeof header,f)==sizeof header&&!memcmp(header,"GPRBND1",8)&&add(&whole,header,sizeof header);
 unsigned count=ok?le32(header+8):0,done=sizeof header;ok=ok&&count<=MAX_FILES;
 unsigned char *seen=calloc(count?count:1,32);if(!seen)ok=false;
 for(unsigned n=0;ok&&n<count;n++){
  unsigned char object[36];ok=!stopped(h)&&fread(object,1,sizeof object,f)==sizeof object&&add(&whole,object,sizeof object);if(!ok)break;
  unsigned size=le32(object+32);const unsigned char *expected=find_object(i,object,size);ok=expected&&done<=bytes&&bytes-done>=36&&size<=bytes-done-36;
  for(unsigned j=0;ok&&j<n;j++)if(!memcmp(seen+j*32,object,32))ok=false;
  if(!ok)break;
  memcpy(seen+n*32,object,32);done+=36;
  char target[384],temporary[400];ok=object_path(directory,object,target,sizeof target)&&object_folder(target);if(!ok)break;
  snprintf(temporary,sizeof temporary,"%s.part",target);remove(temporary);FILE *output=fopen(temporary,"wb");if(!output){ok=false;break;}
  Digest digest;begin(&digest);unsigned left=size;
  while(ok&&left){unsigned take=left<IO_BYTES?left:IO_BYTES;ok=!stopped(h)&&fread(buffer,1,take,f)==take&&add(&whole,buffer,take)&&add(&digest,buffer,take)&&fwrite(buffer,1,take,output)==take;if(ok){left-=take;done+=take;progress(h,done);if(h&&h->cooperate)h->cooperate(take,h->user);}}
  ok=ok&&end(&digest,actual)&&!memcmp(actual,object,32);release(&digest);if(fclose(output))ok=false;
  if(ok&&!commit_object(temporary,target))ok=false;
  if(!ok)remove(temporary);else{remember_object(target,expected);if(stats){stats->written+=size;stats->verified+=size;stats->files++;}}
 }
 char checksum[65];ok=ok&&done==bytes&&fgetc(f)==EOF&&!ferror(f)&&end(&whole,actual);hex(actual,checksum);ok=ok&&!strcmp(checksum,sha);release(&whole);if(fclose(f))ok=false;free(seen);free(buffer);return ok;
}
bool resource_store_init(const char *index,const char *directory){resource_store_exit();if(!directory||strlen(directory)>=sizeof R.directory)return false;R.index=resource_index_load(index);if(!R.index)return false;strcpy(R.directory,directory);R.complete=resource_store_ready(R.index,directory);return true;}
void resource_store_exit(void){resource_index_free(R.index);R.index=NULL;}
bool resource_store_enabled(void){return R.index!=NULL;}
bool resource_store_complete(void){return R.index&&R.complete;}
bool resource_store_path(const char *path,char *out,size_t capacity){
 if(!R.index||strncmp(path,"romfs:/",7))return false;
 const char *name=path+7;unsigned lo=0,hi=R.index->count;
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;const unsigned char *r=row(R.index,mid);int compare=strcmp(name,(const char *)r);if(!compare)return object_path(R.directory,r+128,out,capacity);if(compare<0)hi=mid;else lo=mid+1;}return false;
}
FILE *resource_store_open(const char *path,const char *mode){char resolved[384];if(!strcmp(mode,"rb")&&resource_store_path(path,resolved,sizeof resolved))return fopen(resolved,mode);return fopen(path,mode);}
