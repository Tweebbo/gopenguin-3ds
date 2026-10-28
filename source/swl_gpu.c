#include "swl_gpu.h"
#include "swl_shader_shbin.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#ifndef SWL_GPU_FRAME_QUADS
#define SWL_GPU_FRAME_QUADS (SWL_MAX_DRAWS+128)
#endif
enum {VERTICES=6*SWL_GPU_FRAME_QUADS,GROUPS=3};
typedef struct {float x,y,z,u,v;} Vertex;
typedef struct {
 C3D_Tex texture;C3D_RenderTarget *target;
 C3D_RenderTarget *parent;unsigned width,height,masks;bool painting[8];
 SWLColor color;
} Surface;
static struct {
 DVLB_s *dvlb;shaderProgram_s shader;int projection;
 Vertex *vertices;unsigned used,pending,count,width,height,masks,groups,reserved,parentVertices;
 C3D_RenderTarget *target;C3D_Tex *texture;SWLColor color;bool colorSet,painting[8],ready,premultiplied;
 float left,top,right,bottom;Surface surfaces[GROUPS];C3D_Tex white;
 const SWLGPUAtlas *atlases;bool (*depth)(void *,float),(*needed)(void *,float);void *depthUser;
} G;
static const SWLColor identity={{1,1,1,1},{0,0,0,0}};
static void flush(void){if(G.count){GSPGPU_FlushDataCache(G.vertices+G.pending,G.count*sizeof(Vertex));C3D_DrawArrays(GPU_TRIANGLES,G.pending,G.count);G.count=0;}}
static void shader_state(void){
 C3D_BindProgram(&G.shader);C3D_AttrInfo *a=C3D_GetAttrInfo();AttrInfo_Init(a);AttrInfo_AddLoader(a,0,GPU_FLOAT,3);AttrInfo_AddLoader(a,1,GPU_FLOAT,2);
 C3D_BufInfo *b=C3D_GetBufInfo();BufInfo_Init(b);BufInfo_Add(b,G.vertices,sizeof(Vertex),2,0x10);
 C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_ALL);C3D_AlphaTest(true,GPU_GREATER,0);
 C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
 for(unsigned i=0;i<6;i++){C3D_TexEnv *e=C3D_GetTexEnv(i);C3D_TexEnvInit(e);C3D_TexEnvSrc(e,C3D_Both,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(e,C3D_Both,GPU_REPLACE);}
 G.texture=NULL;G.colorSet=false;
}
static void projection(void){
 C3D_Mtx m;
 if(G.groups)Mtx_Ortho(&m,0,512,256,0,0,1,true);
 else Mtx_OrthoTilt(&m,0,G.width,G.height,0,0,1,true);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,G.projection,&m);
 if(G.groups)C3D_SetScissor(GPU_SCISSOR_NORMAL,0,256-G.height,G.width,256);
 else C3D_SetScissor(GPU_SCISSOR_NORMAL,(u32)fmaxf(0,G.height-G.bottom),(u32)fmaxf(0,G.width-G.right),(u32)fminf(G.height,G.height-G.top),(u32)fminf(G.width,G.width-G.left));
}
static void stencil_state(void){
 unsigned read=0,write=0;for(unsigned i=0;i<G.masks;i++){if(G.painting[i])write=1u<<i;else read|=1u<<i;}
 C3D_StencilTest(G.masks!=0,GPU_EQUAL,read|write,read,write);C3D_StencilOp(GPU_STENCIL_KEEP,GPU_STENCIL_KEEP,write?GPU_STENCIL_REPLACE:GPU_STENCIL_KEEP);
 C3D_DepthTest(false,GPU_ALWAYS,write?0:GPU_WRITE_COLOR);C3D_AlphaTest(true,GPU_GREATER,0);
}
static bool set_color(SWLColor c){
 if(G.colorSet&&!memcmp(&c,&G.color,sizeof c))return true;
 float max=1,m[4],select[4],offset[4],constant[4]={0};
 for(unsigned i=0;i<4;i++){
  if(fabsf(c.mult[i])>4||c.add[i]<-255||c.add[i]>255||(c.mult[i]>1&&c.add[i]<0))return false;
  float value=c.mult[i];if(value<0&&c.add[i]<=0)value=0;
  select[i]=value<0?0:1;m[i]=fabsf(value);max=fmaxf(max,m[i]);
  offset[i]=c.add[i]/255-(value<0&&!G.premultiplied?m[i]:0);constant[i]=value<0&&G.premultiplied?m[i]:0;
 }
 float scale=max>2?4:max>1?2:1,mul[4],positive[4],negative[4];
 for(unsigned i=0;i<4;i++){mul[i]=m[i]/scale;positive[i]=fmaxf(0,offset[i]);negative[i]=fmaxf(0,-offset[i]);}
 flush();for(unsigned i=0;i<6;i++){C3D_TexEnv *e=C3D_GetTexEnv(i);C3D_TexEnvInit(e);C3D_TexEnvSrc(e,C3D_Both,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(e,C3D_Both,GPU_REPLACE);}
 C3D_TexEnv *e=C3D_GetTexEnv(0);C3D_TexEnvSrc(e,C3D_Both,GPU_TEXTURE0,GPU_TEXTURE0,GPU_CONSTANT);C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_ONE_MINUS_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR);C3D_TexEnvOpAlpha(e,GPU_TEVOP_A_SRC_ALPHA,GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA,GPU_TEVOP_A_SRC_ALPHA);C3D_TexEnvFunc(e,C3D_Both,GPU_INTERPOLATE);C3D_TexEnvColor(e,C2D_Color32f(select[0],select[1],select[2],select[3]));
 e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_Both,GPU_PREVIOUS,GPU_CONSTANT,0);C3D_TexEnvFunc(e,C3D_Both,GPU_MODULATE);C3D_TexEnvColor(e,C2D_Color32f(mul[0],mul[1],mul[2],mul[3]));C3D_TexEnvScale(e,C3D_Both,scale==4?GPU_TEVSCALE_4:scale==2?GPU_TEVSCALE_2:GPU_TEVSCALE_1);
 e=C3D_GetTexEnv(2);C3D_TexEnvSrc(e,C3D_Both,GPU_PREVIOUS,GPU_CONSTANT,0);C3D_TexEnvFunc(e,C3D_Both,GPU_ADD);C3D_TexEnvColor(e,C2D_Color32f(positive[0],positive[1],positive[2],positive[3]));
 e=C3D_GetTexEnv(3);C3D_TexEnvSrc(e,C3D_Both,GPU_PREVIOUS,GPU_CONSTANT,0);C3D_TexEnvFunc(e,C3D_Both,GPU_SUBTRACT);C3D_TexEnvColor(e,C2D_Color32f(negative[0],negative[1],negative[2],negative[3]));
 if(G.premultiplied){
  if(c.add[3]!=0)return false;
  e=C3D_GetTexEnv(2);C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_CONSTANT,GPU_PREVIOUS);C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_SRC_ALPHA,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR);C3D_TexEnvFunc(e,C3D_RGB,GPU_MULTIPLY_ADD);C3D_TexEnvSrc(e,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(e,C3D_Alpha,GPU_REPLACE);
  e=C3D_GetTexEnv(3);C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_CONSTANT,GPU_PREVIOUS);C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_ONE_MINUS_SRC_ALPHA,GPU_TEVOP_RGB_SRC_COLOR,GPU_TEVOP_RGB_SRC_COLOR);C3D_TexEnvFunc(e,C3D_RGB,GPU_MULTIPLY_ADD);C3D_TexEnvSrc(e,C3D_Alpha,GPU_PREVIOUS,0,0);C3D_TexEnvFunc(e,C3D_Alpha,GPU_REPLACE);
  e=C3D_GetTexEnv(4);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,0);C3D_TexEnvFunc(e,C3D_RGB,GPU_SUBTRACT);C3D_TexEnvColor(e,C2D_Color32f(negative[0]+constant[0],negative[1]+constant[1],negative[2]+constant[2],0));
 }
 G.color=c;G.colorSet=true;return true;
}
static bool quad(C3D_Tex *texture,const SWLMatrix *m,float left,float top,float width,float height,float u0,float v0,float u1,float v1,SWLColor color){
 if(G.used+6>VERTICES||!set_color(color))return false;
 if(G.texture!=texture){flush();C3D_TexBind(0,texture);G.texture=texture;}
 static const unsigned order[]={0,1,2,2,1,3};if(!G.count)G.pending=G.used;
 for(unsigned i=0;i<6;i++){unsigned n=order[i];float x=left+(n&1?width:0),y=top+(n&2?height:0);G.vertices[G.used++]=(Vertex){m->a*x+m->c*y+m->x,m->b*x+m->d*y+m->y,.5f,n&1?u1:u0,n&2?v1:v0};}
 G.count+=6;return true;
}
static bool emit(void *user,const SWLArt *a,const SWLMatrix *m,const SWLColor *c){
 (void)user;const SWLGPUAtlas *atlas=&G.atlases[a->page];C3D_Tex *t=atlas->texture;const Tex3DS_SubTexture *s=atlas->sub;
 return quad(t,m,a->left,a->top,a->width,a->height,s->left+(float)a->x/t->width,s->top-(float)a->y/t->height,s->left+(float)(a->x+a->w)/t->width,s->top-(float)(a->y+a->h)/t->height,*c);
}
static bool mask(void *user,unsigned event){
 (void)user;flush();
 if(event==1){if(G.masks>=8)return false;G.painting[G.masks++]=true;}
 else if(event==2){if(!G.masks)return false;G.painting[G.masks-1]=false;}
 else if(event==3){
  if(!G.masks)return false;
  unsigned bit=1u<<(--G.masks);C3D_StencilTest(true,GPU_ALWAYS,0,0,bit);C3D_StencilOp(GPU_STENCIL_KEEP,GPU_STENCIL_KEEP,GPU_STENCIL_REPLACE);C3D_DepthTest(false,GPU_ALWAYS,0);C3D_AlphaTest(false,GPU_ALWAYS,0);
  if(!quad(&G.white,&(SWLMatrix){1,0,0,1,0,0},0,0,G.width,G.height,0,1,1,0,identity))return false;
  flush();
 }
 stencil_state();return true;
}
static bool color(void *user,unsigned event,const SWLColor *c){
 (void)user;flush();
 if(event==1){
  if(G.groups>=G.reserved)return false;
  Surface *s=&G.surfaces[G.groups++];s->parent=G.target;s->width=G.width;s->height=G.height;s->masks=G.masks;memcpy(s->painting,G.painting,sizeof s->painting);s->color=*c;
  G.target=s->target;G.masks=0;C3D_RenderTargetClear(G.target,C3D_CLEAR_ALL,0,0);C3D_FrameDrawOn(G.target);projection();stencil_state();
 }else if(event==3){
  if(!G.groups)return false;
  Surface *s=&G.surfaces[--G.groups];G.target=s->parent;G.width=s->width;G.height=s->height;G.masks=s->masks;memcpy(G.painting,s->painting,sizeof G.painting);C3D_FrameDrawOn(G.target);projection();stencil_state();
  // Render-target textures hold premultiplied color. Composite with ONE rather
  // than SRC_ALPHA after applying the group's color transform.
  C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
  SWLColor transformed=s->color;for(unsigned k=0;k<3;k++){transformed.mult[k]*=transformed.mult[3];transformed.add[k]*=transformed.mult[3];}
  G.premultiplied=true;G.colorSet=false;
  if(!quad(&s->texture,&(SWLMatrix){1,0,0,1,0,0},0,0,G.width,G.height,0,1,(float)G.width/512,1-(float)G.height/256,transformed))return false;
  flush();
  G.premultiplied=false;G.colorSet=false;
  C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
 }
 return true;
}
static bool depth(void *user,float value){(void)user;if(!G.depth||(G.needed&&!G.needed(G.depthUser,value)))return true;flush();C2D_Prepare();if(!G.groups)C3D_SetScissor(GPU_SCISSOR_NORMAL,(u32)fmaxf(0,G.height-G.bottom),(u32)fmaxf(0,G.width-G.right),(u32)fminf(G.height,G.height-G.top),(u32)fminf(G.width,G.width-G.left));bool ok=G.depth(G.depthUser,value);C2D_Flush();shader_state();projection();stencil_state();return ok;}
bool swl_gpu_init(void){
 memset(&G,0,sizeof G);G.vertices=linearAlloc(sizeof(Vertex)*VERTICES);G.dvlb=DVLB_ParseFile((u32 *)swl_shader_shbin,swl_shader_shbin_size);if(!G.vertices||!G.dvlb)return false;
 shaderProgramInit(&G.shader);shaderProgramSetVsh(&G.shader,&G.dvlb->DVLE[0]);G.projection=shaderInstanceGetUniformLocation(G.shader.vertexShader,"projection");
 if(!C3D_TexInit(&G.white,8,8,GPU_RGBA8))return false;
 memset(G.white.data,255,G.white.size);C3D_TexFlush(&G.white);C3D_TexSetFilter(&G.white,GPU_NEAREST,GPU_NEAREST);G.ready=true;return true;
}
bool swl_gpu_reserve(unsigned groups){
 if(groups>GROUPS)return false;
 C3D_FrameSync();
 while(G.reserved<groups){Surface *s=&G.surfaces[G.reserved];if(!C3D_TexInitVRAM(&s->texture,512,256,GPU_RGBA8))return false;s->target=C3D_RenderTargetCreateFromTex(&s->texture,GPU_TEXFACE_2D,0,GPU_RB_DEPTH24_STENCIL8);if(!s->target){C3D_TexDelete(&s->texture);return false;}C3D_TexSetFilter(&s->texture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&s->texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);G.reserved++;}
 return true;
}
void swl_gpu_free(void){for(unsigned i=0;i<G.reserved;i++){C3D_RenderTargetDelete(G.surfaces[i].target);C3D_TexDelete(&G.surfaces[i].texture);}if(G.ready)C3D_TexDelete(&G.white);if(G.dvlb){shaderProgramFree(&G.shader);DVLB_Free(G.dvlb);}linearFree(G.vertices);memset(&G,0,sizeof G);}
void swl_gpu_frame(void){G.used=0;}
bool swl_gpu_embed(SWLScene *s,SWLMatrix view,const SWLGPUAtlas *atlases){
 if(!G.ready||!swl_prepare(s)||G.used+6*(swl_stats(s).draws+8)+G.parentVertices>VERTICES)return false;
 C2D_Flush();flush();const SWLGPUAtlas *previous=G.atlases;unsigned masks=G.masks,groups=G.groups;G.atlases=atlases;shader_state();projection();stencil_state();
 bool ok=swl_render(s,view,-1,1,&(SWLRenderer){.emit=emit,.mask=mask,.color=color},NULL);flush();G.atlases=previous;ok=ok&&G.masks==masks&&G.groups==groups;C3D_StencilTest(false,GPU_ALWAYS,0,0,0);C2D_Prepare();return ok;
}
bool swl_gpu_draw(SWLScene *s,C3D_RenderTarget *target,unsigned width,unsigned height,SWLMatrix view,int layer,float interpolation,const SWLGPUAtlas *atlases,bool (*callback)(void *,float),bool (*needed)(void *,float),void *user,bool screenClip){
 if(!G.ready||!swl_prepare(s))return false;
 G.parentVertices=6*(swl_stats(s).draws+8);
 C2D_Flush();G.width=width;G.height=height;G.target=target;G.atlases=atlases;G.groups=G.masks=G.count=0;G.depth=callback;G.needed=needed;G.depthUser=user;
 G.left=screenClip?0:fmaxf(0,view.x);G.top=screenClip?0:fmaxf(0,view.y);G.right=screenClip?width:fminf(width,view.x+760*view.a);G.bottom=screenClip?height:fminf(height,view.y+480*view.d);
 C3D_RenderTargetClear(target,C3D_CLEAR_DEPTH,0,0);shader_state();projection();stencil_state();
 // The depth/stencil attachment changes between the two drawing backends.
 // Invalidate the framebuffer cache without splitting the GX command list.
 GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH,1);GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE,1);GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR,1);
 bool ok=swl_render(s,view,layer,interpolation,&(SWLRenderer){.emit=emit,.mask=mask,.color=color,.depth=callback?depth:NULL},NULL);if(ok&&callback)ok=depth(NULL,FLT_MAX);flush();C3D_StencilTest(false,GPU_ALWAYS,0,0,0);C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,0,0);C2D_Prepare();return ok;
}
