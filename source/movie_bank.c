#include "movie_bank.h"
#include "package.h"
#include <tex3ds.h>
#include <stdlib.h>
struct MovieBank {SWLScene *scene;MovieMeta *meta;C3D_Tex textures[SWL_MAX_PAGES];Tex3DS_SubTexture subs[SWL_MAX_PAGES];unsigned loaded;size_t bytes,limit;void *encoded[SWL_MAX_PAGES];size_t encodedBytes[SWL_MAX_PAGES];SWLGPUAtlas extra[2];SWLText content;void *contentUser;};
static int text_glyphs(void *user,const char *path,unsigned id,SWLArt *glyphs,unsigned capacity){MovieBank *b=user;if(b->content){int count=b->content(b->contentUser,path,id,glyphs,capacity);if(count>=0)return count;}return movie_meta_glyphs(b->meta,b->scene,path,id,glyphs,capacity);}
void movie_bank_unload_textures(MovieBank *b){if(!b)return;for(unsigned i=0;i<b->loaded;i++)C3D_TexDelete(&b->textures[i]);b->loaded=0;b->bytes=0;}
void movie_bank_free(MovieBank *b){if(!b)return;movie_bank_unload_textures(b);for(unsigned i=0;i<SWL_MAX_PAGES;i++)free(b->encoded[i]);swl_free(b->scene);movie_meta_free(b->meta);free(b);}
MovieBank *movie_bank_load(const char *path,size_t limit,const char **error){
 GPPackage p;MovieBank *b=NULL;void *data=NULL;size_t bytes=0;const char *failure="Invalid movie package.";
 if(!package_open(&p,path))goto fail;
 b=calloc(1,sizeof *b);if(!b)goto fail;
 data=package_read(&p,0,SWL_MAX_FILE,&bytes);if(!data)goto fail;
 b->scene=swl_load(data,bytes,&failure);free(data);data=NULL;if(!b->scene)goto fail;
 data=package_read(&p,1,2u*1024u*1024u,&bytes);if(!data)goto fail;
 b->meta=movie_meta_load(data,bytes);free(data);data=NULL;if(!b->meta)goto fail;
 if(p.count!=swl_pages(b->scene)+2)goto fail;
 b->limit=limit;size_t encodedTotal=0;
 for(unsigned i=0;i<swl_pages(b->scene);i++){b->encoded[i]=package_read(&p,i+2,8u*1024u*1024u,&b->encodedBytes[i]);encodedTotal+=b->encodedBytes[i];if(!b->encoded[i]||encodedTotal>8u*1024u*1024u)goto fail;}
 if(!movie_bank_reload_textures(b)){failure="Could not import the movie textures within budget.";goto fail;}
 swl_text_handler(b->scene,text_glyphs,b);return b;
fail:free(data);movie_bank_free(b);if(error)*error=failure;return NULL;
}
bool movie_bank_reload_textures(MovieBank *b){
 if(!b)return false;
 if(b->loaded==swl_pages(b->scene))return true;
 movie_bank_unload_textures(b);
 for(unsigned i=0;i<swl_pages(b->scene);i++){
  Tex3DS_Texture packed=Tex3DS_TextureImport(b->encoded[i],b->encodedBytes[i],&b->textures[i],NULL,false);if(!packed)goto fail;
  b->loaded++;if(Tex3DS_GetNumSubTextures(packed)!=1){Tex3DS_TextureFree(packed);goto fail;}
  b->subs[i]=*Tex3DS_GetSubTexture(packed,0);Tex3DS_TextureFree(packed);
  b->bytes+=b->textures[i].size;if(b->bytes>b->limit||b->textures[i].width!=SWL_ATLAS_WIDTH||b->textures[i].height!=SWL_ATLAS_HEIGHT||Tex3DS_SubTextureRotated(&b->subs[i])||b->subs[i].width!=SWL_ATLAS_WIDTH||b->subs[i].height!=SWL_ATLAS_HEIGHT)goto fail;
  C3D_TexSetFilter(&b->textures[i],GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&b->textures[i],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 }
 return true;
fail:movie_bank_unload_textures(b);return false;
}
SWLScene *movie_bank_scene(MovieBank *b){return b?b->scene:NULL;}
MovieMeta *movie_bank_meta(MovieBank *b){return b?b->meta:NULL;}
size_t movie_bank_bytes(const MovieBank *b){return b?b->bytes:0;}
bool movie_bank_text(MovieBank *b,const char *path,const char *value){if(!b||!movie_meta_text(b->meta,path,value))return false;swl_invalidate(b->scene);return true;}
bool movie_bank_draw_depth(MovieBank *b,C3D_RenderTarget *target,unsigned width,unsigned height,SWLMatrix view,float interpolation,bool (*depth)(void *,float),bool (*needed)(void *,float),void *user,bool screenClip){
 if(!b||b->loaded!=swl_pages(b->scene))return false;
 SWLGPUAtlas atlases[SWL_MAX_PAGES+2]={0};for(unsigned i=0;i<b->loaded;i++)atlases[i]=(SWLGPUAtlas){&b->textures[i],&b->subs[i]};
 atlases[SWL_MAX_PAGES]=b->extra[0];atlases[SWL_MAX_PAGES+1]=b->extra[1];
 return swl_gpu_draw(b->scene,target,width,height,view,-1,interpolation,atlases,depth,needed,user,screenClip);
}

bool movie_bank_draw(MovieBank *b,C3D_RenderTarget *target,unsigned width,unsigned height,SWLMatrix view,float interpolation){return movie_bank_draw_depth(b,target,width,height,view,interpolation,NULL,NULL,NULL,false);}
bool movie_bank_embed(MovieBank *b,SWLMatrix view){if(!b||b->loaded!=swl_pages(b->scene))return false;SWLGPUAtlas atlases[SWL_MAX_PAGES];for(unsigned i=0;i<b->loaded;i++)atlases[i]=(SWLGPUAtlas){&b->textures[i],&b->subs[i]};return swl_gpu_embed(b->scene,view,atlases);}

void movie_bank_content(MovieBank *b,SWLText content,void *user){if(!b)return;b->content=content;b->contentUser=user;swl_invalidate(b->scene);}
void movie_bank_extra(MovieBank *b,unsigned index,SWLGPUAtlas atlas){if(!b||index>=2)return;if(b->extra[index].texture!=atlas.texture||b->extra[index].sub!=atlas.sub){b->extra[index]=atlas;swl_invalidate(b->scene);}}
