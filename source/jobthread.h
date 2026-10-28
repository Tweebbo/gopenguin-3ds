#pragma once
#include "common.h"
#ifdef __3DS__
#include <3ds.h>
typedef LightLock JobMutex;
typedef Thread JobThread;
static inline void job_mutex_init(JobMutex *m){LightLock_Init(m);}
static inline void job_lock(JobMutex *m){LightLock_Lock(m);}
static inline void job_unlock(JobMutex *m){LightLock_Unlock(m);}
static inline void job_mutex_free(JobMutex *m){(void)m;}
static inline void job_sleep(unsigned ms){svcSleepThread((s64)ms*1000000);}
#else
#include <pthread.h>
#include <time.h>
typedef pthread_mutex_t JobMutex;
typedef pthread_t JobThread;
static inline void job_mutex_init(JobMutex *m){pthread_mutex_init(m,NULL);}
static inline void job_lock(JobMutex *m){pthread_mutex_lock(m);}
static inline void job_unlock(JobMutex *m){pthread_mutex_unlock(m);}
static inline void job_mutex_free(JobMutex *m){pthread_mutex_destroy(m);}
static inline void job_sleep(unsigned ms){struct timespec t={(time_t)(ms/1000),(long)(ms%1000)*1000000};nanosleep(&t,NULL);}
#endif
