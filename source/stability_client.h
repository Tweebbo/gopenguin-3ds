#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "stability_core.h"
#ifndef STABILITY_DIAGNOSTICS
#define STABILITY_DIAGNOSTICS 0
#endif
#if STABILITY_DIAGNOSTICS
void stability_client_tick(uint64_t now,const char *room,const char *activity,unsigned memory_kib,unsigned power,bool online,unsigned network,unsigned owner);
void stability_client_event(uint64_t now,unsigned code,unsigned value);
void stability_client_close(void);
void stability_client_storage(const char *folder);
#else
static inline void stability_client_tick(uint64_t now,const char *room,const char *activity,unsigned memory_kib,unsigned power,bool online,unsigned network,unsigned owner){(void)now;(void)room;(void)activity;(void)memory_kib;(void)power;(void)online;(void)network;(void)owner;}
static inline void stability_client_event(uint64_t now,unsigned code,unsigned value){(void)now;(void)code;(void)value;}
static inline void stability_client_close(void){}
static inline void stability_client_storage(const char *folder){(void)folder;}
#endif
