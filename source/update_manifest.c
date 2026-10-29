#include "update_manifest.h"
#include "hash.h"
#include <jansson.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool version(const char *s,unsigned parts[3]){
 if(!s||!*s)return false;
 for(unsigned i=0;i<3;i++){if(*s<'0'||*s>'9')return false;char *end;unsigned long n=strtoul(s,&end,10);if(n>65535)return false;parts[i]=(unsigned)n;s=end;if(i<2){if(*s++!='.')return false;}else if(*s)return false;}
 return true;
}
bool gp_version_valid(const char *s){unsigned p[3];return version(s,p);}
int gp_version_compare(const char *a,const char *b){unsigned x[3],y[3];if(!version(a,x)||!version(b,y))return 0;for(unsigned i=0;i<3;i++)if(x[i]!=y[i])return x[i]>y[i]?1:-1;return 0;}
static bool url_valid(const char *url){
 if(!url||strchr(url,'\r')||strchr(url,'\n')||strchr(url,'#')||strchr(url,'@'))return false;
 const char *allowed[]={"https://gopenguin.lol/","https://github.com/Tweebbo/gopenguin-3ds/releases/download/"};
 for(unsigned i=0;i<2;i++)if(!strncmp(url,allowed[i],strlen(allowed[i]))&&!strstr(url,"/../"))return true;
#ifdef GP_LAN_ONLY
 if(!strncmp(url,"http://127.0.0.1:",17)||!strncmp(url,"http://10.0.2.2:",16))return true;
#endif
 return false;
}
static bool artifact(json_t *j,GPUpdateArtifact *out){
 const char *url=json_string_value(json_object_get(j,"url")),*sha=json_string_value(json_object_get(j,"sha256"));json_int_t bytes=json_integer_value(json_object_get(j,"bytes"));
 if(!url_valid(url)||strlen(url)>=sizeof out->url||!sha||strlen(sha)!=64||bytes<32||bytes>GP_UPDATE_MAX_BYTES)return false;
 for(unsigned i=0;i<64;i++)if(!strchr("0123456789abcdef",sha[i]))return false;
 strcpy(out->url,url);strcpy(out->sha,sha);out->bytes=(unsigned)bytes;return true;
}
bool gp_update_manifest(const char *data,size_t bytes,GPUpdateManifest *out){
 memset(out,0,sizeof *out);if(!data||!bytes||bytes>16384)return false;
 json_error_t error;json_t *j=json_loadb(data,bytes,JSON_REJECT_DUPLICATES,&error);if(!j)return false;
 const char *title=json_string_value(json_object_get(j,"titleId")),*v=json_string_value(json_object_get(j,"version")),*notes=json_string_value(json_object_get(j,"notes"));
 bool ok=json_integer_value(json_object_get(j,"schema"))==1&&title&&!strcmp(title,GP_RELEASE_TITLE_ID)&&gp_version_valid(v)&&strlen(v)<sizeof out->version&&artifact(json_object_get(j,"cia"),&out->cia)&&artifact(json_object_get(j,"3dsx"),&out->homebrew);
 unsigned parts[3];if(ok&&(!version(v,parts)||parts[0]>63||parts[1]>63||parts[2]>15))ok=false;
 if(ok){strcpy(out->version,v);snprintf(out->notes,sizeof out->notes,"%s",notes?notes:"");out->cia.titleVersion=(parts[0]<<10)|(parts[1]<<4)|parts[2];out->cia.checkTitleVersion=true;}json_decref(j);if(!ok)memset(out,0,sizeof *out);return ok;
}
static uint32_t le32(const unsigned char *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t aligned(uint64_t n){return (n+63)&~63ull;}
bool gp_update_file_progress(const char *path,const GPUpdateArtifact *a,bool homebrew,bool (*cancel)(void *),void (*progress)(unsigned,void *),void *user){
 FILE *f=fopen(path,"rb");if(!f)return false;bool ok=false;unsigned char header[32],digest[32];
 if(fseek(f,0,SEEK_END)||ftell(f)!=(long)a->bytes||fseek(f,0,SEEK_SET)||fread(header,1,32,f)!=32)goto done;
 for(unsigned i=0;i<32;i++){char pair[3]={a->sha[i*2],a->sha[i*2+1],0};digest[i]=(unsigned char)strtoul(pair,NULL,16);}
 if(fseek(f,0,SEEK_SET)||!hash_file_progress(f,digest,cancel,progress,user))goto done;
 if(homebrew){ok=!memcmp(header,"3DSX",4);goto done;}
 if(le32(header)<32||le32(header)>0x10000)goto done;
 uint64_t tmd=aligned(le32(header))+aligned(le32(header+8))+aligned(le32(header+12));
 if(tmd+0x140+0x54>a->bytes||fseek(f,(long)tmd,SEEK_SET)||fread(header,1,4,f)!=4)goto done;
 if(memcmp(header,"\x00\x01\x00\x04",4))goto done;
 if(fseek(f,(long)(tmd+0x140+0x4c),SEEK_SET)||fread(header,1,8,f)!=8)goto done;
 char title[17];for(unsigned i=0;i<8;i++)snprintf(title+i*2,3,"%02X",header[i]);ok=!strcmp(title,GP_RELEASE_TITLE_ID);
 if(ok&&a->checkTitleVersion){if(fseek(f,(long)(tmd+0x140+0x9c),SEEK_SET)||fread(header,1,2,f)!=2)ok=false;else ok=((unsigned)header[0]<<8|header[1])==a->titleVersion;}
done:fclose(f);return ok;
}
bool gp_update_file_cancel(const char *path,const GPUpdateArtifact *a,bool homebrew,bool (*cancel)(void *),void *user){return gp_update_file_progress(path,a,homebrew,cancel,NULL,user);}
bool gp_update_file(const char *path,const GPUpdateArtifact *a,bool homebrew){return gp_update_file_cancel(path,a,homebrew,NULL,NULL);}
