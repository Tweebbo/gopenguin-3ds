#include "igloo_visit.h"
void igloo_visit_begin(IglooVisit *v,unsigned owner,uint64_t now,bool game){*v=(IglooVisit){.owner=owner,.started=now,.requested=now,.active=true,.deferred=game};}
void igloo_visit_layout(IglooVisit *v,uint64_t now){if(v->active&&!v->layout){v->layout=true;v->started=now;}}
void igloo_visit_finish(IglooVisit *v){v->active=v->deferred=false;}
IglooVisitAction igloo_visit_poll(IglooVisit *v,uint64_t now,bool online,bool game){
 if(!v->active)return IGLOO_VISIT_WAIT;
 if(!online){igloo_visit_finish(v);return IGLOO_VISIT_DISCONNECTED;}
 uint64_t elapsed=now>=v->started?now-v->started:0;
 if(elapsed>=(v->layout?30000u:20000u)){igloo_visit_finish(v);return IGLOO_VISIT_TIMEOUT;}
 if(v->deferred){if(game)return IGLOO_VISIT_WAIT;v->deferred=false;v->started=now;return IGLOO_VISIT_REQUEST;}
 if(!v->layout&&v->retries<2&&elapsed>=(v->retries?6000u:2000u)){v->retries++;return IGLOO_VISIT_RETRY;}
 return IGLOO_VISIT_WAIT;
}
