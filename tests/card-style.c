#define _POSIX_C_SOURCE 200809L
#include "card_style.h"
#include "transport/ws.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static atomic_ullong clockValue=100000;
static atomic_uint requests;
static atomic_bool blocked;
static const char *response="{\"schema\":1,\"players\":[{\"id\":109,\"card\":\"gold\",\"badge\":\"gold\"}]}";
static int status=200;
u64 now_ms(void){return atomic_load(&clockValue);}
bool endpoint_parse(const char *url,Endpoint *out){memset(out,0,sizeof *out);return !strcmp(url,"https://fixture.invalid");}
bool conn_global_init(char *error,int capacity){(void)error;(void)capacity;return true;}
void conn_global_exit(void){}
void http_request_cancel(const Endpoint *endpoint,const char *method,const char *path,const char *bearer,const char *json,int timeout,HttpResult *out,const ConnCancel *cancel){
 (void)endpoint;(void)json;assert(!strcmp(method,"GET"));assert(!strcmp(path,"/data/player-card-styles.json"));assert(!bearer&&timeout==5000);
 atomic_fetch_add(&requests,1);memset(out,0,sizeof *out);
 while(atomic_load(&blocked)&&!cancel->requested(cancel->user)){struct timespec delay={0,1000000};nanosleep(&delay,NULL);}
 if(cancel->requested(cancel->user))return;
 out->status=status;out->body=strdup(response);out->len=(int)strlen(response);
}
static void wait_style(GPCardColour colour){
 for(unsigned i=0;i<1000;i++){
  gp_card_styles_poll("https://fixture.invalid",false);
  if(gp_card_style(109).card==colour)return;
  struct timespec delay={0,1000000};nanosleep(&delay,NULL);
 }
 assert(!"style request did not finish");
}
static void rejected(const char *json){
 GPCardStyle output[64]={{.id=777}};unsigned count=33;
 assert(!gp_card_styles_parse(json,strlen(json),output,&count));assert(output[0].id==777&&count==33);
}
int main(void){
 GPCardStyle output[64];unsigned count=0;assert(gp_card_styles_parse(response,strlen(response),output,&count));assert(count==1&&output[0].id==109&&output[0].badge==GP_CARD_GOLD);
 rejected("{\"schema\":1,\"players\":[{\"id\":109,\"card\":\"green\",\"badge\":\"gold\"}]}");
 rejected("{\"schema\":1,\"players\":[{\"id\":0,\"card\":\"gold\",\"badge\":\"gold\"}]}");
 rejected("{\"schema\":true,\"players\":[]}");rejected("{\"schema\":1,\"players\":[],\"players\":[]}");
 rejected("{\"schema\":1,\"players\":[{\"id\":109,\"card\":\"red\",\"badge\":\"blue\"},{\"id\":109,\"card\":\"gold\",\"badge\":\"gold\"}]}");
 char oversized[16385];memset(oversized,' ',sizeof oversized);assert(!gp_card_styles_parse(oversized,sizeof oversized,output,&count));
 assert(gp_card_style(999).card==GP_CARD_BLUE);
 gp_card_styles_poll("https://fixture.invalid",true);wait_style(GP_CARD_GOLD);assert(atomic_load(&requests)==1);
 for(unsigned i=0;i<100;i++)gp_card_styles_poll("https://fixture.invalid",true);
 assert(atomic_load(&requests)==1);
 assert(gp_card_fill(109)==0xd6ae4a&&gp_card_ink(109)==0x3d2900);
 response="{\"schema\":1,\"players\":[{\"id\":109,\"card\":\"red\",\"badge\":\"blue\"}]}";
 atomic_fetch_add(&clockValue,10001);gp_card_styles_poll("https://fixture.invalid",true);wait_style(GP_CARD_RED);assert(gp_card_style(109).badge==GP_CARD_BLUE);
 atomic_store(&blocked,true);atomic_fetch_add(&clockValue,10001);gp_card_styles_poll("https://fixture.invalid",true);assert(gp_card_styles_stop_within(1000));assert(gp_card_style(109).card==GP_CARD_RED);
 puts("PASS card styles: validation, atomic rejection, default palette, live palette change, request throttling, separate badge colour and bounded cancellation");
}
