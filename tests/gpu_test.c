#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include "swl_gpu.h"
typedef struct {SWLScene *scene;SWLGPUAtlas atlas;bool done,rejected;} Budget;
static bool budget_needed(void *user,float depth){(void)depth;return !((Budget *)user)->done;}
static bool budget_fill(void *user,float depth){
 (void)depth;Budget *b=user;b->done=true;unsigned accepted=0;
 for(unsigned i=0;i<128;i++){if(!swl_gpu_embed(b->scene,(SWLMatrix){0,0,0,0,0,0},&b->atlas)){b->rejected=accepted>0&&!*swl_error(b->scene);return b->rejected;}accepted++;}
 return false;
}
int main(void){
 gfxInitDefault();romfsInit();C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(256);C2D_Prepare();
 C3D_RenderTarget *target=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
 void *stencil=vramAlloc(C3D_CalcDepthBufSize(target->frameBuf.width,target->frameBuf.height,GPU_RB_DEPTH24_STENCIL8));
 C3D_FrameBufDepth(&target->frameBuf,stencil,GPU_RB_DEPTH24_STENCIL8);target->ownsDepth=true;
 const char *error=NULL;SWLScene *scene=swl_load_file("romfs:/scenes/graphics.swl",&error);C3D_Tex texture;Tex3DS_SubTexture sub;
 FILE *f=fopen("romfs:/gfx/swl-graphics-0.t3x","rb");Tex3DS_Texture t=f?Tex3DS_TextureImportStdio(f,&texture,NULL,false):NULL;if(f)fclose(f);
 unsigned frames=0;bool ok=scene&&t&&swl_gpu_init()&&swl_gpu_reserve(3)&&stencil;
 if(t){sub=*Tex3DS_GetSubTexture(t,0);Tex3DS_TextureFree(t);C3D_TexSetFilter(&texture,GPU_LINEAR,GPU_LINEAR);}
 Budget budget={.scene=scene,.atlas={&texture,&sub}};
 if(ok)while(aptMainLoop()){
  hidScanInput();if(hidKeysDown()&KEY_START)break;
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);swl_gpu_frame();C2D_TargetClear(target,0xffffffff);C2D_SceneBegin(target);
  ok=swl_gpu_draw(scene,target,400,240,(SWLMatrix){.5f,0,0,.5f,10,0},-1,1,&(SWLGPUAtlas){&texture,&sub},budget_fill,budget_needed,&budget,false)&&budget.rejected;
  swl_gpu_commit();C3D_FrameEnd(0);if(frames++==5||!ok){FILE *result=fopen("sdmc:/gopenguin-gpu-result.txt","w");if(result){fprintf(result,"%s\n",ok?"PASS masks, color groups and vertex budget":"FAIL GPU rendering");fclose(result);}}if(!ok)break;
 }
 if(!ok){consoleInit(GFX_BOTTOM,NULL);printf("GPU TEST FAILED\n%s\n",scene?swl_error(scene):error);while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gspWaitForVBlank();}}
 if(t)C3D_TexDelete(&texture);
 swl_free(scene);swl_gpu_free();C2D_Fini();C3D_Fini();romfsExit();gfxExit();return ok?0:1;
}
