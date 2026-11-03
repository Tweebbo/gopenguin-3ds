#include "jobevent.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
static JobEvent wake;static atomic_uint received;static atomic_bool stop;
static void *worker(void *unused){(void)unused;while(!atomic_load(&stop)){job_event_wait(&wake);atomic_fetch_add(&received,1);}return NULL;}
int main(void){
 job_event_init(&wake);pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));
 job_sleep(150);assert(!atomic_load(&received));
 job_event_signal(&wake);for(unsigned n=0;n<100&&!atomic_load(&received);n++)job_sleep(1);assert(atomic_load(&received)==1);
 job_sleep(150);assert(atomic_load(&received)==1);
 atomic_store(&stop,true);job_event_signal(&wake);assert(!pthread_join(thread,NULL));job_event_free(&wake);
 job_event_init(&wake);job_event_signal(&wake);job_event_wait(&wake);job_event_free(&wake);
 puts("PASS: no periodic idle wakeups, immediate request/stop wakeup and signal-before-wait retained");return 0;
}
