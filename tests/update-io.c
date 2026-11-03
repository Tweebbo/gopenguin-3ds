#include <stdio.h>
#include <stdlib.h>
static unsigned long reads,writes;
size_t __real_fread(void *,size_t,size_t,FILE *);
size_t __real_fwrite(const void *,size_t,size_t,FILE *);
void *__real_malloc(size_t);
size_t __wrap_fread(void *p,size_t size,size_t count,FILE *file){reads++;return __real_fread(p,size,count,file);}
size_t __wrap_fwrite(const void *p,size_t size,size_t count,FILE *file){writes++;return __real_fwrite(p,size,count,file);}
void *__wrap_malloc(size_t size){if(size==131072&&getenv("FAIL_LARGE_BUFFER"))return NULL;return __real_malloc(size);}
static void report(void){fprintf(stderr,"{\"freadCalls\":%lu,\"fwriteCalls\":%lu}\n",reads,writes);}
__attribute__((constructor)) static void start(void){atexit(report);}
