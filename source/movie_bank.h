#pragma once
#include "movie_meta.h"
#include "swl_gpu.h"
typedef struct MovieBank MovieBank;
MovieBank *movie_bank_load(const char *path,size_t textureLimit,const char **error);
void movie_bank_free(MovieBank *bank);
/** Two-step load: file, scene, metadata and texture inflation (any thread), then the GPU texture
 *  import on the main thread. movie_bank_load does both. */
MovieBank *movie_bank_load_staged(const char *path,size_t textureLimit,const char **error);
bool movie_bank_commit(MovieBank *bank,const char **error);
/** Main thread, after movie_bank_load_staged: imports the next texture page. 1 = all pages imported, 0 = more
 *  remain (call again next frame), -1 = failed (movie_bank_commit then reports the error). */
int movie_bank_import_next(MovieBank *bank);
SWLScene *movie_bank_scene(MovieBank *bank);
MovieMeta *movie_bank_meta(MovieBank *bank);
bool movie_bank_draw(MovieBank *bank,C3D_RenderTarget *target,unsigned width,unsigned height,SWLMatrix view,float interpolation);
bool movie_bank_embed(MovieBank *bank,SWLMatrix view);
bool movie_bank_draw_depth(MovieBank *bank,C3D_RenderTarget *target,unsigned width,unsigned height,SWLMatrix view,float interpolation,bool (*depth)(void *,float),bool (*needed)(void *,float),void *user,bool screenClip);
void movie_bank_unload_textures(MovieBank *bank);
bool movie_bank_reload_textures(MovieBank *bank);
void movie_bank_content(MovieBank *bank,SWLText content,void *user);
void movie_bank_extra(MovieBank *bank,unsigned index,SWLGPUAtlas atlas);
size_t movie_bank_bytes(const MovieBank *bank);
bool movie_bank_text(MovieBank *bank,const char *path,const char *value);
