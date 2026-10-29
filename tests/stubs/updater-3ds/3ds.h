#pragma once
#include "3ds/types.h"
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#define U64_MAX UINT64_MAX
#define CUR_THREAD_HANDLE 0
#define MEDIATYPE_SD 1
#define R_FAILED(r) ((r)<0)
#define R_SUCCEEDED(r) ((r)>=0)
typedef s32 Result;
typedef u32 Handle;
typedef pthread_mutex_t LightLock;
typedef struct TestThread *Thread;
typedef struct {u64 titleID,size;u16 version;u32 titleType;} AM_TitleInfo;
static inline void LightLock_Init(LightLock *m){pthread_mutex_init(m,NULL);}
static inline void LightLock_Lock(LightLock *m){pthread_mutex_lock(m);}
static inline void LightLock_Unlock(LightLock *m){pthread_mutex_unlock(m);}
void aptSetSleepAllowed(bool allowed);
void aptSetHomeAllowed(bool allowed);
void svcSleepThread(s64 ns);
Result svcGetThreadPriority(s32 *priority,Handle handle);
Thread threadCreate(void (*entry)(void *),void *arg,size_t stack,s32 priority,int core,bool detached);
Result threadJoin(Thread thread,u64 timeout);
void threadFree(Thread thread);
Result amInit(void);
void amExit(void);
Result AM_StartCiaInstall(int media,Handle *handle);
Result FSFILE_Write(Handle handle,u32 *written,u64 offset,const void *data,u32 bytes,u32 flags);
Result AM_FinishCiaInstall(Handle handle);
Result AM_CancelCIAInstall(Handle handle);
Result AM_GetTitleInfo(int media,u32 count,u64 *titles,AM_TitleInfo *info);
