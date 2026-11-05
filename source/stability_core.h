#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STABILITY_JSON_MAX 2048
#define STABILITY_EVENTS 24
enum { ST_ROOM=1, ST_ACTIVITY, ST_NETWORK, ST_PANEL, ST_ERROR, ST_SLEEP, ST_POWER, ST_RESUME };
typedef struct { uint32_t ms, code, value; } StabilityEvent;
typedef struct {
 uint64_t started,last_frame,last_send,last_attempt;
 uint32_t sequence,frames,slow,max_ms,memory_kib,power,network,owner;
 unsigned count,head;
 bool sent,urgent,previous_unclean,attempted,pending_error;
 char session[33],build[33],device[9],room[49],activity[25];
 StabilityEvent events[STABILITY_EVENTS];
} Stability;
void stability_init(Stability *s,uint64_t now,const char *session,const char *build,const char *device,bool unclean);
void stability_event(Stability *s,uint64_t now,unsigned code,uint32_t value);
void stability_context(Stability *s,uint64_t now,const char *room,const char *activity);
void stability_frame(Stability *s,uint64_t now,uint32_t memory_kib,unsigned power,unsigned network);
bool stability_due(const Stability *s,uint64_t now);
size_t stability_json(const Stability *s,uint64_t now,const char *reason,char *out,size_t cap);
void stability_submitted(Stability *s,uint64_t now);
void stability_attempted(Stability *s,uint64_t now);
