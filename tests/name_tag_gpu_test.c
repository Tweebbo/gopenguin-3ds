#include <3ds.h>
#include <citro2d.h>
#include <tex3ds.h>
#include <stdio.h>
#include <string.h>
#include "name_tag.h"
#include "font.h"
static void draw(C3D_Tex *texture,unsigned sx,unsigned sy,unsigned w,unsigned h,int x,int y){
 Tex3DS_SubTexture sub={.width=w,.height=h,.left=(float)sx/texture->width,.right=(float)(sx+w)/texture->width,.top=1-(float)sy/texture->height,.bottom=1-(float)(sy+h)/texture->height};
 C2D_ImageTint tint;C2D_PlainImageTint(&tint,C2D_Color32(0,51,102,255),1);C2D_DrawImageAt((C2D_Image){texture,&sub},x,y,.5f,&tint,1,1);
}
int main(void){
 gfxInitDefault();romfsInit();C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(1024);C2D_Prepare();C2D_SetTintMode(C2D_TintMult);
 C3D_RenderTarget *screen=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);C3D_Tex font,cache;
 FILE *f=fopen("romfs:/gfx/font.t3x","rb");Tex3DS_Texture packed=f?Tex3DS_TextureImportStdio(f,&font,NULL,false):NULL;if(f)fclose(f);
 bool ok=packed&&font.fmt==GPU_LA8&&C3D_TexInit(&cache,NAME_TAG_W,NAME_TAG_H,GPU_LA8);
 const char *names[]={"Penguin","Captain Rockhopper","Gold0","jj_WWgy","!$@[]{}~","0123456789"};NameTag tags[6];
 if(ok){C3D_TexSetFilter(&font,GPU_NEAREST,GPU_NEAREST);C3D_TexSetFilter(&cache,GPU_NEAREST,GPU_NEAREST);memset(cache.data,0,cache.size);for(unsigned i=0;i<6;i++){ok=name_tag_layout(&tags[i],names[i]);if(!ok)break;name_tag_write(cache.data,i,&tags[i],names[i],font.data,font.width,font.height);}C3D_TexFlush(&cache);}
 if(!ok)consoleInit(GFX_BOTTOM,NULL);
 while(aptMainLoop()){
  hidScanInput();if(hidKeysDown()&KEY_START)break;
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);C2D_TargetClear(screen,C2D_Color32(255,255,255,255));C2D_SceneBegin(screen);
  if(ok&&(hidKeysDown()&KEY_A)){
   names[0]="Tweeb";names[1]="Gold";
   for(unsigned i=0;i<2;i++){name_tag_layout(&tags[i],names[i]);name_tag_write(cache.data,i,&tags[i],names[i],font.data,font.width,font.height);}C3D_TexFlush(&cache);
  }
  if(ok)for(unsigned i=0;i<6;i++){
   int pen=12,y=20+i*32;
   for(const char *s=names[i];*s;s++){const FontGlyph *g=&fontGlyphs[0][*s-32];if(g->w&&g->h)draw(&font,g->x,g->y,g->w,g->h,pen+g->left,y+fontAscents[0]+g->top);pen+=g->advance;}
   draw(&cache,i%NAME_TAG_COLUMNS*NAME_TAG_SLOT_W,i/NAME_TAG_COLUMNS*NAME_TAG_SLOT_H,tags[i].width,tags[i].height,212+tags[i].left,y+tags[i].top);
  }
  C3D_FrameEnd(0);
 }
 if(packed){Tex3DS_TextureFree(packed);C3D_TexDelete(&font);}if(ok)C3D_TexDelete(&cache);C2D_Fini();C3D_Fini();romfsExit();gfxExit();return ok?0:1;
}
