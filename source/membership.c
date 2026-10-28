// GoPenguin membership on the 3DS: the console creates a short-lived link, shows it as a
// QR code, and polls the server. A phone or PC opens the link, pays with PayPal, and the
// server grants 15,000 coins, the member flag and the Founder cosmetic set exactly once.
#include "membership.h"
#include "transport/conn.h"
#include "transport/ws.h"
#include "../vendor/qrcodegen/qrcodegen.h"
#include "jobthread.h"
#include <stdatomic.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {KIND_LINK=1,KIND_STATUS=2};
static struct {
  atomic_int state;
  JobThread worker;
  bool started,tookPaid;
  atomic_bool done;
  int kind,priceCents;
  unsigned coins;
  PNetConfig config;
  bool configured,member;
  char session[160],token[80],url[192],message[180],itemsText[160];
  u64 nextPoll;
  uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
} M;

static void set_error(const char *text) {
  snprintf(M.message,sizeof M.message,"%s",text);
  M.state=MEMBERSHIP_ERROR;
}

static bool request_path(const PNetConfig *config,const char *endpoint,char *path,size_t cap) {
  const char *start=strstr(config->api,"://");
  if(!start)return false;
  start+=3;
  const char *slash=strchr(start,'/');
  char base[96]="";
  if(slash){snprintf(base,sizeof base,"%s",slash);size_t n=strlen(base);while(n&&base[n-1]=='/')base[--n]=0;}
  return snprintf(path,cap,"%s/tweebbo/membership/%s",base,endpoint)<(int)cap;
}

static void parse_error(json_t *j,const char *fallback) {
  const char *e=j?json_string_value(json_object_get(j,"error")):NULL;
  set_error(e&&*e?e:fallback);
}

static void parse_link(json_t *j) {
  const char *url=j?json_string_value(json_object_get(j,"url")):NULL;
  const char *token=j?json_string_value(json_object_get(j,"token")):NULL;
  if(!url||!token||strlen(url)>=sizeof M.url||strlen(token)>=sizeof M.token){set_error("The server did not return a payment code.");return;}
  snprintf(M.url,sizeof M.url,"%s",url);
  snprintf(M.token,sizeof M.token,"%s",token);
  M.priceCents=(int)json_integer_value(json_object_get(j,"priceCents"));
  M.coins=(unsigned)json_integer_value(json_object_get(j,"coins"));
  M.configured=json_is_true(json_object_get(j,"configured"));
  M.member=json_is_true(json_object_get(j,"member"));
  json_t *names=json_object_get(j,"itemNames");
  M.itemsText[0]=0;
  if(json_is_array(names)){
    size_t used=0;
    for(size_t i=0;i<json_array_size(names);i++){
      const char *name=json_string_value(json_array_get(names,i));
      if(!name)continue;
      int n=snprintf(M.itemsText+used,sizeof M.itemsText-used,"%s%s",used?", ":"",name);
      if(n<0||(size_t)n>=sizeof M.itemsText-used)break;
      used+=(size_t)n;
    }
  }
  uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
  if(!qrcodegen_encodeText(M.url,temp,M.qr,qrcodegen_Ecc_MEDIUM,qrcodegen_VERSION_MIN,8,qrcodegen_Mask_AUTO,true)){
    set_error("Could not draw the payment code.");
    return;
  }
  if(!M.configured){set_error("Payments are not open yet.");return;}
  snprintf(M.message,sizeof M.message,"Scan the code with your phone and pay there. This screen updates by itself.");
  M.nextPoll=now_ms()+4000;
  M.state=MEMBERSHIP_READY;
}

static void parse_status(json_t *j) {
  if(json_is_true(json_object_get(j,"paid"))){
    M.member=json_is_true(json_object_get(j,"member"));
    if(json_is_false(json_object_get(j,"benefits")))snprintf(M.message,sizeof M.message,"Thank you for your donation! This amount does not include membership, coins or the Founder set.");
    else snprintf(M.message,sizeof M.message,"Thank you! You are a GoPenguin member for life. %u coins and the Founder set were added.",M.coins);
    M.state=MEMBERSHIP_PAID;
  }else{
    M.nextPoll=now_ms()+4000;
    M.state=MEMBERSHIP_READY;
  }
}

