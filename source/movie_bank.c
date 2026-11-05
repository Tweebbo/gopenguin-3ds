#include "movie_bank.h"
#include "package.h"
#include <tex3ds.h>
#include <stdlib.h>
#include <string.h>
#include <3ds/util/decompress.h>
struct MovieBank {SWLScene *scene;MovieMeta *meta;C3D_Tex textures[SWL_MAX_PAGES];Tex3DS_SubTexture subs[SWL_MAX_PAGES];unsigned loaded;size_t bytes,limit;void *encoded[SWL_MAX_PAGES];size_t encodedBytes[SWL_MAX_PAGES];void *raw[SWL_MAX_PAGES];size_t rawBytes[SWL_MAX_PAGES];SWLGPUAtlas extra[2];SWLText content;void *contentUser;};
static int text_glyphs(void *user,const char *path,unsigned id,SWLArt *glyphs,unsigned capacity){MovieBank *b=user;if(b->content){int count=b->content(b->contentUser,path,id,glyphs,capacity);if(count>=0)return count;}return movie_meta_glyphs(b->meta,b->scene,path,id,glyphs,capacity);}
void movie_bank_unload_textures(MovieBank *b){if(!b)return;for(unsigned i=0;i<b->loaded;i++)C3D_TexDelete(&b->textures[i]);b->loaded=0;b->bytes=0;}
void movie_bank_free(MovieBank *b){if(!b)return;movie_bank_unload_textures(b);for(unsigned i=0;i<SWL_MAX_PAGES;i++){free(b->encoded[i]);free(b->raw[i]);}swl_free(b->scene);movie_meta_free(b->meta);free(b);}
/* Inflate one LZ texture page into plain memory behind a "dummy" (uncompressed) header, so the
   later Tex3DS import is only a linear allocation and a copy. Safe off the main thread: it uses
   malloc only (libctru's linear heap has no lock and stays on the main thread). */
static void *inflate_page(const void *in,size_t inBytes,size_t limit,size_t *outBytes){
 /* Tex3DS stores a five-byte texture header and one twelve-byte subtexture before
    the compressed payload. Keep both headers for Tex3DS_TextureImport. */
 const unsigned char *data=in;const size_t prefix=17;
 if(inBytes<prefix+4||data[0]!=1||data[1]||data[2]&0xc0||data[4]||
    (8u<<(data[2]&7))!=SWL_ATLAS_WIDTH||(8u<<((data[2]>>3)&7))!=SWL_ATLAS_HEIGHT)return NULL;
 decompressType type;size_t size=0;
 ssize_t header=decompressHeader(&type,&size,NULL,(void *)(data+prefix),inBytes-prefix);
 if(header<0||!size||size>limit||size>(size_t)SWL_ATLAS_WIDTH*SWL_ATLAS_HEIGHT*4||(size_t)header>inBytes-prefix)return NULL;
 unsigned char *out=malloc(prefix+size+4);if(!out)return NULL;
 memcpy(out,data,prefix);unsigned char *payload=out+prefix;
 payload[0]=DECOMPRESS_DUMMY;payload[1]=(unsigned char)size;payload[2]=(unsigned char)(size>>8);payload[3]=(unsigned char)(size>>16);
 if(type==DECOMPRESS_DUMMY){if((size_t)header+size>inBytes-prefix){free(out);return NULL;}memcpy(payload+4,data+prefix+header,size);}
 else if(!decompress(payload+4,size,NULL,(void *)(data+prefix),inBytes-prefix)){free(out);return NULL;}
 *outBytes=prefix+size+4;return out;
}
MovieBank *movie_bank_load_staged(const char *path,size_t limit,const char **error){
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
 size_t rawTotal=0;
 for(unsigned i=0;i<swl_pages(b->scene);i++){
  b->raw[i]=inflate_page(b->encoded[i],b->encodedBytes[i],limit-rawTotal,&b->rawBytes[i]);
  if(!b->raw[i]){failure="Could not decode the movie textures within budget.";goto fail;}
  rawTotal+=b->rawBytes[i]-21;
 }
 return b;
fail:free(data);movie_bank_free(b);if(error)*error=failure;return NULL;
}
bool movie_bank_commit(MovieBank *b,const char **error){
 if(!b)return false;
 if(!movie_bank_reload_textures(b)){if(error)*error="Could not import the movie textures within budget.";return false;}
 for(unsigned i=0;i<SWL_MAX_PAGES;i++){free(b->raw[i]);b->raw[i]=NULL;b->rawBytes[i]=0;}
 swl_text_handler(b->scene,text_glyphs,b);return true;
}
MovieBank *movie_bank_load(const char *path,size_t limit,const char **error){
 MovieBank *b=movie_bank_load_staged(path,limit,error);
 if(b&&!movie_bank_commit(b,error)){movie_bank_free(b);return NULL;}
 return b;
}
/* Imports texture page i (the next one) into linear memory. One page is up to 2 MiB: importing every page of a
   bank in one frame stalled the Old 3DS for 100-150 ms when a game or mission scene finished loading. */
static bool import_page(MovieBank *b,unsigned i){
 Tex3DS_Texture packed=b->raw[i]?Tex3DS_TextureImport(b->raw[i],b->rawBytes[i],&b->textures[i],NULL,false):Tex3DS_TextureImport(b->encoded[i],b->encodedBytes[i],&b->textures[i],NULL,false);if(!packed)return false;
 b->loaded++;if(Tex3DS_GetNumSubTextures(packed)!=1){Tex3DS_TextureFree(packed);return false;}
 b->subs[i]=*Tex3DS_GetSubTexture(packed,0);Tex3DS_TextureFree(packed);
 b->bytes+=b->textures[i].size;if(b->bytes>b->limit||b->textures[i].width!=SWL_ATLAS_WIDTH||b->textures[i].height!=SWL_ATLAS_HEIGHT||Tex3DS_SubTextureRotated(&b->subs[i])||b->subs[i].width!=SWL_ATLAS_WIDTH||b->subs[i].height!=SWL_ATLAS_HEIGHT)return false;
 C3D_TexSetFilter(&b->textures[i],GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&b->textures[i],GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
 return true;
}
int movie_bank_import_next(MovieBank *b){
 if(!b)return -1;
 unsigned pages=swl_pages(b->scene);if(b->loaded>=pages)return 1;
 if(!import_page(b,b->loaded)){movie_bank_unload_textures(b);return -1;}
 return b->loaded>=pages?1:0;
}
bool movie_bank_reload_textures(MovieBank *b){
 if(!b)return false;
 if(b->loaded==swl_pages(b->scene))return true;
 movie_bank_unload_textures(b);
 for(unsigned i=0;i<swl_pages(b->scene);i++){
  if(!import_page(b,i))goto fail;
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
