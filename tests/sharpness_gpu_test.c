#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include "swl_gpu.h"

int main(void){
 gfxInitDefault();romfsInit();C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(256);C2D_Prepare();consoleInit(GFX_BOTTOM,NULL);
 C3D_RenderTarget *target=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
 void *stencil=vramAlloc(C3D_CalcDepthBufSize(target->frameBuf.width,target->frameBuf.height,GPU_RB_DEPTH24_STENCIL8));
 C3D_FrameBufDepth(&target->frameBuf,stencil,GPU_RB_DEPTH24_STENCIL8);target->ownsDepth=true;
 SWLScene *scene=NULL;C3D_Tex texture={0};Tex3DS_SubTexture sub={0};unsigned selected=0,loaded=99;bool textureLoaded=false;
 bool ok=stencil&&swl_gpu_init()&&swl_gpu_reserve(3);const char *error=NULL;
 while(ok&&aptMainLoop()){
  hidScanInput();u32 down=hidKeysDown();if(down&KEY_START)break;if(down&KEY_A)selected=(selected+1)%4;
  if(selected!=loaded){
   C3D_FrameSync();swl_free(scene);scene=NULL;if(textureLoaded){C3D_TexDelete(&texture);textureLoaded=false;}
   char file[96];const char *variant=selected%2?"after":"before";
   snprintf(file,sizeof file,"romfs:/scenes/%s.swl",variant);scene=swl_load_file(file,&error);
   snprintf(file,sizeof file,"romfs:/gfx/%s.t3x",variant);FILE *f=fopen(file,"rb");Tex3DS_Texture packed=f?Tex3DS_TextureImportStdio(f,&texture,NULL,false):NULL;if(f)fclose(f);
   ok=scene&&packed;if(packed){textureLoaded=true;sub=*Tex3DS_GetSubTexture(packed,0);Tex3DS_TextureFree(packed);C3D_TexSetFilter(&texture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);}
   loaded=selected;consoleClear();printf("Town: %s\nView: %s\n400 x 240, linear filtering\nA: next comparison\nStart: exit\n",variant,selected<2?"overview":"1.2x zoom");
  }
  if(!ok)break;
  float scale=400.0f/760*(selected<2?1:1.2f);
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);swl_gpu_frame();C2D_TargetClear(target,0xffffffff);C2D_SceneBegin(target);
  ok=swl_gpu_draw(scene,target,400,240,(SWLMatrix){scale,0,0,scale,200-380*scale,120-240*scale},-1,1,&(SWLGPUAtlas){&texture,&sub},NULL,NULL,NULL,false);
  swl_gpu_commit();C3D_FrameEnd(0);
  FILE *result=fopen("sdmc:/gopenguin-sharpness.txt","w");if(result){fprintf(result,"%s case=%u\n",ok?"PASS":"FAIL",selected);fclose(result);}
 }
 if(!ok)printf("GPU TEST FAILED\n%s\n",scene?swl_error(scene):error?error:"Texture import failed.");
 C3D_FrameSync();if(textureLoaded)C3D_TexDelete(&texture);swl_free(scene);swl_gpu_free();C2D_Fini();C3D_Fini();romfsExit();gfxExit();return ok?0:1;
}