static void worker(void *unused) {
  (void)unused;
  char error[160];
  if(!conn_global_init(error,sizeof error)){set_error(error);M.done=true;return;}
  Endpoint api;
  if(!endpoint_parse(M.config.api,&api)){set_error("Set the GoPenguin server in Help first.");conn_global_exit();M.done=true;return;}
  char path[192];
  const char *endpoint=M.kind==KIND_LINK?"link":"status";
  if(!request_path(&M.config,endpoint,path,sizeof path)){set_error("The server address is too long.");conn_global_exit();M.done=true;return;}
  char full[320];
  if(M.kind==KIND_STATUS)snprintf(full,sizeof full,"%s?token=%s",path,M.token);
  else snprintf(full,sizeof full,"%s",path);
  HttpResult result;
  http_request(&api,M.kind==KIND_LINK?"POST":"GET",full,M.session,M.kind==KIND_LINK?"{}":NULL,10000,&result);
  json_error_t parseError;
  json_t *j=result.body&&result.len<=65536?json_loadb(result.body,result.len,JSON_REJECT_DUPLICATES,&parseError):NULL;
  if(M.kind==KIND_STATUS&&(result.status==0||result.status==429||result.status>=500)){
    M.nextPoll=now_ms()+8000;
  }
  else if(result.status!=200){parse_error(j,result.status==401?"Reconnect to GoPenguin and try again.":"Could not reach the payment server.");}
  else if(!j){set_error("The payment server sent an unreadable answer.");}
  else if(M.kind==KIND_LINK)parse_link(j);
  else parse_status(j);
  if(j)json_decref(j);
  free(result.body);
  conn_global_exit();
  M.done=true;
}

static void join_worker(void) {
  if(!M.started)return;
#ifdef __3DS__
  threadJoin(M.worker,U64_MAX);
  threadFree(M.worker);
#else
  pthread_join(M.worker,NULL);
#endif
  M.started=M.done=false;
}
#ifndef __3DS__
static void *host_worker(void *unused){worker(unused);return NULL;}
#endif

static bool start_worker(int kind) {
  if(M.started&&!M.done)return false;
  if(M.started)join_worker();
  M.kind=kind;M.done=false;if(kind==KIND_LINK)M.state=MEMBERSHIP_LOADING;
#ifdef __3DS__
  s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
  M.worker=threadCreate(worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);
  if(!M.worker){set_error("Not enough memory for the payment request.");return false;}
#else
  if(pthread_create(&M.worker,NULL,host_worker,NULL)){set_error("Could not start the payment request.");return false;}
#endif
  M.started=true;
  return true;
}

void membership_link_open(const PNetConfig *config,const char *session) {
  if(!config||!config->api[0]||!session||!*session){set_error("Connect to GoPenguin first.");return;}
  if(M.started&&!M.done)return;
  M.config=*config;
  snprintf(M.session,sizeof M.session,"%s",session);
  M.tookPaid=false;
  M.url[0]=M.token[0]=M.itemsText[0]=0;
  M.priceCents=999;M.coins=15000;
  if(!start_worker(KIND_LINK))return;
}

void membership_tick(u64 now) {
  if(M.started&&M.done)join_worker();
  if(M.state==MEMBERSHIP_READY&&!M.started&&now>=M.nextPoll)start_worker(KIND_STATUS);
}

void membership_close(void) {
  if(M.started&&!M.done)return;
  if(M.started)join_worker();
  memset(&M,0,sizeof M);
}

MembershipState membership_state(void){return M.state;}
const char *membership_message(void){return M.message;}
const char *membership_url(void){return M.url;}
int membership_price_cents(void){return M.priceCents;}
unsigned membership_coins(void){return M.coins;}
bool membership_configured(void){return M.configured;}
bool membership_member(void){return M.member;}
const char *membership_items(void){return M.itemsText;}
int membership_qr_size(void){return M.state==MEMBERSHIP_READY?qrcodegen_getSize(M.qr):0;}
bool membership_qr_dark(int x,int y){return qrcodegen_getModule(M.qr,x,y);}

bool membership_take_paid(void) {
  if(M.state!=MEMBERSHIP_PAID||M.tookPaid)return false;
  M.tookPaid=true;
  return true;
}
