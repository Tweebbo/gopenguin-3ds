#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __3DS__
#include <3ds.h>
u64 now_ms(void){return osGetTime();}
#else
#include <time.h>
u64 now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (u64)t.tv_sec*1000+(u64)t.tv_nsec/1000000;}
#endif
int rnd_range(int lo,int hi){return hi>lo?lo+rand()%(hi-lo):lo;}
void dbg_log(const char *fmt,...){(void)fmt;}
