#include "settings.h"
#include "transport/conn.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
static bool field(json_t *root,const char *key,char *out,size_t cap){const char *s=json_string_value(json_object_get(root,key));if(!s||strlen(s)>=cap)return false;strcpy(out,s);return true;}
static bool load_file(Settings *s,const char *path){
 FILE *f=fopen(path,"rb");if(!f)return false;
 char buffer[2049];size_t n=fread(buffer,1,sizeof buffer,f);bool readable=!ferror(f);if(fclose(f))readable=false;if(!readable||n>2048)return false;
 json_error_t error;json_t *root=json_loadb(buffer,n,JSON_REJECT_DUPLICATES,&error);if(!root)return false;
 Settings parsed=*s;Endpoint endpoint;
 bool ok=field(root,"api",parsed.network.api,sizeof parsed.network.api)&&field(root,"world",parsed.network.world,sizeof parsed.network.world)&&endpoint_parse(parsed.network.api,&endpoint)&&endpoint_parse(parsed.network.world,&endpoint);
 json_t *session=json_object_get(root,"session");if(session&&(!json_is_string(session)||json_string_length(session)>=sizeof parsed.session))ok=false;
 if(ok){field(root,"session",parsed.session,sizeof parsed.session);field(root,"name",parsed.name,sizeof parsed.name);field(root,"assets",parsed.assets,sizeof parsed.assets);field(root,"donate",parsed.donate,sizeof parsed.donate);parsed.audio=!json_is_false(json_object_get(root,"audio"));
#ifndef GP_LAN_ONLY
  if(!parsed.assets[0]&&strstr(parsed.network.api,"gopenguin.lol"))snprintf(parsed.assets,sizeof parsed.assets,"https://gopenguin.lol/native");
#endif
  parsed.batterySaver=json_is_true(json_object_get(root,"batterySaver"));
  parsed.hideOtherPenguins=json_is_true(json_object_get(root,"hideOtherPenguins"));
  *s=parsed;}
 json_decref(root);memset(buffer,0,sizeof buffer);return ok;
}
bool settings_load(Settings *s,const char *path){
 memset(s,0,sizeof *s);s->audio=true;s->network.room=100;s->network.x=380;s->network.y=340;
#ifndef GP_LAN_ONLY
 snprintf(s->network.api,sizeof s->network.api,"https://gopenguin.lol");snprintf(s->network.world,sizeof s->network.world,"wss://gopenguin.lol/world");
 snprintf(s->assets,sizeof s->assets,"https://gopenguin.lol/native");
#endif
 Settings initial=*s,primary=initial;bool loaded=load_file(&primary,path),present=false;
 struct stat info;if(!stat(path,&info)||errno!=ENOENT)present=true;
 if(loaded&&primary.session[0]){*s=primary;return true;}
 const char *suffixes[]={".rollback",".bak",".tmp",".repair"};Settings fallback=initial;bool haveFallback=false;
 for(unsigned i=0;i<sizeof suffixes/sizeof *suffixes;i++){
  char candidate[256];int n=snprintf(candidate,sizeof candidate,"%s%s",path,suffixes[i]);if(n<=0||n>=(int)sizeof candidate)continue;
  if(!stat(candidate,&info)||errno!=ENOENT)present=true;
  Settings recovered=initial;if(!load_file(&recovered,candidate))continue;
  if(loaded&&(strcmp(primary.network.api,recovered.network.api)||strcmp(primary.network.world,recovered.network.world)))continue;
  if(recovered.session[0]){*s=recovered;s->recoveredFromBackup=true;return true;}
  if(!haveFallback){fallback=recovered;haveFallback=true;}
 }
 if(loaded){*s=primary;s->restoreRequired=primary.name[0]!=0;return !s->restoreRequired;}
 if(haveFallback){*s=fallback;s->restoreRequired=present;return !s->restoreRequired;}
 s->restoreRequired=present;return false;
}
static bool finish_file(FILE *file){
 bool ok=!ferror(file)&&fflush(file)==0;if(ok&&fsync(fileno(file)))ok=false;if(fclose(file))ok=false;return ok;
}
static bool publish_file(const char *temporary,const char *path,bool preserveBackup){
 if(!rename(temporary,path))return true;
 char rollback[256],backup[256];struct stat original;
 int length=snprintf(rollback,sizeof rollback,"%s.rollback",path),other=snprintf(backup,sizeof backup,"%s.bak",path);
 if(length<=0||length>=(int)sizeof rollback||other<=0||other>=(int)sizeof backup||stat(path,&original)||!S_ISREG(original.st_mode))return false;
 // Keep the last backup until the new file has committed on FAT.
 if(remove(rollback)&&errno!=ENOENT)return false;
 if(rename(path,rollback))return false;
 if(rename(temporary,path)){rename(rollback,path);return false;}
 if(preserveBackup)remove(rollback);
 else if(!remove(backup)||errno==ENOENT)rename(rollback,backup);
 return true;
}
bool settings_save(const Settings *s,const char *path){
 if(s->restoreRequired)return false;
 json_t *root=json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:b,s:b,s:b}","api",s->network.api,"world",s->network.world,"session",s->session,"name",s->name,"assets",s->assets,"donate",s->donate,"audio",s->audio,"batterySaver",s->batterySaver,"hideOtherPenguins",s->hideOtherPenguins);if(!root)return false;
 char temp[256];int n=snprintf(temp,sizeof temp,"%s%s",path,s->recoveredFromBackup?".repair":".tmp");bool ok=n>0&&n<(int)sizeof temp;
 if(ok){
  FILE *f=fopen(temp,"wb");if(f){ok=json_dumpf(root,f,JSON_COMPACT)==0;bool durable=finish_file(f);ok=ok&&durable;}else ok=false;
  if(ok){
   chmod(temp,0600);
   ok=publish_file(temp,path,s->recoveredFromBackup);
  }
  if(!ok)remove(temp);
 }
 json_decref(root);return ok;
}
bool settings_backup_session(const Settings *s,const char *path){
 if(!s->session[0])return true;
 char expected[sizeof s->session+16],saved[sizeof expected];
 int length=snprintf(expected,sizeof expected,"TWEEBBO-%s\n",s->session);
 if(length<=0||length>=(int)sizeof expected)return false;
 FILE *file=fopen(path,"rb");
 if(file){size_t count=fread(saved,1,sizeof saved,file);bool closed=fclose(file)==0;if(closed&&count==(size_t)length&&!memcmp(saved,expected,count))return true;}
 char temporary[256];int n=snprintf(temporary,sizeof temporary,"%s.tmp",path);
 if(n<=0||n>=(int)sizeof temporary)return false;
 file=fopen(temporary,"wb");if(!file)return false;
 bool ok=fwrite(expected,1,(size_t)length,file)==(size_t)length;
 bool durable=finish_file(file);ok=ok&&durable;
 if(ok){chmod(temporary,0600);ok=publish_file(temporary,path,false);}
 if(!ok)remove(temporary);
 return ok;
}
bool settings_select_account(Settings *s,const char *session,const char *name,const char *path){
 if(!session||!session[0]||strlen(session)>=sizeof s->session||(name&&strlen(name)>=sizeof s->name))return false;
 if(!strcmp(s->session,session)&&(!name||!name[0]||!strcmp(s->name,name))){if(!s->recoveredFromBackup)return true;if(!settings_save(s,path))return false;s->recoveredFromBackup=false;return true;}
 Settings next=*s;next.restoreRequired=false;next.recoveredFromBackup=false;snprintf(next.session,sizeof next.session,"%s",session);
 if(name&&name[0])snprintf(next.name,sizeof next.name,"%s",name);
 if(!settings_save(&next,path))return false;
 *s=next;return true;
}
bool settings_remember_name(Settings *s,const char *name,const char *path){
 if(!name||!name[0]||strlen(name)>=sizeof s->name)return false;
 snprintf(s->name,sizeof s->name,"%s",name);return settings_save(s,path);
}
