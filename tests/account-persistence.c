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
 assert(!old.hideOtherPenguins);
 FILE *legacy=fopen(path,"w");assert(legacy);fputs("{\"api\":\"https://gopenguin.lol\",\"world\":\"wss://gopenguin.lol/world\"}",legacy);assert(!fclose(legacy));
 old.hideOtherPenguins=true;assert(settings_load(&old,path));assert(!old.hideOtherPenguins);
 old.hideOtherPenguins=true;
 snprintf(old.session,sizeof old.session,"old-fixture-session");snprintf(old.name,sizeof old.name,"Old Penguin");
 assert(settings_save(&old,path));Settings next=old;
 assert(mkdir(temporary,0700)==0);
 char blocker[280];snprintf(blocker,sizeof blocker,"%s/occupied",temporary);FILE *file=fopen(blocker,"wb");assert(file);fclose(file);
 assert(!settings_select_account(&next,"new-fixture-session","New Penguin",path));
 assert(!memcmp(&next,&old,sizeof old));Settings saved;assert(settings_load(&saved,path));assert(!strcmp(saved.session,old.session));assert(!strcmp(saved.name,old.name));
 assert(unlink(blocker)==0);assert(rmdir(temporary)==0);assert(settings_select_account(&next,"new-fixture-session","New Penguin",path));
 assert(settings_load(&saved,path));assert(!strcmp(saved.session,next.session));assert(!strcmp(saved.name,next.name));
 assert(saved.hideOtherPenguins);
 char tooLong[200];memset(tooLong,'x',sizeof tooLong);tooLong[sizeof tooLong-1]=0;
 Settings before=next;assert(!settings_select_account(&next,tooLong,"Invalid",path));assert(!memcmp(&next,&before,sizeof next));
 next.hideOtherPenguins=false;assert(settings_save(&next,path));assert(settings_load(&saved,path));assert(!saved.hideOtherPenguins);
 unlink(path);rmdir(dir);puts("PASS SD failure rollback, account reload, oversized credentials, legacy graphics default and hide-penguin setting on/off persistence");return 0;
}
