#include "gp_update.h"
#include "jobthread.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 gp_update_init(argv[1]);bool repair=!strcmp(argv[2],"repair");
 if(repair){if(!gp_update_repair_resources())return 3;}else{
  if(!gp_update_check())return 3;
  while(gp_update_busy(gp_update_view().state))job_sleep(2);
  if(gp_update_view().state!=GP_UPD_AVAILABLE)return 4;
 }
 GPUpdateView view={0};
 for(unsigned attempt=0;attempt<3;attempt++){
  if(!repair&&!gp_update_start())return 5;
  do{job_sleep(2);view=gp_update_view();}while(gp_update_busy(view.state));
  if(view.state==GP_UPD_INSTALLED)break;
  if(repair)break;
 }
 if(!gp_update_stop_within(1000))return 7;
 if(view.state!=GP_UPD_INSTALLED){fprintf(stderr,"%s\n",view.message);return 6;}
 puts(repair?"PASS resource repair":"PASS resources plus application update");return 0;
}
