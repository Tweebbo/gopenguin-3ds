#include "stability_core.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static uint32_t duration(uint64_t a,uint64_t b){return a<b?0:a-b>UINT32_MAX?UINT32_MAX:(uint32_t)(a-b);}
static bool identifier(const char *v,size_t max){
 if(!v||!*v||strlen(v)>max)return false;
 for(;*v;v++)if(!((*v>='a'&&*v<='z')||(*v>='A'&&*v<='Z')||(*v>='0'&&*v<='9')||*v=='_'||*v=='-'||*v=='.'||*v=='+'))return false;
 return true;
}
static uint32_t hash(const char *v){uint32_t h=2166136261u;for(;*v;v++){h^=(unsigned char)*v;h*=16777619u;}return h;}
void stability_init(Stability *s,uint64_t now,const char *session,const char *build,const char *device,bool unclean){
 memset(s,0,sizeof *s);s->started=s->last_frame=now;s->previous_unclean=unclean;
 snprintf(s->session,sizeof s->session,"%s",session);snprintf(s->build,sizeof s->build,"%s",identifier(build,32)?build:"dev");
 snprintf(s->device,sizeof s->device,"%s",device);strcpy(s->room,"unknown");strcpy(s->activity,"room");
}
void stability_event(Stability *s,uint64_t now,unsigned code,uint32_t value){
 if(code<ST_ROOM||code>ST_RESUME)return;
 s->events[s->head]=(StabilityEvent){duration(now,s->started),code,value};s->head=(s->head+1)%STABILITY_EVENTS;
 if(s->count<STABILITY_EVENTS)s->count++;
 if(code==ST_ERROR||code==ST_ROOM||code==ST_ACTIVITY)s->urgent=true;
 if(code==ST_ERROR)s->pending_error=true;
}
void stability_context(Stability *s,uint64_t now,const char *room,const char *activity){
 if(identifier(room,48)&&strcmp(s->room,room)){snprintf(s->room,sizeof s->room,"%s",room);stability_event(s,now,ST_ROOM,hash(room));}
 if(identifier(activity,24)&&strcmp(s->activity,activity)){snprintf(s->activity,sizeof s->activity,"%s",activity);stability_event(s,now,ST_ACTIVITY,hash(activity));}
}
void stability_frame(Stability *s,uint64_t now,uint32_t memory_kib,unsigned power,unsigned network){
 uint32_t delta=duration(now,s->last_frame);s->last_frame=now;
 if(s->frames<UINT32_MAX)s->frames++;
 if(delta>50&&s->slow<UINT32_MAX)s->slow++;
 if(delta>s->max_ms)s->max_ms=delta>1000?1000:delta;
 if(delta>1000)stability_event(s,now,ST_RESUME,delta);
 s->memory_kib=memory_kib;
 if(power!=s->power){s->power=power;stability_event(s,now,ST_POWER,power);}
 if(network!=s->network){s->network=network;stability_event(s,now,ST_NETWORK,network);}
}
bool stability_due(const Stability *s,uint64_t now){return (!s->attempted||duration(now,s->last_attempt)>=5000)&&(!s->sent||duration(now,s->last_send)>=(s->urgent?15000u:120000u));}
static bool append(char **p,size_t *left,const char *format,...){
 va_list args;va_start(args,format);int n=vsnprintf(*p,*left,format,args);va_end(args);
 if(n<0||(size_t)n>=*left)return false;
 *p+=n;*left-=(size_t)n;return true;
}
size_t stability_json(const Stability *s,uint64_t now,const char *reason,char *out,size_t cap){
 if(!cap||strlen(s->session)!=32||!identifier(s->build,32)||!identifier(s->device,8)||!identifier(reason,8))return 0;
 char *p=out;size_t left=cap;
 if(!append(&p,&left,"{\"v\":1,\"session\":\"%s\",\"seq\":%u,\"uptime\":%u,\"build\":\"%s\",\"device\":\"%s\",\"reason\":\"%s\",\"room\":\"%s\",\"activity\":\"%s\",\"frames\":[%u,%u,%u],\"memory_kib\":%u,\"power\":%u,\"previous_unclean\":%s,\"events\":[",
 s->session,s->sequence,duration(now,s->started),s->build,s->device,reason,s->room,s->activity,s->frames,s->slow,s->max_ms,s->memory_kib,s->power,s->previous_unclean?"true":"false"))return 0;
 for(unsigned i=0;i<s->count;i++){
  const StabilityEvent *e=&s->events[(s->head+STABILITY_EVENTS-s->count+i)%STABILITY_EVENTS];
  if(!append(&p,&left,"%s[%u,%u,%u]",i?",":"",e->ms,e->code,e->value))return 0;
 }
 if(!append(&p,&left,"]}"))return 0;
 return (size_t)(p-out);
}
void stability_submitted(Stability *s,uint64_t now){s->last_send=now;s->sent=true;s->urgent=false;s->pending_error=false;if(s->sequence<UINT32_MAX)s->sequence++;}
void stability_attempted(Stability *s,uint64_t now){s->last_attempt=now;s->attempted=true;}
