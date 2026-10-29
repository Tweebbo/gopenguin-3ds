#include "settings.h"
#include "transport/conn.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static bool field(json_t *root,const char *key,char *out,size_t cap){const char *s=json_string_value(json_object_get(root,key));if(!s||strlen(s)>=cap)return false;strcpy(out,s);return true;}
static bool load_file(Settings *s,const char *path){
 FILE *f=fopen(path,"rb");if(!f)return false;
 char buffer[2049];size_t n=fread(buffer,1,sizeof buffer,f);fclose(f);if(n>2048)return false;
 json_error_t error;json_t *root=json_loadb(buffer,n,JSON_REJECT_DUPLICATES,&error);if(!root)return false;
 Settings parsed=*s;Endpoint endpoint;
 bool ok=field(root,"api",parsed.network.api,sizeof parsed.network.api)&&field(root,"world",parsed.network.world,sizeof parsed.network.world)&&endpoint_parse(parsed.network.api,&endpoint)&&endpoint_parse(parsed.network.world,&endpoint);
 if(ok){field(root,"session",parsed.session,sizeof parsed.session);field(root,"name",parsed.name,sizeof parsed.name);field(root,"assets",parsed.assets,sizeof parsed.assets);field(root,"donate",parsed.donate,sizeof parsed.donate);parsed.audio=!json_is_false(json_object_get(root,"audio"));
#ifndef GP_LAN_ONLY
  if(!parsed.assets[0]&&strstr(parsed.network.api,"gopenguin.lol"))snprintf(parsed.assets,sizeof parsed.assets,"https://gopenguin.lol/native");
#endif
  parsed.batterySaver=json_is_true(json_object_get(root,"batterySaver"));
  *s=parsed;}
 json_decref(root);memset(buffer,0,sizeof buffer);return ok;
}
bool settings_load(Settings *s,const char *path){
 memset(s,0,sizeof *s);s->audio=true;s->network.room=100;s->network.x=380;s->network.y=340;
#ifndef GP_LAN_ONLY
 snprintf(s->network.api,sizeof s->network.api,"https://gopenguin.lol");snprintf(s->network.world,sizeof s->network.world,"wss://gopenguin.lol/world");
 snprintf(s->assets,sizeof s->assets,"https://gopenguin.lol/native");
#endif
 if(load_file(s,path))return true;
 char backup[256];int n=snprintf(backup,sizeof backup,"%s.bak",path);
 return n>0&&n<(int)sizeof backup&&load_file(s,backup);
}
static bool publish_file(const char *temporary,const char *path){
 if(!rename(temporary,path))return true;
 char backup[256];struct stat original;int length=snprintf(backup,sizeof backup,"%s.bak",path);
 if(length<=0||length>=(int)sizeof backup||stat(path,&original)||!S_ISREG(original.st_mode))return false;
 // FAT can refuse replacing an existing file; keep it until the new file is published.
 remove(backup);
 if(rename(path,backup))return false;
 if(!rename(temporary,path))return true;
 rename(backup,path);return false;
}
bool settings_save(const Settings *s,const char *path){
 json_t *root=json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:b,s:b}","api",s->network.api,"world",s->network.world,"session",s->session,"name",s->name,"assets",s->assets,"donate",s->donate,"audio",s->audio,"batterySaver",s->batterySaver);if(!root)return false;
 char temp[256];int n=snprintf(temp,sizeof temp,"%s.tmp",path);bool ok=n>0&&n<(int)sizeof temp;
 if(ok){
  FILE *f=fopen(temp,"wb");ok=f&&json_dumpf(root,f,JSON_COMPACT)==0;if(f&&fclose(f))ok=false;
  if(ok){
   chmod(temp,0600);
   ok=publish_file(temp,path);
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
 if(fclose(file))ok=false;
 if(ok){chmod(temporary,0600);ok=publish_file(temporary,path);}
 if(!ok)remove(temporary);
 return ok;
}
bool settings_select_account(Settings *s,const char *session,const char *name,const char *path){
 if(!session||!session[0]||strlen(session)>=sizeof s->session||(name&&strlen(name)>=sizeof s->name))return false;
 if(!strcmp(s->session,session)&&(!name||!name[0]||!strcmp(s->name,name)))return true;
 Settings next=*s;snprintf(next.session,sizeof next.session,"%s",session);
 if(name&&name[0])snprintf(next.name,sizeof next.name,"%s",name);
 if(!settings_save(&next,path))return false;
 *s=next;return true;
}
bool settings_remember_name(Settings *s,const char *name,const char *path){
 if(!name||!name[0]||strlen(name)>=sizeof s->name)return false;
 snprintf(s->name,sizeof s->name,"%s",name);return settings_save(s,path);
}
