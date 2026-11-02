#include "settings.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
static bool failSync;
int __real_fsync(int fd);
int __wrap_fsync(int fd){if(failSync){errno=EIO;return -1;}return __real_fsync(fd);}
static void put(const char *file,const char *text){FILE *f=fopen(file,"wb");assert(f);assert(fputs(text,f)>=0);assert(!fclose(f));}
int main(void){
 char dir[]="/tmp/gopenguin-recovery-XXXXXX";assert(mkdtemp(dir));char main[256],backup[260],staged[260],rollback[270];snprintf(main,sizeof main,"%s/settings.json",dir);snprintf(backup,sizeof backup,"%s.bak",main);snprintf(staged,sizeof staged,"%s.tmp",main);snprintf(rollback,sizeof rollback,"%s.rollback",main);
 const char *valid="{\"api\":\"http://127.0.0.1:24105\",\"world\":\"ws://127.0.0.1:24107\",\"session\":\"fixture-existing-session\",\"name\":\"SavedBird\"}";
 Settings s;put(main,"{\"api\":\"http://127.0.0.1:24105\",\"world\":\"ws://127.0.0.1:24107\"}");put(backup,valid);
 assert(settings_load(&s,main));
#ifdef BASELINE
 printf("Baseline lost the saved session despite a valid backup: %s\n",s.session[0]?"NO":"YES");assert(!s.session[0]);
#else
 assert(!strcmp(s.session,"fixture-existing-session")&&!s.restoreRequired&&s.recoveredFromBackup);assert(settings_select_account(&s,s.session,NULL,main));assert(!s.recoveredFromBackup);Settings repaired;assert(settings_load(&repaired,main));assert(!repaired.recoveredFromBackup);
#endif
 put(main,"{");assert(settings_load(&s,main));assert(!strcmp(s.session,"fixture-existing-session"));
 put(backup,"{");put(rollback,valid);
#ifndef BASELINE
 assert(settings_load(&s,main));assert(!strcmp(s.session,"fixture-existing-session"));
#endif
 unlink(rollback);put(staged,valid);
#ifndef BASELINE
 assert(settings_load(&s,main));assert(!strcmp(s.session,"fixture-existing-session"));
#endif
 unlink(staged);
 assert(!settings_load(&s,main));
#ifndef BASELINE
 put(backup,"{\"api\":\"http://127.0.0.1:24105\",\"world\":\"ws://127.0.0.1:24107\"}");assert(!settings_load(&s,main));assert(s.restoreRequired);put(backup,"{");
#endif
#ifndef BASELINE
 assert(s.restoreRequired);assert(!settings_save(&s,main));
 assert(settings_select_account(&s,"fixture-restored-session","Restored",main));assert(!s.restoreRequired);
 failSync=true;assert(!settings_remember_name(&s,"UnsavedName",main));failSync=false;
 Settings saved;assert(settings_load(&saved,main));assert(!strcmp(saved.session,"fixture-restored-session"));assert(!strcmp(saved.name,"Restored"));
 put(main,"{\"api\":\"http://127.0.0.1:24105\",\"world\":\"ws://127.0.0.1:24107\",\"session\":42}");put(backup,valid);assert(settings_load(&s,main));assert(!strcmp(s.session,"fixture-existing-session"));
 put(main,"{\"api\":\"http://127.0.0.1:25105\",\"world\":\"ws://127.0.0.1:25107\"}");assert(settings_load(&s,main));assert(!s.session[0]&&!s.restoreRequired);
#endif
 unlink(main);unlink(backup);unlink(staged);unlink(rollback);rmdir(dir);
#ifndef BASELINE
 puts("PASS saved identity: missing/invalid session, corrupt settings, rollback/staged recovery, explicit restore, failed SD flush and server-endpoint isolation");
#endif
 return 0;
}
