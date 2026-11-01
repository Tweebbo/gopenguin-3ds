#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
typedef enum {DECOMPRESS_DUMMY=0} decompressType;
typedef ssize_t (*decompressCallback)(void *,void *,size_t);
typedef struct {void *data;size_t size;} decompressIOVec;
ssize_t decompressHeader(decompressType *,size_t *,decompressCallback,void *,size_t);
bool decompressV(const decompressIOVec *,size_t,decompressCallback,void *,size_t);
static inline bool decompress(void *output,size_t size,decompressCallback cb,void *user,size_t bytes){decompressIOVec vec={output,size};return decompressV(&vec,1,cb,user,bytes);}
