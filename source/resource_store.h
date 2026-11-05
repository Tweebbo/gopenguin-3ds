#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "incremental.h"
typedef struct ResourceIndex ResourceIndex;
typedef struct {uint64_t copied,existing,written,verified,cached;unsigned files;} ResourceStats;
ResourceIndex *resource_index_load(const char *path);
void resource_index_free(ResourceIndex *index);
unsigned resource_index_count(const ResourceIndex *index);
uint64_t resource_index_bytes(const ResourceIndex *index);
bool resource_store_ready(const ResourceIndex *index,const char *directory);
bool resource_store_reuse(const ResourceIndex *index,const char *directory,const char *embedded,
                         const IncrementalHooks *hooks,ResourceStats *stats);
/* Normal updates may reuse immutable objects verified earlier and unchanged on SD.
   Explicit repair uses resource_store_reuse and always checks their contents. */
bool resource_store_reuse_cached(const ResourceIndex *index,const char *directory,const char *embedded,
                                const IncrementalHooks *hooks,ResourceStats *stats);
bool resource_store_bundle(const ResourceIndex *index,const char *directory,const char *bundle,
                          const char *sha256,unsigned bytes,const IncrementalHooks *hooks,ResourceStats *stats);
bool resource_store_init(const char *index,const char *directory);
void resource_store_exit(void);
bool resource_store_enabled(void);
bool resource_store_complete(void);
bool resource_store_path(const char *path,char *out,size_t capacity);
FILE *resource_store_open(const char *path,const char *mode);
