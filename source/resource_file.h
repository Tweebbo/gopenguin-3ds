#pragma once
#include <stdio.h>
/* Small host fixtures can exercise a loader without linking the SD store. */
extern FILE *resource_store_open(const char *path,const char *mode) __attribute__((weak));
static inline FILE *resource_fopen(const char *path,const char *mode){return resource_store_open?resource_store_open(path,mode):fopen(path,mode);}
