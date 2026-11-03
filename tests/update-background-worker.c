#include "gp_update.h"
#include "jobthread.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GPUpdateView wait_done(void){
 u64 end=now_ms()+20000;GPUpdateView v;
 do{job_sleep(2);v=gp_update_view();}while(gp_update_busy(v.state)&&now_ms()<end);
 return v;
}
int main(int argc,char **argv){
 assert(argc==3);const char *mode=argv[2];bool old=!strcmp(mode,"old"),saver=!strcmp(mode,"saver");
 gp_update_init(argv[1]);assert(gp_update_check());assert(wait_done().state==GP_UPD_AVAILABLE);
 gp_update_activity(1,false,false,saver,!old);assert(gp_update_prepare());
 GPUpdateView v=gp_update_view();assert(v.background&&!gp_update_blocks_game(v));
 if(!strcmp(mode,"pause")||!strcmp(mode,"pause-drop")||!strcmp(mode,"cancel")){
  job_sleep(40);gp_update_pause(true);job_sleep(100);unsigned before=gp_update_view().done;
  job_sleep(150);assert(gp_update_view().done==before);assert(gp_update_view().paused);
  if(!strcmp(mode,"cancel")){gp_update_request_stop();assert(gp_update_stop_within(1000));puts("PASS cancel paused preparation");return 0;}
  gp_update_pause(false);
 }
 if(!strcmp(mode,"pressure")||!strcmp(mode,"sleep")){
  bool sleep=!strcmp(mode,"sleep");
  gp_update_activity(1,!sleep,sleep,false,false);job_sleep(100);unsigned before=gp_update_view().done;
  for(unsigned n=0;n<4;n++){gp_update_activity(80,!sleep,sleep,false,false);job_sleep(40);}
  assert(gp_update_view().done==before);gp_update_activity(1,false,false,false,false);
 }
 v=wait_done();if(v.state!=GP_UPD_READY){fprintf(stderr,"%u %s\n",v.state,v.message);return 2;}
 assert(!gp_update_blocks_game(v));assert(!gp_update_check());
 FILE *app=fopen(argv[1],"rb");assert(app);char old_bytes[32]={0};fread(old_bytes,1,sizeof old_bytes-1,app);fclose(app);assert(!strcmp(old_bytes,"3DSX previous application"));
 job_sleep(20);
 if(!strcmp(mode,"corrupt")){char temp[512];snprintf(temp,sizeof temp,"%s.part",argv[1]);FILE *f=fopen(temp,"r+b");assert(f);assert(!fseek(f,128,SEEK_SET));fputc(0xff,f);fclose(f);}
 bool missing=!strcmp(mode,"missing-resource"),index_bad=!strcmp(mode,"damaged-index");
 if(missing){unsigned char row[180];FILE *f=fopen(DATA_DIR "/resources/target.idx.part","rb");assert(f&&fread(row,1,sizeof row,f)==sizeof row);fclose(f);char digest[65];for(unsigned n=0;n<32;n++)snprintf(digest+2*n,3,"%02x",row[144+n]);char path[320];snprintf(path,sizeof path,DATA_DIR "/resources/%c/%s.bin",digest[0],digest);assert(!remove(path));}
 if(index_bad){FILE *f=fopen(DATA_DIR "/resources/target.idx.part","r+b");assert(f);fputc('x',f);fclose(f);}
 assert(gp_update_start());v=wait_done();assert(v.state==(!strcmp(mode,"corrupt")||missing||index_bad?GP_UPD_ERROR:GP_UPD_INSTALLED));
 assert(gp_update_stop_within(1000));puts("PASS background prepare, explicit commit, pacing and unchanged running application");return 0;
}
