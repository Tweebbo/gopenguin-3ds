#include "stability_client.h"
#if STABILITY_DIAGNOSTICS
#include "stability_core.h"
#include "penguin_net.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#ifndef STABILITY_PRIVATE_BUILD
#define STABILITY_PRIVATE_BUILD 0
#endif
#if STABILITY_PRIVATE_BUILD
#define STABILITY_BUILD_VERSION GOPENGUIN_VERSION "+stability-private"
#else
#define STABILITY_BUILD_VERSION GOPENGUIN_VERSION
#endif
static Stability state;
static bool initialized;
static char marker[256];
static unsigned previous_owner,marked_owner;
void stability_client_storage(const char *folder){
 snprintf(marker,sizeof marker,"%s/stability-active",folder);FILE *f=fopen(marker,"r");
 if(f){if(fscanf(f,"%u",&previous_owner)!=1)previous_owner=0;fclose(f);}
}
void stability_client_tick(uint64_t now,const char *room,const char *activity,unsigned memory_kib,unsigned power,bool online,unsigned network,unsigned owner){
 if(!initialized||state.owner!=owner){
  uint64_t tick=svcGetSystemTick(),wall=osGetTime();char session[33];bool model=false;
  snprintf(session,sizeof session,"%016llx%016llx",(unsigned long long)tick,(unsigned long long)wall);
  APT_CheckNew3DS(&model);stability_init(&state,now,session,STABILITY_BUILD_VERSION,model?"new3ds":"old3ds",owner&&owner==previous_owner);state.owner=owner;initialized=true;
 }
 stability_context(&state,now,room,activity);
 if(network!=state.network&&(network==PNET_RETRYING||network==PNET_SIGN_IN))stability_event(&state,now,ST_ERROR,1000u+network);
 stability_frame(&state,now,memory_kib,power,network);
 if(online&&owner&&stability_due(&state,now)){
  if(marker[0]&&marked_owner!=owner){FILE *f=fopen(marker,"w");if(f){fprintf(f,"%u\n",owner);fclose(f);}marked_owner=owner;}
  stability_attempted(&state,now);
  state.memory_kib=linearSpaceFree()/1024u;
  char body[STABILITY_JSON_MAX];const char *reason=!state.sent?"start":state.pending_error?"error":"sample";
  if(stability_json(&state,now,reason,body,sizeof body)){
   if(pnet_send_stability(body,state.owner))stability_submitted(&state,now);
  }
 }
}
void stability_client_event(uint64_t now,unsigned code,unsigned value){if(initialized)stability_event(&state,now,code,value);}
void stability_client_close(void){
 if(initialized&&pnet_state()==PNET_ONLINE){char body[STABILITY_JSON_MAX];if(stability_json(&state,state.last_frame,"close",body,sizeof body))pnet_send_stability(body,state.owner);}
 if(marked_owner&&marker[0]){FILE *f=fopen(marker,"w");if(f){fputs("0\n",f);fclose(f);}}
}
#endif
