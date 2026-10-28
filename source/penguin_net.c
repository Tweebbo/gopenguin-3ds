#include "penguin_net.h"
#include "transport/ws.h"
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
typedef LightLock Mutex;
typedef Thread Worker;
static void mutex_init(Mutex *m){LightLock_Init(m);}
static void lock(Mutex *m){LightLock_Lock(m);}
static void unlock(Mutex *m){LightLock_Unlock(m);}
static void sleep_ms(unsigned n){svcSleepThread((s64)n*1000000);}
#else
#include <pthread.h>
#include <time.h>
typedef pthread_mutex_t Mutex;
typedef pthread_t Worker;
static void mutex_init(Mutex *m){pthread_mutex_init(m,NULL);}
static void lock(Mutex *m){pthread_mutex_lock(m);}
static void unlock(Mutex *m){pthread_mutex_unlock(m);}
static void sleep_ms(unsigned n){struct timespec t={(time_t)(n/1000),(long)(n%1000)*1000000};nanosleep(&t,NULL);}
#endif
enum {QUEUE_COUNT=256,QUEUE_BYTES=1024*1024,PACKET_BYTES=65536};
typedef struct Message {struct Message *next;size_t len;char data[];} Message;
typedef struct {Message *head,*tail;unsigned count;size_t bytes;} Queue;
static struct {
 Mutex mutex;Worker worker;bool started,quit,newSession,create,guest,manualRoom;
 PNetConfig config;char session[160],name[64],password[129],status[160],transfer[49];
 unsigned id;PNetState state;Queue incoming,outgoing;
} N;
static char pendingTransfer[49],pendingSession[160];
typedef struct {Ws socket;Endpoint api;char apiPath[96],key[160],error[160],buffer[PACKET_BYTES];unsigned id;size_t buffered;bool joined,signedIn;} Connection;
static bool push(Queue *q,const char *data,size_t len){
 if(len>PACKET_BYTES||q->count>=QUEUE_COUNT||q->bytes+len>QUEUE_BYTES)return false;
 Message *m=malloc(sizeof *m+len);if(!m)return false;m->next=NULL;m->len=len;memcpy(m->data,data,len);
 if(q->tail)q->tail->next=m;else q->head=m;q->tail=m;q->count++;q->bytes+=len;return true;
}
static Message *pop(Queue *q){Message *m=q->head;if(m){q->head=m->next;if(!q->head)q->tail=NULL;q->count--;q->bytes-=m->len;}return m;}
static void clear(Queue *q){Message *m;while((m=pop(q)))free(m);}
static void state(PNetState value,const char *text){lock(&N.mutex);N.state=value;snprintf(N.status,sizeof N.status,"%s",text);unlock(&N.mutex);}
static bool stopping(void){lock(&N.mutex);bool quit=N.quit;unlock(&N.mutex);return quit;}
static bool cancelled(void *unused){(void)unused;return stopping();}
static bool safe_command(const char *command){size_t n=strlen(command);if(!n||n>19)return false;for(const char *p=command;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='#'||*p=='_'))return false;return true;}
bool pnet_packet_to(char *out,size_t cap,char extension,const char *command,unsigned count,const char *const *args){
 if(extension<'a'||extension>'z')return false;
 if(!out||!safe_command(command)||count>120||cap<strlen(command)+12)return false;
 size_t used=(size_t)snprintf(out,cap,"%%xt%%%c%%%s%%-1%%",extension,command);
 for(unsigned i=0;i<count;i++){if(!args||!args[i])return false;for(const char *p=args[i];*p;p++){if(*p=='%')continue;if(used+2>=cap)return false;out[used++]=*p;}out[used++]='%';}
 out[used]=0;return true;
}
bool pnet_packet(char *out,size_t cap,const char *command,unsigned count,const char *const *args){return pnet_packet_to(out,cap,'s',command,count,args);}
bool pnet_parse(char *text,PNetMessage *out){
 memset(out,0,sizeof *out);out->args=out->small;size_t len=strlen(text);if(len<9||len>=PACKET_BYTES||strncmp(text,"%xt%",4)||text[len-1]!='%')return false;
 unsigned separators=0;for(const char *p=text;*p;p++)if(*p=='%')separators++;if(separators<4||separators-4>8192)return false;
 char *command=text+4,*end=strchr(command,'%');if(!end||end-command>19)return false;*end=0;if(!safe_command(command))return false;
 char *roomEnd=strchr(end+1,'%');if(!roomEnd)return false;
 out->count=separators-4;snprintf(out->command,sizeof out->command,"%s",command);
 if(out->count>16){out->args=malloc(out->count*sizeof *out->args);if(!out->args){out->args=out->small;return false;}}
 char *cursor=roomEnd+1;for(unsigned i=0;i<out->count;i++){out->args[i]=cursor;end=strchr(cursor,'%');if(!end){pnet_message_free(out);return false;}*end=0;cursor=end+1;}
 return true;
}
static bool raw(Connection *c,const char *command,unsigned count,const char *const *args){char data[4096];if(!pnet_packet(data,sizeof data,command,count,args))return false;return ws_send_text(&c->socket,(const u8 *)data,(int)strlen(data)+1);}
static bool join_room(Connection *c){PNetConfig config;lock(&N.mutex);config=N.config;unlock(&N.mutex);char id[20],x[20],y[20];snprintf(id,sizeof id,"%u",config.room);snprintf(x,sizeof x,"%d",config.x);snprintf(y,sizeof y,"%d",config.y);return raw(c,"j#jr",3,(const char *[]){id,x,y});}
static void receive(const u8 *bytes,int length,void *user){
 Connection *c=user;if(length<0||(size_t)length>sizeof c->buffer-c->buffered){c->joined=false;return;}memcpy(c->buffer+c->buffered,bytes,length);c->buffered+=(size_t)length;size_t start=0;
 for(size_t i=0;i<c->buffered;i++)if(!c->buffer[i]){
  char *text=c->buffer+start;size_t len=i-start;
  if(len>=4&&!strncmp(text,"%xt%",4)){
   lock(&N.mutex);bool accepted=push(&N.incoming,text,len+1);unlock(&N.mutex);
   if(!accepted){c->joined=false;state(PNET_RETRYING,"Too many pending world updates. Reconnecting...");return;}
   PNetMessage m;if(pnet_parse(text,&m)){
    if(!strcmp(m.command,"gpmsg")&&m.count>=1&&!strcmp(m.args[0],"moved")){lock(&N.mutex);N.quit=true;unlock(&N.mutex);}
    if(!strcmp(m.command,"l")){lock(&N.mutex);N.transfer[0]=0;unlock(&N.mutex);char id[20];snprintf(id,sizeof id,"%u",c->id);c->signedIn=raw(c,"j#js",3,(const char *[]){id,c->key,"en"});}
    else if(!strcmp(m.command,"js")){
     lock(&N.mutex);bool manual=N.manualRoom;unlock(&N.mutex);
     if(manual){c->joined=true;state(PNET_ONLINE,"Connected to GoPenguin");}
     else if(join_room(c))state(PNET_JOINING,"Entering the island...");
    }
    else if(!strcmp(m.command,"jr")){c->joined=true;state(PNET_ONLINE,"Connected to GoPenguin");}
    pnet_message_free(&m);
   }
  }
  start=i+1;
 }
 if(start){memmove(c->buffer,c->buffer+start,c->buffered-start);c->buffered-=start;}
}
static json_t *request(Connection *c,const char *endpoint,const char *session,const char *json,int *status){
 char path[192];if(snprintf(path,sizeof path,"%s/tweebbo/%s",c->apiPath,endpoint)>=(int)sizeof path)return NULL;
 HttpResult result;http_request_cancel(&c->api,"POST",path,session&&*session?session:NULL,json?json:"{}",8000,&result,&(ConnCancel){cancelled,NULL});*status=result.status;if(!result.status)snprintf(c->error,sizeof c->error,"%s",result.err);
 json_error_t error;json_t *data=result.body&&result.len<=65536?json_loadb(result.body,result.len,JSON_REJECT_DUPLICATES,&error):NULL;free(result.body);return data;
}
static bool configure(Connection *c,Endpoint *world,char *worldPath,size_t cap){
 if(!endpoint_parse(N.config.api,&c->api)||!endpoint_parse(N.config.world,world))return false;
 const char *apiStart=strstr(N.config.api,"://")+3,*api=strchr(apiStart,'/');if(api&&strlen(api)>=sizeof c->apiPath)return false;snprintf(c->apiPath,sizeof c->apiPath,"%s",api?api:"");size_t n=strlen(c->apiPath);while(n&&c->apiPath[n-1]=='/')c->apiPath[--n]=0;
 const char *wsStart=strstr(N.config.world,"://")+3,*path=strchr(wsStart,'/');if(path&&strlen(path)>=cap)return false;snprintf(worldPath,cap,"%s",path?path:"/");return true;
}
static bool authenticate(Connection *c){
 char session[160];lock(&N.mutex);snprintf(session,sizeof session,"%s",N.session);unlock(&N.mutex);int status=0;
 if(!*session){
  state(PNET_AUTHENTICATING,N.guest?"Connecting to the island...":N.create?"Creating your penguin...":"Signing in...");
  json_t *body=N.guest?json_object():json_pack("{s:s,s:s}","name",N.name,"password",N.password);char *payload=body?json_dumps(body,JSON_COMPACT):NULL;if(body)json_decref(body);if(!payload)return false;
  json_t *reply=request(c,N.guest?"guest":N.create?"register":"login",NULL,payload,&status);memset(payload,0,strlen(payload));free(payload);memset(N.password,0,sizeof N.password);
  const char *token=reply?json_string_value(json_object_get(reply,"session")):NULL;
  if(status<200||status>=300||!token||!*token||strlen(token)>=sizeof session){
   if(N.guest){const char *why=reply?json_string_value(json_object_get(reply,"error")):NULL;if(why)snprintf(c->error,sizeof c->error,"%s",why);else if(!c->error[0])snprintf(c->error,sizeof c->error,"Could not connect. Retrying...");}
   else state(PNET_SIGN_IN,status==401?"Incorrect name or password.":"Could not sign in. Please try again.");
   if(reply)json_decref(reply);
   return false;
  }
  snprintf(session,sizeof session,"%s",token);lock(&N.mutex);snprintf(N.session,sizeof N.session,"%s",token);N.newSession=true;unlock(&N.mutex);json_decref(reply);
 }
 char transferBody[80];lock(&N.mutex);snprintf(transferBody,sizeof transferBody,"{\"transfer\":\"%s\"}",N.transfer);unlock(&N.mutex);
 json_t *reply=request(c,"world-key",session,transferBody,&status);memset(transferBody,0,sizeof transferBody);const char *key=reply?json_string_value(json_object_get(reply,"worldKey")):NULL;json_int_t id=reply?json_integer_value(json_object_get(reply,"penguinId")):0;
 const char *code=reply?json_string_value(json_object_get(reply,"code")):NULL;
 bool ok=status==200&&key&&strlen(key)<sizeof c->key&&!strstr(key,"]]>")&&id>0&&id<=0x7fffffff;
 if(ok){
  c->id=(unsigned)id;snprintf(c->key,sizeof c->key,"%s",key);
  lock(&N.mutex);N.id=c->id;
  if(code&&*code&&strlen(code)<sizeof N.session){snprintf(N.session,sizeof N.session,"%s",code);N.newSession=true;}
  unlock(&N.mutex);
 }else if(status==401)state(PNET_SIGN_IN,"Your session expired. Sign in again.");
 // A suspension or maintenance explains itself; the worker keeps retrying with backoff.
 else if((status==403||status==503)&&reply){const char *why=json_string_value(json_object_get(reply,"error"));if(why)snprintf(c->error,sizeof c->error,"%s",why);}
 if(reply)json_decref(reply);
 memset(session,0,sizeof session);return ok;
}
static void worker(void *unused){
 (void)unused;char error[160];if(!conn_global_init(error,sizeof error)){state(PNET_SIGN_IN,"Network initialization failed.");return;}unsigned delay=1;
 while(!stopping()){
  Connection *c=calloc(1,sizeof *c);if(!c){state(PNET_SIGN_IN,"Not enough memory for the connection.");break;}Endpoint world;char path[160];bool configured=configure(c,&world,path,sizeof path);
  error[0]=0;state(PNET_CONNECTING,"Connecting to GoPenguin...");bool online=configured&&authenticate(c)&&ws_connect_cancel(&c->socket,&world,path,8000,error,sizeof error,&(ConnCancel){cancelled,NULL});
  if(online){
   c->socket.text_messages=true;char login[512];int n=snprintf(login,sizeof login,"<msg t='sys'><body action='login' r='0'><login z='w1'><nick><![CDATA[%u]]></nick><pword><![CDATA[%s]]></pword></login></body></msg>",c->id,c->key);
   online=n>0&&n<(int)sizeof login&&ws_send_text(&c->socket,(const u8 *)login,n+1);memset(login,0,sizeof login);state(PNET_AUTHENTICATING,"Opening the world session...");u64 deadline=now_ms()+15000;
   while(online&&!stopping()){
    unsigned sent=0;
    while(online&&sent<32&&!stopping()){
     lock(&N.mutex);Message *out=c->joined?pop(&N.outgoing):NULL;unlock(&N.mutex);
     if(!out)break;
     online=ws_send_text(&c->socket,(const u8 *)out->data,(int)out->len);free(out);sent++;
    }
    if(online)online=ws_poll(&c->socket,sent?0:16,receive,c);
    if(!c->joined&&now_ms()>deadline)online=false;
    if(c->joined)delay=1;
   }
  }
  char reason[160];snprintf(reason,sizeof reason,"%s",c->error[0]?c->error:error[0]?error:"Connection lost. Reconnecting...");ws_close(&c->socket);memset(c->key,0,sizeof c->key);free(c);
  lock(&N.mutex);bool signIn=N.state==PNET_SIGN_IN;clear(&N.outgoing);unlock(&N.mutex);if(signIn||!configured)break;
  if(!stopping()){state(PNET_RETRYING,reason);for(unsigned i=0;i<delay*10&&!stopping();i++)sleep_ms(100);if(delay<32)delay*=2;}
 }
 conn_global_exit();
}
#ifndef __3DS__
static void *trampoline(void *p){worker(p);return NULL;}
#endif
static bool start_mode(const PNetConfig *config,const char *session,const char *name,const char *password,bool create,bool guest){
 Endpoint endpoint;if(!config||!memchr(config->api,0,sizeof config->api)||!memchr(config->world,0,sizeof config->world)||!endpoint_parse(config->api,&endpoint)||!endpoint_parse(config->world,&endpoint))return false;
#ifdef GP_LAN_ONLY
 if(!endpoint_parse(config->api,&endpoint)||!endpoint_is_lan(&endpoint)||!endpoint_parse(config->world,&endpoint)||!endpoint_is_lan(&endpoint))return false;
#endif
 if(!config||!config->api[0]||!config->world[0]||strlen(session?session:"")>=sizeof N.session||strlen(name?name:"")>=sizeof N.name||strlen(password?password:"")>=sizeof N.password)return false;
 pnet_stop();memset(&N,0,sizeof N);mutex_init(&N.mutex);N.config=*config;N.create=create;N.guest=guest;snprintf(N.session,sizeof N.session,"%s",session?session:"");snprintf(N.name,sizeof N.name,"%s",name?name:"");snprintf(N.password,sizeof N.password,"%s",password?password:"");snprintf(N.status,sizeof N.status,"Starting the connection...");N.state=PNET_CONNECTING;N.started=true;
 if(session&&pendingSession[0]&&!strcmp(session,pendingSession)){snprintf(N.transfer,sizeof N.transfer,"%s",pendingTransfer);memset(pendingTransfer,0,sizeof pendingTransfer);memset(pendingSession,0,sizeof pendingSession);}
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);N.worker=threadCreate(worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);if(!N.worker){N.started=false;return false;}
#else
 if(pthread_create(&N.worker,NULL,trampoline,NULL)){N.started=false;return false;}
