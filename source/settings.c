#include "settings.h"
#include "transport/conn.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static bool field(json_t *root,const char *key,char *out,size_t cap){const char *s=json_string_value(json_object_get(root,key));if(!s||strlen(s)>=cap)return false;strcpy(out,s);return true;}
bool settings_load(Settings *s,const char *path){
 memset(s,0,sizeof *s);s->audio=true;s->network.room=100;s->network.x=380;s->network.y=340;
#ifndef GP_LAN_ONLY
 // Release builds play on gopenguin.lol out of the box (crossplay with the web); Help can still change it.
 snprintf(s->network.api,sizeof s->network.api,"https://gopenguin.lol");snprintf(s->network.world,sizeof s->network.world,"wss://gopenguin.lol/world");
 snprintf(s->assets,sizeof s->assets,"https://gopenguin.lol/native");
#endif
 FILE *f=fopen(path,"rb");if(!f)return false;
 char buffer[2049];size_t n=fread(buffer,1,sizeof buffer,f);fclose(f);if(n>2048)return false;
 json_error_t error;json_t *root=json_loadb(buffer,n,JSON_REJECT_DUPLICATES,&error);if(!root)return false;
 Settings parsed=*s;Endpoint endpoint;
 bool ok=field(root,"api",parsed.network.api,sizeof parsed.network.api)&&field(root,"world",parsed.network.world,sizeof parsed.network.world)&&endpoint_parse(parsed.network.api,&endpoint)&&endpoint_parse(parsed.network.world,&endpoint);
 if(ok){field(root,"session",parsed.session,sizeof parsed.session);field(root,"name",parsed.name,sizeof parsed.name);field(root,"assets",parsed.assets,sizeof parsed.assets);field(root,"donate",parsed.donate,sizeof parsed.donate);parsed.audio=!json_is_false(json_object_get(root,"audio"));
#ifndef GP_LAN_ONLY
  if(!parsed.assets[0]&&strstr(parsed.network.api,"gopenguin.lol"))snprintf(parsed.assets,sizeof parsed.assets,"https://gopenguin.lol/native");
#endif
  *s=parsed;}
 json_decref(root);memset(buffer,0,sizeof buffer);return ok;
}
bool settings_save(const Settings *s,const char *path){
 json_t *root=json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:b}","api",s->network.api,"world",s->network.world,"session",s->session,"name",s->name,"assets",s->assets,"donate",s->donate,"audio",s->audio);if(!root)return false;
 char temp[256];int n=snprintf(temp,sizeof temp,"%s.tmp",path);bool ok=n>0&&n<(int)sizeof temp;
 if(ok){FILE *f=fopen(temp,"wb");ok=f&&json_dumpf(root,f,JSON_COMPACT)==0;if(f){if(fclose(f))ok=false;}if(ok){chmod(temp,0600);ok=rename(temp,path)==0;}if(!ok)remove(temp);}
 json_decref(root);return ok;
}
