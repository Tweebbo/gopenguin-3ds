// Host replacement for source/game_gpu.c: reads a GPK1 game bank's scene and
// metadata and counts its encoded texture pages, without citro3d.
#include "game_port.h"
#include "package.h"
#include <stdlib.h>

struct GameBank {SWLScene *scene;MovieMeta *meta;size_t textures;};
static int text_glyphs(void *user,const char *path,unsigned id,SWLArt *glyphs,unsigned capacity){GameBank *b=user;return movie_meta_glyphs(b->meta,b->scene,path,id,glyphs,capacity);}

GameBank *game_bank_load(const char *path,size_t limit,const char **error){
 GPPackage p;GameBank *b=calloc(1,sizeof *b);void *data=NULL;size_t bytes=0;
 if(error)*error="Invalid game package.";
 if(!b||!package_open(&p,path))goto fail;
 data=package_read(&p,0,SWL_MAX_FILE,&bytes);if(!data)goto fail;
 b->scene=swl_load(data,bytes,error);free(data);data=NULL;if(!b->scene)goto fail;
 data=package_read(&p,1,2u*1024u*1024u,&bytes);if(!data)goto fail;
 b->meta=movie_meta_load(data,bytes);free(data);data=NULL;if(!b->meta)goto fail;
 if(p.count!=swl_pages(b->scene)+2)goto fail;
 // Tex3DS header byte 3 is the GPU colour format: RGBA8 4 bytes, RGB8 3, 16-bit formats 2.
 for(unsigned i=0;i<swl_pages(b->scene);i++){
  uint8_t *t3x=package_read(&p,i+2,8u*1024u*1024u,&bytes);if(!t3x||bytes<4){free(t3x);goto fail;}
  unsigned format=t3x[3],depth=format==0?4:format==1?3:format<=5?2:1;free(t3x);
  b->textures+=(size_t)SWL_ATLAS_WIDTH*SWL_ATLAS_HEIGHT*depth;
 }
 if(b->textures>limit){if(error)*error="Game textures exceed the budget.";goto fail;}
 swl_text_handler(b->scene,text_glyphs,b);
 return b;
fail:free(data);game_bank_free(b);return NULL;
}
void game_bank_free(GameBank *b){if(!b)return;swl_free(b->scene);movie_meta_free(b->meta);free(b);}
SWLScene *game_bank_scene(GameBank *b){return b?b->scene:NULL;}
MovieMeta *game_bank_meta(GameBank *b){return b?b->meta:NULL;}
size_t game_bank_texture_bytes(const GameBank *b){return b?b->textures:0;}
bool game_bank_text(GameBank *b,const char *path,const char *value){if(!b||!movie_meta_text(b->meta,path,value))return false;swl_invalidate(b->scene);return true;}
