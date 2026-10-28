#pragma once
#include "swl.h"
typedef struct MovieMeta MovieMeta;
typedef struct {
 bool (*custom)(void *user,unsigned id,const char *path);
 void (*page)(void *user,unsigned frame);
 void (*buy)(void *user,unsigned kind,unsigned id);
 void (*adopt)(void *user,unsigned type);
 void (*close)(void *user);
 void (*room)(void *user,const char *room);
} MovieHooks;
MovieMeta *movie_meta_load(const void *bytes,size_t length);
void movie_meta_free(MovieMeta *meta);
bool movie_meta_click(MovieMeta *meta,SWLScene *scene,float x,float y,const MovieHooks *hooks,void *user);
unsigned movie_meta_frames(const MovieMeta *meta);
const char *movie_meta_clip(const MovieMeta *meta);
unsigned movie_meta_source(const MovieMeta *meta,unsigned symbol);
bool movie_meta_text(MovieMeta *meta,const char *path,const char *value);
int movie_meta_glyphs(MovieMeta *meta,const SWLScene *scene,const char *path,unsigned id,SWLArt *glyphs,unsigned capacity);
void *movie_meta_blob(const MovieMeta *meta,const char *section,const char *key,size_t limit,size_t *length);
unsigned movie_meta_number(const MovieMeta *meta,const char *section,const char *key);
