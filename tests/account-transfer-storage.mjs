import assert from 'node:assert/strict';
import {execFileSync,spawnSync} from 'node:child_process';
import {readFile,writeFile,mkdtemp,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'..'),folder=await mkdtemp(path.join(tmpdir(),'gopenguin-transfer-'));
try{
 const main=await readFile(root+'/source/main.c','utf8');
 const extract=name=>{const match=main.match(new RegExp('static bool '+name+'\\([^)]*\\)\\{[\\s\\S]*?\\n\\}'));assert(match,'Missing '+name);return match[0];};
 const functions=['account_storage_ready','account_select_session','account_backup_session'].map(extract).join('\n');
 const code=`#include "settings.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#undef DATA_DIR
#define DATA_DIR ${JSON.stringify(folder)}
#define SETTINGS_FILE DATA_DIR "/settings.json"
static Settings settings;
static unsigned cleared,errors,collisions;
static bool failSettingsPublish;
static struct {unsigned count;} chatLog,chatHistory;
static void chat_log_clear(void *log){(void)log;cleared++;}
static void modal_message(const char *text){(void)text;errors++;}
static bool account_backup_session(void);
int __real_rename(const char *,const char *);
int __wrap_rename(const char *old,const char *next){
 if(!access(next,F_OK)){collisions++;errno=EEXIST;return -1;}
 if(failSettingsPublish&&strstr(old,"settings.json.tmp")){errno=EIO;return -1;}
 return __real_rename(old,next);
}
${functions}
static void check_previous(const char *session){
 char actual[256],expected[256];FILE *file=fopen(DATA_DIR "/previous-session.txt","rb");assert(file);
 assert(fgets(actual,sizeof actual,file));assert(!fclose(file));snprintf(expected,sizeof expected,"TWEEBBO-%s\\n",session);assert(!strcmp(actual,expected));
}
int main(void){
 settings_load(&settings,"/missing-fixture");strcpy(settings.session,"fixture-original");strcpy(settings.name,"Original");assert(settings_save(&settings,SETTINGS_FILE));
 assert(account_storage_ready());check_previous("fixture-original");
 assert(account_select_session("fixture-imported","Imported"));
 Settings saved;assert(settings_load(&saved,SETTINGS_FILE));assert(!strcmp(saved.session,"fixture-imported"));check_previous("fixture-original");assert(cleared==1);
 assert(account_storage_ready());assert(account_select_session("fixture-second","Second"));check_previous("fixture-imported");assert(cleared==2);assert(collisions);
 assert(account_storage_ready());Settings before=settings;failSettingsPublish=true;
 assert(account_select_session(settings.session,NULL));
 assert(!account_select_session("fixture-rejected","Rejected"));assert(!memcmp(&before,&settings,sizeof before));
 assert(settings_load(&saved,SETTINGS_FILE));assert(!strcmp(saved.session,"fixture-second"));assert(cleared==2);failSettingsPublish=false;
 FILE *file=fopen(DATA_DIR "/previous-session.txt","wb");assert(file);assert(fputs("TWEEBBO-fixture-older\\n",file)>=0);assert(!fclose(file));
 assert(!mkdir(DATA_DIR "/previous-session.txt.tmp",0700));file=fopen(DATA_DIR "/previous-session.txt.tmp/blocker","wb");assert(file);assert(!fclose(file));
 assert(!account_storage_ready());assert(!memcmp(&before,&settings,sizeof before));check_previous("fixture-older");
 assert(!unlink(DATA_DIR "/previous-session.txt.tmp/blocker"));assert(!rmdir(DATA_DIR "/previous-session.txt.tmp"));
 assert(account_storage_ready());assert(account_select_session("fixture-third","Third"));check_previous("fixture-second");
 assert(settings_load(&saved,SETTINGS_FILE));assert(!strcmp(saved.session,"fixture-third"));assert(errors==2);
 puts("PASS actual main.c import guards on FAT: first/repeated transfer, existing backup replacement, no-op confirmation, failed-save rollback and failed-backup protection");
 return 0;
}
`;
 await writeFile(folder+'/test.c',code);
 execFileSync('cc',['-O2','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-DNO_TLS','-Isource','-Ibuild/host-include',folder+'/test.c','source/settings.c','source/transport/conn.c','source/common.c','-Wl,--wrap=rename','-pthread','-l:libjansson.so.4','-o',folder+'/test'],{cwd:root,stdio:'inherit'});
 const result=spawnSync(folder+'/test',[],{encoding:'utf8'});
 if(process.env.SD_TRANSFER_REPORT)await writeFile(process.env.SD_TRANSFER_REPORT,JSON.stringify({status:result.status===0?'PASS':'FAIL',actualMainFunctionsExtracted:true,filesystem:'rename refuses overwriting existing files',stdout:result.stdout,stderr:result.stderr,exitCode:result.status,signal:result.signal},null,2)+'\n');
 process.stdout.write(result.stdout);process.stderr.write(result.stderr);assert.equal(result.status,0);
}finally{await rm(folder,{recursive:true,force:true});}
