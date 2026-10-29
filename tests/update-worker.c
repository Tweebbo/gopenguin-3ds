#include "gp_update.h"
#include "jobthread.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 gp_update_init(argv[1]);if(!gp_update_check())return 3;
 u64 end=now_ms()+15000;GPUpdateView view;
 do{job_sleep(10);view=gp_update_view();}while(view.state==GP_UPD_CHECKING&&now_ms()<end);
 if(view.state!=GP_UPD_AVAILABLE){fprintf(stderr,"Check failed: %s\n",view.message);return 4;}
 for(unsigned attempt=0;attempt<2;attempt++){
  if(!gp_update_start())return 5;
  do{job_sleep(10);view=gp_update_view();}while(gp_update_busy(view.state)&&now_ms()<end);
  if(view.state==GP_UPD_INSTALLED)break;
 }
 if(view.state!=GP_UPD_INSTALLED){fprintf(stderr,"Install failed: %s\n",view.message);return 6;}
 if(gp_update_check()||gp_update_view().state!=GP_UPD_INSTALLED)return 8;
 if(!gp_update_stop_within(1000))return 7;
 puts("PASS verified update worker");return 0;
}
