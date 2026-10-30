#include "card_style.h"
#include "jobthread.h"
#include "transport/ws.h"
#include <stdatomic.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
 GPCardStyle current[64],next[64];unsigned count,nextCount;
 JobThread worker;bool started,success;atomic_bool done,quit;
 char api[192];u64 nextRequest;
} S;

static bool colour(json_t *value,GPCardColour *out){
 const char *s=json_string_value(value);if(!s)return false;
 if(!strcmp(s,"blue"))*out=GP_CARD_BLUE;else if(!strcmp(s,"gold"))*out=GP_CARD_GOLD;else if(!strcmp(s,"red"))*out=GP_CARD_RED;else return false;
 return true;
}
bool gp_card_styles_parse(const char *source,size_t bytes,GPCardStyle out[64],unsigned *count){
 if(!source||!count||bytes>16384)return false;
 json_error_t error;json_t *root=json_loadb(source,bytes,JSON_REJECT_DUPLICATES,&error);if(!root)return false;
 json_t *schema=json_object_get(root,"schema"),*players=json_object_get(root,"players");
 bool ok=json_is_integer(schema)&&json_integer_value(schema)==1&&json_is_array(players)&&json_array_size(players)<=64;
 GPCardStyle pending[64];unsigned n=0;
 for(size_t i=0;ok&&i<json_array_size(players);i++){
  json_t *row=json_array_get(players,i),*id=json_object_get(row,"id");json_int_t number=json_integer_value(id);
  ok=json_is_integer(id)&&number>0&&number<=1000000000&&colour(json_object_get(row,"card"),&pending[n].card)&&colour(json_object_get(row,"badge"),&pending[n].badge);
  if(!ok)break;
  pending[n].id=(unsigned)number;for(unsigned j=0;j<n;j++)if(pending[j].id==pending[n].id)ok=false;
  n++;
 }
 if(ok){memcpy(out,pending,n*sizeof *out);*count=n;}
 json_decref(root);return ok;
}
GPCardStyle gp_card_style(unsigned id){for(unsigned i=0;i<S.count;i++)if(S.current[i].id==id)return S.current[i];return (GPCardStyle){.id=id};}
unsigned gp_card_fill(unsigned id){GPCardColour c=gp_card_style(id).card;return c==GP_CARD_GOLD?0xd6ae4a:c==GP_CARD_RED?0x9d202c:0x0085c7;}
unsigned gp_card_ink(unsigned id){return gp_card_style(id).card==GP_CARD_GOLD?0x3d2900:0xffffff;}
static bool cancelled(void *unused){(void)unused;return atomic_load(&S.quit);}
static void worker(void *unused){
 (void)unused;char error[160];S.success=false;
 if(conn_global_init(error,sizeof error)){
  Endpoint endpoint;
  if(endpoint_parse(S.api,&endpoint)){
   const char *start=strstr(S.api,"://"),*slash=start?strchr(start+3,'/'):NULL;char path[256];
   size_t length=slash?strlen(slash):0;while(length&&slash[length-1]=='/')length--;
   if(length<96&&snprintf(path,sizeof path,"%.*s/data/player-card-styles.json",(int)length,slash?slash:"")<(int)sizeof path){
    HttpResult result;http_request_cancel(&endpoint,"GET",path,NULL,NULL,5000,&result,&(ConnCancel){cancelled,NULL});
    if(result.status==200&&result.body&&result.len>0)S.success=gp_card_styles_parse(result.body,(size_t)result.len,S.next,&S.nextCount);
    free(result.body);
   }
  }
  conn_global_exit();
 }
 atomic_store(&S.done,true);
}
#ifndef __3DS__
static void *host_worker(void *unused){worker(unused);return NULL;}
#endif
static void joined(void){
#ifdef __3DS__
 threadFree(S.worker);
#else
 pthread_join(S.worker,NULL);
#endif
 S.started=false;
}
void gp_card_styles_poll(const char *api,bool visible){
 if(S.started){
  if(!atomic_load(&S.done))return;
#ifdef __3DS__
  threadJoin(S.worker,U64_MAX);
#endif
  joined();if(S.success){memcpy(S.current,S.next,S.nextCount*sizeof *S.current);S.count=S.nextCount;}
 }
 if(!visible||!api||!api[0]||strlen(api)>=sizeof S.api||now_ms()<S.nextRequest||atomic_load(&S.quit))return;
 snprintf(S.api,sizeof S.api,"%s",api);S.nextRequest=now_ms()+10000;atomic_store(&S.done,false);
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
 S.worker=threadCreate(worker,NULL,48*1024,priority>0x19?priority-1:priority,-2,false);S.started=S.worker!=NULL;
#else
 S.started=pthread_create(&S.worker,NULL,host_worker,NULL)==0;
#endif
}
bool gp_card_styles_stop_within(unsigned ms){
 atomic_store(&S.quit,true);if(!S.started)return true;
#ifdef __3DS__
 if(R_FAILED(threadJoin(S.worker,(u64)ms*1000000)))return false;
#else
 u64 end=now_ms()+ms;while(!atomic_load(&S.done)&&now_ms()<end)job_sleep(10);if(!atomic_load(&S.done))return false;
#endif
 joined();return true;
}
