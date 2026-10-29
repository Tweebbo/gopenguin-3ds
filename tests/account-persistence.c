#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
int main(void){
 char dir[]="/tmp/gopenguin-account-XXXXXX";assert(mkdtemp(dir));
 char path[256],temporary[260];snprintf(path,sizeof path,"%s/settings.json",dir);snprintf(temporary,sizeof temporary,"%s.tmp",path);
 Settings old;settings_load(&old,"/nonexistent-gopenguin-fixture");
 snprintf(old.session,sizeof old.session,"old-fixture-session");snprintf(old.name,sizeof old.name,"Old Penguin");
 assert(settings_save(&old,path));Settings next=old;
 assert(mkdir(temporary,0700)==0);
 char blocker[280];snprintf(blocker,sizeof blocker,"%s/occupied",temporary);FILE *file=fopen(blocker,"wb");assert(file);fclose(file);
 assert(!settings_select_account(&next,"new-fixture-session","New Penguin",path));
 assert(!memcmp(&next,&old,sizeof old));Settings saved;assert(settings_load(&saved,path));assert(!strcmp(saved.session,old.session));assert(!strcmp(saved.name,old.name));
 assert(unlink(blocker)==0);assert(rmdir(temporary)==0);assert(settings_select_account(&next,"new-fixture-session","New Penguin",path));
 assert(settings_load(&saved,path));assert(!strcmp(saved.session,next.session));assert(!strcmp(saved.name,next.name));
 char tooLong[200];memset(tooLong,'x',sizeof tooLong);tooLong[sizeof tooLong-1]=0;
 Settings before=next;assert(!settings_select_account(&next,tooLong,"Invalid",path));assert(!memcmp(&next,&before,sizeof next));
 unlink(path);rmdir(dir);puts("PASS SD write failure retains old account in memory and on disk; successful selection survives reload; oversized credentials rejected");return 0;
}
