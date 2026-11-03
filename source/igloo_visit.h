#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {unsigned owner,retries;uint64_t started,requested;bool active,deferred,layout;} IglooVisit;
typedef enum {IGLOO_VISIT_WAIT,IGLOO_VISIT_REQUEST,IGLOO_VISIT_RETRY,IGLOO_VISIT_TIMEOUT,IGLOO_VISIT_DISCONNECTED} IglooVisitAction;
void igloo_visit_begin(IglooVisit *visit,unsigned owner,uint64_t now,bool leaving_game);
void igloo_visit_layout(IglooVisit *visit,uint64_t now);
void igloo_visit_finish(IglooVisit *visit);
IglooVisitAction igloo_visit_poll(IglooVisit *visit,uint64_t now,bool online,bool game_active);
