#include "igloo_visit.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 IglooVisit v={0};
 igloo_visit_begin(&v,42,100,true);
 assert(igloo_visit_poll(&v,200,true,true)==IGLOO_VISIT_WAIT);
 assert(igloo_visit_poll(&v,300,true,false)==IGLOO_VISIT_REQUEST);
 assert(!v.deferred&&v.owner==42);
 assert(igloo_visit_poll(&v,301,true,false)==IGLOO_VISIT_WAIT);
 assert(igloo_visit_poll(&v,2299,true,false)==IGLOO_VISIT_WAIT);
 assert(igloo_visit_poll(&v,2300,true,false)==IGLOO_VISIT_RETRY);
 assert(igloo_visit_poll(&v,2400,true,false)==IGLOO_VISIT_WAIT);
 assert(igloo_visit_poll(&v,6300,true,false)==IGLOO_VISIT_RETRY);
 assert(igloo_visit_poll(&v,8300,true,false)==IGLOO_VISIT_WAIT);
 assert(igloo_visit_poll(&v,20300,true,false)==IGLOO_VISIT_TIMEOUT&&!v.active);
 igloo_visit_begin(&v,43,0,false);igloo_visit_layout(&v,5000);
 igloo_visit_layout(&v,12000);assert(v.started==5000);
 assert(igloo_visit_poll(&v,34000,true,false)==IGLOO_VISIT_WAIT);
 assert(igloo_visit_poll(&v,35000,true,false)==IGLOO_VISIT_TIMEOUT);
 igloo_visit_begin(&v,44,0,true);
 assert(igloo_visit_poll(&v,20000,true,true)==IGLOO_VISIT_TIMEOUT);
 igloo_visit_begin(&v,45,0,false);
 assert(igloo_visit_poll(&v,1,false,false)==IGLOO_VISIT_DISCONNECTED);
 igloo_visit_begin(&v,46,2,false);igloo_visit_finish(&v);
 assert(igloo_visit_poll(&v,99999,true,false)==IGLOO_VISIT_WAIT);
 puts("PASS: igloo visit waits for game exit, retries at 2/6 seconds, ignores duplicate layout deadlines, bounds waits and recovers from disconnect.");
}
