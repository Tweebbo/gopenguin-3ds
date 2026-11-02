#include <3ds.h>
#include <citro2d.h>
#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font_layout.h"
#include "game_controller.h"
#include "swl_gpu.h"
#include "ui_theme.h"

static C3D_Tex fontTexture;
static Tex3DS_SubTexture fontSub;
static C2D_TextBuf unicodeGlyphs,unicodeMeasure;
static bool has_unicode(const char *value){for(;*value;value++)if((unsigned char)*value>=128)return true;return false;}
static unsigned row_for(unsigned size){return (size<FONT_MIN?FONT_MIN:size>FONT_MAX?FONT_MAX:size)-FONT_MIN;}
static const FontGlyph *glyph_for(unsigned row,unsigned code){return &fontGlyphs[row][(code>=32&&code<127?code:'?')-32];}
static unsigned text_width(unsigned size,const char *value){if(has_unicode(value)){C2D_Text parsed;float width=0;C2D_TextBufClear(unicodeMeasure);C2D_TextParse(&parsed,unicodeMeasure,value);C2D_TextGetDimensions(&parsed,size/30.0f,size/30.0f,&width,NULL);return (unsigned)ceilf(width);}unsigned width=0,row=row_for(size);for(;*value;value++)width+=glyph_for(row,(unsigned char)*value)->advance;return width;}
static u32 colour(unsigned rgb){return C2D_Color32(rgb>>16&255,rgb>>8&255,rgb&255,255);}
static void native_text(float x,float y,unsigned size,unsigned rgb,const char *value){
 if(has_unicode(value)){C2D_Text parsed;C2D_TextParse(&parsed,unicodeGlyphs,value);C2D_DrawText(&parsed,C2D_WithColor,x,y,.5f,size/30.0f,size/30.0f,colour(rgb));return;}
 unsigned row=row_for(size);int pen=lroundf(x),baseline=lroundf(y)+fontAscents[row];
 for(;*value;value++){
  const FontGlyph *g=glyph_for(row,(unsigned char)*value);
  if(g->w&&g->h){
   Tex3DS_SubTexture sub={.width=g->w,.height=g->h,.left=fontSub.left+(float)g->x/fontTexture.width,.top=fontSub.top-(float)g->y/fontTexture.height,.right=fontSub.left+(float)(g->x+g->w)/fontTexture.width,.bottom=fontSub.top-(float)(g->y+g->h)/fontTexture.height};
   C2D_ImageTint tint;C2D_PlainImageTint(&tint,colour(rgb),1);C2D_DrawImageAt((C2D_Image){&fontTexture,&sub},pen+g->left,baseline+g->top,.5f,&tint,1,1);
  }
  pen+=g->advance;
 }
}
static void draw_text(void *user,float x,float y,float size,uint32_t rgb,GameAlign align,const char *value){
 (void)user;unsigned pixels=(unsigned)fmaxf(FONT_MIN,fminf(FONT_MAX,size));if(align==GAME_ALIGN_CENTER)x-=text_width(pixels,value)/2.0f;else if(align==GAME_ALIGN_RIGHT)x-=text_width(pixels,value);native_text(x,y,pixels,rgb,value);
}
static void draw_rect(void *user,float x,float y,float w,float h,uint32_t rgb){(void)user;ui_game_fill(x,y,w,h,rgb);}
static void draw_button(void *user,float x,float y,float w,float h,const char *label,bool pressed,bool enabled){
 (void)user;float dy=ui_game_button(x,y,w,h,pressed,enabled)?1:0;FontBounds b=font_ink(11,label);native_text(x+w/2-(b.left+b.right)/2.0f,y+h/2+dy-(b.top+b.bottom)/2.0f,11,enabled?0x003366:0x526575,label);
}
static GameResource resolve(void *user,const char *name,char *path,size_t capacity){
 (void)user;if(strncmp(name,"missions/2/",11)&&strncmp(name,"games/",6))return GAME_RESOURCE_FAILED;
 if(strstr(name,"..")||snprintf(path,capacity,"romfs:/%s",name)>=(int)capacity)return GAME_RESOURCE_FAILED;
 FILE *file=fopen(path,"rb");if(!file)return GAME_RESOURCE_FAILED;fclose(file);return GAME_RESOURCE_READY;
}
static bool send_packet(void *user,char extension,const char *command,unsigned count,const char *const *args){(void)user;(void)extension;(void)command;(void)count;(void)args;return true;}
static void leave(void *user,unsigned room,int x,int y){(void)user;(void)room;(void)x;(void)y;}
static bool edit_text(void *user,char *value,size_t capacity,const char *hint){
 (void)user;SwkbdState keyboard;swkbdInit(&keyboard,SWKBD_TYPE_NORMAL,2,(int)capacity-1);swkbdSetHintText(&keyboard,hint);swkbdSetInitialText(&keyboard,value);swkbdSetValidation(&keyboard,SWKBD_NOTEMPTY_NOTBLANK,0,0);return swkbdInputText(&keyboard,value,capacity)==SWKBD_BUTTON_CONFIRM;
}
static GamePort *load_case(json_t *entry){
 const char *key=json_string_value(json_object_get(entry,"game"));if(!key)return NULL;
 GameDescriptor descriptor={.online=true,.player=1,.colour=1,.colourRGB=0x0099ff};snprintf(descriptor.key,sizeof descriptor.key,"%s",key);
 GameHost host={.resource=resolve,.send=send_packet,.leave=leave,.text=draw_text,.rect=draw_rect,.button=draw_button,.edit_text=edit_text};
 GamePort *port=game_port_open(&descriptor,&host);if(!port)return NULL;
 game_port_update(port,0,NULL);char *room[]={!strcmp(key,"mission2")?"908":!strcmp(key,"cart")?"905":"910"};game_port_packet(port,"jg",1,room);
 json_t *snapshot=json_object_get(entry,"snapshot");
 if(snapshot){char *encoded=json_dumps(snapshot,JSON_COMPACT);bool ok=encoded&&game_port_packet(port,"psa",1,&encoded);free(encoded);if(!ok){game_port_free(port);return NULL;}game_port_update(port,0,NULL);}
 else if(json_is_true(json_object_get(entry,"playing"))){
  game_port_update(port,0,&(GameInput){.down=GAME_KEY_A});unsigned ticks=!strcmp(key,"pizzatron")?45:12;
  for(unsigned i=0;i<ticks;i++)game_port_update(port,1.0f/24,NULL);
 }
 unsigned guidePart=(unsigned)json_integer_value(json_object_get(entry,"guidePart"));
 if(guidePart>16){game_port_free(port);return NULL;}
 for(unsigned i=0;i<guidePart;i++)game_port_update(port,0,&(GameInput){.down=GAME_KEY_R});
 game_port_snap(port);if(game_port_session(port)!=GAME_SESSION_PLAYING){game_port_free(port);return NULL;}return port;
}
int main(void){
 gfxInitDefault();romfsInit();C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(8192);C2D_Prepare();C2D_SetTintMode(C2D_TintMult);
 C3D_RenderTarget *targets[]={C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT),C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT)};
 unicodeGlyphs=C2D_TextBufNew(4096);unicodeMeasure=C2D_TextBufNew(256);
 bool ok=unicodeGlyphs&&unicodeMeasure&&swl_gpu_init()&&swl_gpu_reserve(3);
 for(unsigned i=0;i<2;i++){
  void *stencil=vramAlloc(C3D_CalcDepthBufSize(targets[i]->frameBuf.width,targets[i]->frameBuf.height,GPU_RB_DEPTH24_STENCIL8));
  if(!stencil)ok=false;else{if(targets[i]->ownsDepth)vramFree(targets[i]->frameBuf.depthBuf);C3D_FrameBufDepth(&targets[i]->frameBuf,stencil,GPU_RB_DEPTH24_STENCIL8);targets[i]->ownsDepth=true;}
 }
 FILE *file=fopen("romfs:/gfx/font.t3x","rb");Tex3DS_Texture packed=file?Tex3DS_TextureImportStdio(file,&fontTexture,NULL,false):NULL;if(file)fclose(file);
 bool fontLoaded=packed!=NULL;if(packed){fontSub=*Tex3DS_GetSubTexture(packed,0);Tex3DS_TextureFree(packed);C3D_TexSetFilter(&fontTexture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&fontTexture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);}else ok=false;
 json_t *cases=json_load_file("romfs:/cases.json",JSON_REJECT_DUPLICATES,NULL);if(!json_is_array(cases))ok=false;
 GamePort *port=NULL;unsigned selected=0,loaded=~0u,drawn=0;
 while(ok&&aptMainLoop()){
  hidScanInput();u32 down=hidKeysDown();if(down&KEY_START)break;if(down&KEY_A)selected=(selected+1)%json_array_size(cases);
  if(selected!=loaded){C3D_FrameSync();game_port_free(port);port=load_case(json_array_get(cases,selected));loaded=selected;drawn=0;if(!port){ok=false;break;}}
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);swl_gpu_frame();C2D_TextBufClear(unicodeGlyphs);
  C2D_TargetClear(targets[0],colour(0xe6f3fb));C2D_SceneBegin(targets[0]);ok=game_port_draw_top(port,targets[0]);
  C2D_TargetClear(targets[1],colour(0xe6f3fb));C2D_SceneBegin(targets[1]);game_port_draw_lower(port,targets[1]);game_port_draw_bottom(port);
  swl_gpu_commit();C3D_FrameEnd(0);
  if(++drawn==3||!ok){FILE *result=fopen("sdmc:/gopenguin-review.txt","w");if(result){fprintf(result,"%s case=%u textures=%lu\n",ok?"PASS":"FAIL",selected,(unsigned long)game_port_texture_bytes(port));fclose(result);}}
 }
 if(!ok){consoleInit(GFX_BOTTOM,NULL);printf("Native review capture failed.\n");while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gspWaitForVBlank();}}
 C3D_FrameSync();game_port_free(port);json_decref(cases);if(fontLoaded)C3D_TexDelete(&fontTexture);if(unicodeGlyphs)C2D_TextBufDelete(unicodeGlyphs);if(unicodeMeasure)C2D_TextBufDelete(unicodeMeasure);swl_gpu_free();C2D_Fini();C3D_Fini();romfsExit();gfxExit();return ok?0:1;
}