#endif
 return true;
}
bool pnet_start(const PNetConfig *config,const char *session,const char *name,const char *password,bool create){return start_mode(config,session,name,password,create,false);}
bool pnet_start_guest(const PNetConfig *config){return start_mode(config,"","","",false,true);}
void pnet_request_stop(void){if(!N.started)return;lock(&N.mutex);N.quit=true;unlock(&N.mutex);}
bool pnet_stop_within(unsigned ms){
 if(!N.started)return true;
 pnet_request_stop();
#ifdef __3DS__
 // A worker can sit in the system's blocking DNS lookup; never hold the exit for it.
 if(R_FAILED(threadJoin(N.worker,(u64)ms*1000000ULL)))return false;
#else
 (void)ms;
#endif
 pnet_stop();return true;
}
void pnet_stop(void){if(!N.started)return;pnet_request_stop();
#ifdef __3DS__
 threadJoin(N.worker,U64_MAX);threadFree(N.worker);
#else
 pthread_join(N.worker,NULL);
#endif
 clear(&N.incoming);clear(&N.outgoing);memset(N.session,0,sizeof N.session);memset(N.password,0,sizeof N.password);N.started=false;N.state=PNET_OFFLINE;
#ifndef __3DS__
 pthread_mutex_destroy(&N.mutex);
#endif
}
PNetState pnet_state(void){if(!N.started)return PNET_OFFLINE;lock(&N.mutex);PNetState s=N.state;unlock(&N.mutex);return s;}
void pnet_status(char *out,size_t cap){if(!N.started){snprintf(out,cap,"Offline");return;}lock(&N.mutex);snprintf(out,cap,"%s",N.status);unlock(&N.mutex);}
unsigned pnet_id(void){if(!N.started)return 0;lock(&N.mutex);unsigned id=N.id;unlock(&N.mutex);return id;}
bool pnet_take_session(char *out,size_t cap){if(!N.started)return false;lock(&N.mutex);bool changed=N.newSession;if(changed){snprintf(out,cap,"%s",N.session);N.newSession=false;}unlock(&N.mutex);return changed;}
bool pnet_send_to(char extension,const char *command,unsigned count,const char *const *args){if(!N.started)return false;char data[8192];if(!pnet_packet_to(data,sizeof data,extension,command,count,args))return false;lock(&N.mutex);bool ok=N.state==PNET_ONLINE&&push(&N.outgoing,data,strlen(data)+1);unlock(&N.mutex);return ok;}
bool pnet_receive(PNetMessage *message){if(!N.started)return false;lock(&N.mutex);Message *m=pop(&N.incoming);unlock(&N.mutex);if(!m)return false;char *data=malloc(m->len);if(!data){free(m);return false;}memcpy(data,m->data,m->len);free(m);if(!pnet_parse(data,message)){free(data);return false;}message->storage=data;return true;}
void pnet_message_free(PNetMessage *m){free(m->storage);if(m->args&&m->args!=m->small)free(m->args);m->storage=NULL;m->args=m->small;m->count=0;}
bool pnet_send(const char *command,unsigned count,const char *const *args){return pnet_send_to('s',command,count,args);}
void pnet_manual_room(bool manual){if(!N.started)return;lock(&N.mutex);N.manualRoom=manual;unlock(&N.mutex);}
bool pnet_join(unsigned room,int x,int y){if(!N.started)return false;lock(&N.mutex);N.config.room=room;N.config.x=x;N.config.y=y;unlock(&N.mutex);char id[20],sx[20],sy[20];snprintf(id,sizeof id,"%u",room);snprintf(sx,sizeof sx,"%d",x);snprintf(sy,sizeof sy,"%d",y);return pnet_send("j#jr",3,(const char *[]){id,sx,sy});}

