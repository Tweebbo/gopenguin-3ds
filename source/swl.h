#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWL_MAX_FILE (16u*1024u*1024u)
#define SWL_MAX_HEADS 1024
#define SWL_MAX_DEPTH 24
#define SWL_PATH 256
#define SWL_ATLAS_WIDTH 1024
#define SWL_ATLAS_HEIGHT 512
#define SWL_MAX_PAGES 6
#define SWL_MAX_DRAWS 4096
#define SWL_MAX_MASKS 8
#define SWL_MAX_INSTANCES 8192
#define SWL_MAX_HITS 256

typedef struct SWLScene SWLScene;
typedef struct {float a,b,c,d,x,y;} SWLMatrix;
typedef struct {float mult[4],add[4];} SWLColor;
typedef struct {uint32_t id,page,x,y,w,h;float left,top,width,height;} SWLArt;
typedef struct {uint32_t symbol,frame;SWLMatrix matrix;} SWLInstance;
typedef struct {uint32_t id;char path[SWL_PATH];SWLMatrix matrix;} SWLHit;
typedef bool (*SWLEmit)(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *color);
typedef struct {
 SWLEmit emit;
 bool (*mask)(void *user,unsigned event);
 bool (*color)(void *user,unsigned event,const SWLColor *color);
 bool (*depth)(void *user,float depth);
} SWLRenderer;
typedef void (*SWLSound)(void *user,const char *path,unsigned id,unsigned flags,unsigned loops);
typedef int (*SWLText)(void *user,const char *path,unsigned id,SWLArt *glyphs,unsigned capacity);
typedef struct {unsigned heads,draws,masks,colorGroups;size_t bytes;uint64_t preparations;} SWLStats;

SWLScene *swl_load(const void *data,size_t bytes,const char **error);
SWLScene *swl_load_file(const char *path,const char **error);
void swl_free(SWLScene *scene);
unsigned swl_pages(const SWLScene *scene);
float swl_rate(const SWLScene *scene);
void swl_begin(SWLScene *scene);
bool swl_prepare(SWLScene *scene);
bool swl_draw(SWLScene *scene,SWLMatrix view,int layer,SWLEmit emit,void *user);
bool swl_render(SWLScene *scene,SWLMatrix view,int layer,float interpolation,const SWLRenderer *renderer,void *user);
/** Display another frame without seeking its timeline or executing that frame's actions.
 *  Clear with frame 0 before ticking the scene; used for a second-screen composition. */
bool swl_view_frame(SWLScene *scene,const char *path,unsigned frame);
bool swl_tick(SWLScene *scene);
bool swl_goto(SWLScene *scene,const char *path,unsigned frame,const char *label,bool playing);
bool swl_target(SWLScene *scene,const char *path,unsigned frame,const char *label,bool playing);
bool swl_color(SWLScene *scene,const char *path,SWLColor color);
bool swl_transform(SWLScene *scene,const char *path,SWLMatrix matrix);
bool swl_instance(SWLScene *scene,const char *path,SWLInstance *instance);
/** Current timeline geometry, including hidden clips, without rebuilding draw commands. */
bool swl_pose(const SWLScene *scene,const char *path,SWLInstance *instance);
unsigned swl_frames(SWLScene *scene,const char *path);
bool swl_art(const SWLScene *scene,unsigned id,SWLArt *art);
bool swl_bounds(SWLScene *scene,float bounds[4]);
void swl_text_handler(SWLScene *scene,SWLText handler,void *user);
void swl_invalidate(SWLScene *scene);
unsigned swl_hits(SWLScene *scene);
const SWLHit *swl_hit(SWLScene *scene,unsigned index);
unsigned swl_frame(const SWLScene *scene,const char *path);
const char *swl_error(const SWLScene *scene);
bool swl_visible(SWLScene *scene,const char *path,bool visible);
void swl_sound_handler(SWLScene *scene,SWLSound handler,void *user);
bool swl_restart(SWLScene *scene);
SWLStats swl_stats(const SWLScene *scene);
SWLMatrix swl_matrix_multiply(SWLMatrix parent,SWLMatrix child);
