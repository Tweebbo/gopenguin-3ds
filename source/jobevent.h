#pragma once
#include "jobthread.h"
#ifdef __3DS__
typedef LightEvent JobEvent;
static inline void job_event_init(JobEvent *e){LightEvent_Init(e,RESET_ONESHOT);}
static inline void job_event_signal(JobEvent *e){LightEvent_Signal(e);}
static inline void job_event_wait(JobEvent *e){LightEvent_Wait(e);}
static inline void job_event_free(JobEvent *e){(void)e;}
#else
typedef struct {pthread_mutex_t mutex;pthread_cond_t cond;bool signalled;} JobEvent;
static inline void job_event_init(JobEvent *e){pthread_mutex_init(&e->mutex,NULL);pthread_cond_init(&e->cond,NULL);e->signalled=false;}
static inline void job_event_signal(JobEvent *e){pthread_mutex_lock(&e->mutex);e->signalled=true;pthread_cond_signal(&e->cond);pthread_mutex_unlock(&e->mutex);}
static inline void job_event_wait(JobEvent *e){pthread_mutex_lock(&e->mutex);while(!e->signalled)pthread_cond_wait(&e->cond,&e->mutex);e->signalled=false;pthread_mutex_unlock(&e->mutex);}
static inline void job_event_free(JobEvent *e){pthread_cond_destroy(&e->cond);pthread_mutex_destroy(&e->mutex);}
#endif