/* B4 account requests: recovery and rename over the launcher API, off the render thread. */
typedef struct {
 Mutex mutex;Worker worker;bool started,done;int kind;PNetAccountState state;
 PNetConfig config;char code[160],session[160],name[64],category[16],text[1024];
 char outSession[160],outName[64],outTransfer[49],error[160];
} Account;
static Account ACCOUNT;

static bool account_done(void){lock(&ACCOUNT.mutex);bool done=ACCOUNT.done;unlock(&ACCOUNT.mutex);return done;}
static void account_finish(PNetAccountState result,const char *error){
 lock(&ACCOUNT.mutex);ACCOUNT.state=result;ACCOUNT.done=true;
 if(error)snprintf(ACCOUNT.error,sizeof ACCOUNT.error,"%s",error);else ACCOUNT.error[0]=0;
 unlock(&ACCOUNT.mutex);
}
static void account_join(void){
 if(!ACCOUNT.started)return;
#ifdef __3DS__
 threadJoin(ACCOUNT.worker,U64_MAX);threadFree(ACCOUNT.worker);
#else
 pthread_join(ACCOUNT.worker,NULL);
#endif
 memset(&ACCOUNT.worker,0,sizeof ACCOUNT.worker);ACCOUNT.started=ACCOUNT.done=false;
}
static json_t *account_request(const PNetConfig *config,const char *endpoint,const char *bearer,const char *json,int *status,char *error,size_t cap){
 Endpoint api;
 if(!endpoint_parse(config->api,&api)){snprintf(error,cap,"Set the GoPenguin server in Help first.");return NULL;}
 const char *start=strstr(config->api,"://");start=start?start+3:config->api;
 const char *slash=strchr(start,'/');
 char base[96]="";if(slash){snprintf(base,sizeof base,"%s",slash);size_t n=strlen(base);while(n&&base[n-1]=='/')base[--n]=0;}
 char path[192];
 if(snprintf(path,sizeof path,"%s/tweebbo/%s",base,endpoint)>=(int)sizeof path){snprintf(error,cap,"The server address is too long.");return NULL;}
 HttpResult result;http_request(&api,"POST",path,bearer&&*bearer?bearer:NULL,json,10000,&result);*status=result.status;
 if(!result.status)snprintf(error,cap,"%s",result.err[0]?result.err:"Could not reach the server.");
 json_error_t parseError;json_t *data=result.body&&result.len<=65536?json_loadb(result.body,result.len,JSON_REJECT_DUPLICATES,&parseError):NULL;
 free(result.body);return data;
}
static void account_worker(void *unused){
 (void)unused;char error[160];if(!conn_global_init(error,sizeof error)){account_finish(PNET_ACCOUNT_ERROR,error);return;}
 json_t *reply=NULL;
 bool empty=ACCOUNT.kind==3||ACCOUNT.kind==5;
 json_t *body=empty?NULL:ACCOUNT.kind==4?json_pack("{s:s,s:s,s:s,s:s}","category",ACCOUNT.category,"message",ACCOUNT.text,"platform","3ds","request_id",ACCOUNT.code):json_pack("{s:s}",ACCOUNT.kind==1?"code":"name",ACCOUNT.kind==1?ACCOUNT.code:ACCOUNT.name);
 char *payload=empty?malloc(3):body?json_dumps(body,JSON_COMPACT):NULL;if(empty&&payload)memcpy(payload,"{}",3);if(body)json_decref(body);
 if(!payload){account_finish(PNET_ACCOUNT_ERROR,"Not enough memory for the account request.");conn_global_exit();return;}
 int status=0;
 bool deviceCode=ACCOUNT.kind==1&&strlen(ACCOUNT.code)<=16;
 const char *endpoint=ACCOUNT.kind==1?(deviceCode?"device/redeem":"recovery"):ACCOUNT.kind==2?"rename":ACCOUNT.kind==4?"suggestions":ACCOUNT.kind==5?"device/link":"guest";
 reply=account_request(&ACCOUNT.config,endpoint,ACCOUNT.kind==2||ACCOUNT.kind==4||ACCOUNT.kind==5?ACCOUNT.session:NULL,payload,&status,error,sizeof error);
 memset(payload,0,strlen(payload));free(payload);
 const char *message=reply?json_string_value(json_object_get(reply,"error")):NULL;
 if(ACCOUNT.kind==5){
  const char *code=reply?json_string_value(json_object_get(reply,"code")):NULL;
  if(status==200&&code&&strlen(code)==12&&!strncmp(code,"GP-",3)){
   lock(&ACCOUNT.mutex);snprintf(ACCOUNT.outSession,sizeof ACCOUNT.outSession,"%s",code);unlock(&ACCOUNT.mutex);account_finish(PNET_ACCOUNT_OK,NULL);
  }else account_finish(PNET_ACCOUNT_ERROR,message?message:"Could not create a device code. Try again.");
 }else if(ACCOUNT.kind==4){
  if(status>=200&&status<300)account_finish(PNET_ACCOUNT_OK,NULL);
  else account_finish(PNET_ACCOUNT_ERROR,message?message:error[0]?error:"Captain Rockhopper could not receive your message. Try again.");
 }else if(ACCOUNT.kind==1||ACCOUNT.kind==3){
  const char *session=reply?json_string_value(json_object_get(reply,"session")):NULL;
  const char *name=reply?json_string_value(json_object_get(reply,"name")):NULL;
  if(status>=200&&status<300&&session&&*session&&strlen(session)<sizeof ACCOUNT.outSession){
   const char *transfer=reply?json_string_value(json_object_get(reply,"transfer")):NULL;
   if(transfer&&strlen(transfer)==48){lock(&ACCOUNT.mutex);snprintf(ACCOUNT.outTransfer,sizeof ACCOUNT.outTransfer,"%s",transfer);unlock(&ACCOUNT.mutex);}
   lock(&ACCOUNT.mutex);snprintf(ACCOUNT.outSession,sizeof ACCOUNT.outSession,"%s",session);snprintf(ACCOUNT.outName,sizeof ACCOUNT.outName,"%s",name?name:"");
   unlock(&ACCOUNT.mutex);account_finish(PNET_ACCOUNT_OK,NULL);
  }else account_finish(PNET_ACCOUNT_ERROR,message?message:error[0]?error:status==401?"This access code was not found.":ACCOUNT.kind==3?"Could not create the penguin.":"Could not recover the character.");
 }else{
  const char *name=reply?json_string_value(json_object_get(reply,"name")):NULL;
  if(status>=200&&status<300&&name&&*name&&strlen(name)<sizeof ACCOUNT.outName){
   lock(&ACCOUNT.mutex);snprintf(ACCOUNT.outName,sizeof ACCOUNT.outName,"%s",name);ACCOUNT.outSession[0]=0;
   unlock(&ACCOUNT.mutex);account_finish(PNET_ACCOUNT_OK,NULL);
  }else account_finish(PNET_ACCOUNT_ERROR,message?message:error[0]?error:status==401?"Your session expired. Sign in again.":"Could not change the name.");
 }
 if(reply)json_decref(reply);
 conn_global_exit();
}
#ifndef __3DS__
static void *account_trampoline(void *p){account_worker(p);return NULL;}
#endif
bool pnet_account_recover(const PNetConfig *config,const char *code){
 if(!code||!*code||!config||!config->api[0])return false;
 if(ACCOUNT.started&&!account_done())return false;
 if(ACCOUNT.started)account_join();
 memset(&ACCOUNT,0,sizeof ACCOUNT);mutex_init(&ACCOUNT.mutex);ACCOUNT.kind=1;ACCOUNT.config=*config;ACCOUNT.state=PNET_ACCOUNT_BUSY;
 snprintf(ACCOUNT.code,sizeof ACCOUNT.code,"%s",code);
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);ACCOUNT.worker=threadCreate(account_worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);
 if(!ACCOUNT.worker){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#else
 if(pthread_create(&ACCOUNT.worker,NULL,account_trampoline,NULL)){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#endif
 ACCOUNT.started=true;return true;
}
bool pnet_account_device_link(const PNetConfig *config,const char *session){
 if(!session||!*session||!config||!config->api[0])return false;
 if(ACCOUNT.started&&!account_done())return false;
 if(ACCOUNT.started)account_join();
 memset(&ACCOUNT,0,sizeof ACCOUNT);mutex_init(&ACCOUNT.mutex);ACCOUNT.kind=5;ACCOUNT.config=*config;ACCOUNT.state=PNET_ACCOUNT_BUSY;
 snprintf(ACCOUNT.session,sizeof ACCOUNT.session,"%s",session);
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);ACCOUNT.worker=threadCreate(account_worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);
 if(!ACCOUNT.worker){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#else
 if(pthread_create(&ACCOUNT.worker,NULL,account_trampoline,NULL)){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#endif
 ACCOUNT.started=true;return true;
}
bool pnet_account_guest(const PNetConfig *config){
 if(!config||!config->api[0])return false;
 if(ACCOUNT.started&&!account_done())return false;
 if(ACCOUNT.started)account_join();
 memset(&ACCOUNT,0,sizeof ACCOUNT);mutex_init(&ACCOUNT.mutex);ACCOUNT.kind=3;ACCOUNT.config=*config;ACCOUNT.state=PNET_ACCOUNT_BUSY;
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);ACCOUNT.worker=threadCreate(account_worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);
 if(!ACCOUNT.worker){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#else
 if(pthread_create(&ACCOUNT.worker,NULL,account_trampoline,NULL)){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#endif
 ACCOUNT.started=true;return true;
}
/* A message for the GoPenguin team through Captain Rockhopper (POST /tweebbo/suggestions). */
bool pnet_suggest(const PNetConfig *config,const char *session,const char *category,const char *message,const char *request_id){
 if(!session||!*session||!message||!*message||!config||!config->api[0])return false;
 if(ACCOUNT.started&&!account_done())return false;
 if(ACCOUNT.started)account_join();
 memset(&ACCOUNT,0,sizeof ACCOUNT);mutex_init(&ACCOUNT.mutex);ACCOUNT.kind=4;ACCOUNT.config=*config;ACCOUNT.state=PNET_ACCOUNT_BUSY;
 snprintf(ACCOUNT.session,sizeof ACCOUNT.session,"%s",session);snprintf(ACCOUNT.category,sizeof ACCOUNT.category,"%s",category);
 snprintf(ACCOUNT.text,sizeof ACCOUNT.text,"%s",message);snprintf(ACCOUNT.code,sizeof ACCOUNT.code,"%s",request_id?request_id:"");
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);ACCOUNT.worker=threadCreate(account_worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);
 if(!ACCOUNT.worker){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#else
 if(pthread_create(&ACCOUNT.worker,NULL,account_trampoline,NULL)){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#endif
 ACCOUNT.started=true;return true;
}
bool pnet_account_rename(const PNetConfig *config,const char *session,const char *name){
 if(!session||!*session||!name||!*name)return false;
 if(!config||!config->api[0])return false;
 if(ACCOUNT.started&&!account_done())return false;
 if(ACCOUNT.started)account_join();
 memset(&ACCOUNT,0,sizeof ACCOUNT);mutex_init(&ACCOUNT.mutex);ACCOUNT.kind=2;ACCOUNT.config=*config;ACCOUNT.state=PNET_ACCOUNT_BUSY;
 snprintf(ACCOUNT.session,sizeof ACCOUNT.session,"%s",session);snprintf(ACCOUNT.name,sizeof ACCOUNT.name,"%s",name);
#ifdef __3DS__
 s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);ACCOUNT.worker=threadCreate(account_worker,NULL,64*1024,priority>0x19?priority-1:priority,-2,false);
 if(!ACCOUNT.worker){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#else
 if(pthread_create(&ACCOUNT.worker,NULL,account_trampoline,NULL)){memset(&ACCOUNT,0,sizeof ACCOUNT);return false;}
#endif
 ACCOUNT.started=true;return true;
}
PNetAccountState pnet_account_state(void){
 if(!ACCOUNT.started)return PNET_ACCOUNT_IDLE;
 lock(&ACCOUNT.mutex);PNetAccountState value=ACCOUNT.state;unlock(&ACCOUNT.mutex);return value;
}
PNetAccountState pnet_account_take(int *kind,char *session,size_t session_cap,char *name,size_t name_cap,char *error,size_t error_cap){
 PNetAccountState value=pnet_account_state();
 if(value!=PNET_ACCOUNT_OK&&value!=PNET_ACCOUNT_ERROR)return value;
 lock(&ACCOUNT.mutex);
 if(kind)*kind=ACCOUNT.kind;
 if(session&&session_cap)snprintf(session,session_cap,"%s",ACCOUNT.outSession);
 if(name&&name_cap)snprintf(name,name_cap,"%s",ACCOUNT.outName);
 if(error&&error_cap)snprintf(error,error_cap,"%s",ACCOUNT.error);
 if(value==PNET_ACCOUNT_OK&&ACCOUNT.kind==1&&ACCOUNT.outTransfer[0]){snprintf(pendingTransfer,sizeof pendingTransfer,"%s",ACCOUNT.outTransfer);snprintf(pendingSession,sizeof pendingSession,"%s",ACCOUNT.outSession);}
 ACCOUNT.state=PNET_ACCOUNT_IDLE;
 unlock(&ACCOUNT.mutex);
 account_join();
 return value;
}
