#pragma once
#include "common.h"
#ifndef ASSET_CACHE_REVISION
#define ASSET_CACHE_REVISION "gopenguin-resources-20261028-timeline-v2"
#endif
enum {ASSET_CLOTHING,ASSET_PAPER,ASSET_ICON,ASSET_PHOTO,ASSET_BOOK,ASSET_MOVIE,ASSET_FURNITURE,ASSET_FURNITURE_ICON,ASSET_FLOOR,ASSET_PUFFLE};
static inline unsigned asset_key(unsigned kind,unsigned id){return kind<=ASSET_PUFFLE&&id&&id<=1000000?(kind<<20)|id:0;}
bool asset_cache_start(const char *url,const char *directory);
void asset_cache_stop(void);
/** Exit path: waits at most `ms` for the download worker; false leaves it running. */
bool asset_cache_stop_within(unsigned ms);
void asset_cache_request_stop(void);
bool asset_cache_request(unsigned clothingId,char *path,size_t capacity);
unsigned asset_cache_pending(void);
enum {ASSET_DISABLED,ASSET_QUEUED,ASSET_READY,ASSET_RETRY,ASSET_UNAVAILABLE};
unsigned asset_cache_status(unsigned key);
