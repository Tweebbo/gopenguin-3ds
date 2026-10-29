#include "settings.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
int __real_rename(const char *,const char *);
static bool failPublish;
static unsigned collisions;
int __wrap_rename(const char *old,const char *next){
 if(!access(next,F_OK)){collisions++;errno=EEXIST;return -1;}
 if(failPublish&&strstr(old,".tmp")){errno=EIO;return -1;}
 return __real_rename(old,next);
}
int main(void){
 char dir[]="/tmp/gopenguin-fat-XXXXXX";assert(mkdtemp(dir));
 char path[256],backup[260];snprintf(path,sizeof path,"%s/settings.json",dir);snprintf(backup,sizeof backup,"%s.bak",path);
 Settings s;settings_load(&s,"/missing-fixture");snprintf(s.session,sizeof s.session,"fixed-fixture-token");snprintf(s.name,sizeof s.name,"Original");
 assert(settings_save(&s,path));
 assert(settings_remember_name(&s,"Renamed",path));assert(collisions);
 Settings saved;assert(settings_load(&saved,path));assert(!strcmp(saved.session,s.session));assert(!strcmp(saved.name,"Renamed"));
 failPublish=true;assert(settings_select_account(&s,s.session,NULL,path));
 assert(!settings_remember_name(&s,"RemoteName",path));assert(!strcmp(s.name,"RemoteName"));assert(!strcmp(s.session,"fixed-fixture-token"));
 assert(settings_load(&saved,path));assert(!strcmp(saved.name,"Renamed"));
 failPublish=false;assert(settings_save(&s,path));
 assert(unlink(path)==0);assert(settings_load(&saved,path));assert(!strcmp(saved.session,"fixed-fixture-token"));
 unlink(backup);rmdir(dir);
 puts("PASS FAT no-overwrite rename, name-only save, unchanged session without I/O, failed publish rollback and backup load");
}
