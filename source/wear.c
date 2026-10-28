#include "wear.h"
#include "asset_cache.h"
#include "garment.h"
#include "sprite.h"
#include "movie_bank.h"
#include "package.h"
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
enum {SLOTS=128,WANTED=256,BUDGET=8*1024*1024};
typedef struct {unsigned id,page;u64 used;C3D_Tex texture;Tex3DS_SubTexture sub;Garment garment;Sprite *sprite;MovieBank *bank;unsigned action,variation;float clock;bool loaded;} Slot;
typedef struct {unsigned id,rotation,variation;} Request;
static Slot slots[SLOTS];static Request wanted[WANTED];static unsigned count,needed[WANTED],ranks[SLOTS],pages[WANTED];static u64 frame,signature;static size_t bytes;
typedef struct {unsigned id,rotation,variation;MovieBank *bank;float clock;bool failed;} Dynamic;
static Dynamic dynamic[4];static u64 dynamicTime;
static void dynamic_release(Dynamic *d){if(d->bank){bytes-=movie_bank_bytes(d->bank);movie_bank_free(d->bank);}memset(d,0,sizeof *d);}
static size_t slot_bytes(const Slot *slot){return slot->bank?movie_bank_bytes(slot->bank):slot->texture.size;}
static void release(Slot *slot){if(slot->loaded){bytes-=slot_bytes(slot);if(slot->bank)movie_bank_free(slot->bank);else C3D_TexDelete(&slot->texture);garment_free(&slot->garment);sprite_free(slot->sprite);}memset(slot,0,sizeof *slot);}
void wear_free(void){for(unsigned i=0;i<SLOTS;i++)release(&slots[i]);for(unsigned i=0;i<4;i++)dynamic_release(&dynamic[i]);memset(needed,0,sizeof needed);signature=0;dynamicTime=0;}
size_t wear_bytes(void){return bytes;}
void wear_begin(void){count=0;frame++;}
void wear_want_pose(unsigned id,unsigned rotation,unsigned variation){if(!id||count==WANTED)return;for(unsigned i=0;i<count;i++)if(wanted[i].id==id&&wanted[i].rotation==rotation&&wanted[i].variation==variation)return;wanted[count++]=(Request){id,rotation,variation};char path[256];asset_cache_request(id,path,sizeof path);}
void wear_want(unsigned id){wear_want_pose(id,1,1);}
static unsigned page_for(const Slot *slot,unsigned rotation,unsigned variation){if(!slot->sprite)return 0;const SpriteSequence *pose=sprite_pose(slot->sprite,rotation,variation);return pose?pose->page:UINT_MAX;}
static Slot *find(unsigned id,unsigned rotation,unsigned variation){for(unsigned i=0;i<SLOTS;i++){Slot *s=&slots[i];if(s->loaded&&s->id==id&&s->page==page_for(s,rotation,variation))return s;}return NULL;}
static bool fits(unsigned rank,size_t need){size_t available=BUDGET-bytes;bool slot=false;for(unsigned i=0;i<SLOTS;i++)if(!slots[i].loaded)slot=true;else if(ranks[i]>rank){slot=true;available+=slot_bytes(&slots[i]);}return slot&&available>=need;}
static bool dynamic_update(void){
 u64 now=osGetTime();float dt=dynamicTime?fminf(.1f,(now-dynamicTime)/1000.0f):0;dynamicTime=now;
 for(unsigned i=0;i<SLOTS;i++)if(slots[i].bank){Slot *s=&slots[i];SWLScene *scene=movie_bank_scene(s->bank);s->clock+=dt;while(s->clock>=1/swl_rate(scene)){swl_tick(scene);s->clock-=1/swl_rate(scene);}}
 for(unsigned i=0;i<4;i++){Dynamic *d=&dynamic[i];if(!d->id)continue;bool keep=false;for(unsigned n=0;n<count;n++)if(wanted[n].id==d->id&&wanted[n].rotation==d->rotation&&wanted[n].variation==d->variation){keep=true;break;}if(!keep){dynamic_release(d);continue;}if(d->bank){SWLScene *scene=movie_bank_scene(d->bank);d->clock+=dt;while(d->clock>=1/swl_rate(scene)){swl_tick(scene);d->clock-=1/swl_rate(scene);}}}
 for(unsigned n=0;n<count;n++){Request r=wanted[n];Slot *s=find(r.id,r.rotation,r.variation);if(!s||!s->sprite||!s->sprite->nativeTimeline||(r.id>>20)!=ASSET_FURNITURE||(r.id&0xfffff)>=100000)continue;
  Dynamic *slot=NULL;bool exists=false;for(unsigned i=0;i<4;i++){if(!dynamic[i].id)slot=&dynamic[i];else if(dynamic[i].id==r.id&&dynamic[i].rotation==r.rotation&&dynamic[i].variation==r.variation)exists=true;}if(exists||!slot)continue;
  char path[256];if(!asset_cache_request(asset_key(ASSET_MOVIE,300000+(r.id&0xfffff)),path,sizeof path))continue;const size_t limit=2u*1024u*1024u;if(!fits(n,limit))continue;
  while(bytes+limit>BUDGET){unsigned victim=SLOTS;for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&ranks[i]>n&&(victim==SLOTS||ranks[i]>ranks[victim]||(ranks[i]==ranks[victim]&&slots[i].used<slots[victim].used)))victim=i;if(victim==SLOTS)break;release(&slots[victim]);}
  slot->id=r.id;slot->rotation=r.rotation;slot->variation=r.variation;const char *error=NULL;slot->bank=movie_bank_load(path,limit,&error);if(!slot->bank){slot->failed=true;continue;}bytes+=movie_bank_bytes(slot->bank);
  SWLScene *scene=movie_bank_scene(slot->bank);const char *root=swl_frames(scene,"root.item")?"root.item":"root";swl_target(scene,root,r.rotation,NULL,false);char art[SWL_PATH];snprintf(art,sizeof art,"%s.art",root);if(!swl_frames(scene,art))snprintf(art,sizeof art,"root.art_mc");if(swl_frames(scene,art))swl_target(scene,art,r.variation,NULL,false);return true;
 }
 return false;
}
void wear_update(void){
 u64 hash=14695981039346656037ull;for(unsigned n=0;n<count;n++){hash=(hash^wanted[n].id)*1099511628211ull;hash=(hash^wanted[n].rotation)*1099511628211ull;hash=(hash^wanted[n].variation)*1099511628211ull;}if(hash!=signature){signature=hash;memset(needed,0,sizeof needed);}
 for(unsigned n=0;n<count;n++){pages[n]=UINT_MAX;for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&slots[i].id==wanted[n].id){pages[n]=page_for(&slots[i],wanted[n].rotation,wanted[n].variation);break;}}
 for(unsigned i=0;i<SLOTS;i++){ranks[i]=WANTED;for(unsigned n=0;n<count;n++)if(slots[i].loaded&&slots[i].id==wanted[n].id&&slots[i].page==pages[n]){ranks[i]=n;break;}}
 if(dynamic_update())return;
 for(unsigned n=0;n<count;n++){
  Slot *resident=find(wanted[n].id,wanted[n].rotation,wanted[n].variation);if(resident){resident->used=frame;continue;}
  if(needed[n]&&!fits(n,needed[n]))continue;
  char path[256];if(!asset_cache_request(wanted[n].id,path,sizeof path))continue;
  GPPackage package;
  if(package_open(&package,path)){
   const size_t limit=2u*1024u*1024u;
   if(!fits(n,limit)){needed[n]=(unsigned)limit;continue;}
   Slot *available=NULL;
   for(;;){unsigned victim=SLOTS;available=NULL;for(unsigned i=0;i<SLOTS;i++)if(!slots[i].loaded)available=&slots[i];else if(ranks[i]>n&&(victim==SLOTS||ranks[i]>ranks[victim]))victim=i;if(available&&bytes+limit<=BUDGET)break;if(victim==SLOTS)break;release(&slots[victim]);}
   if(!available||bytes+limit>BUDGET)continue;
   const char *error=NULL;MovieBank *bank=movie_bank_load(path,limit,&error);if(!bank){needed[n]=BUDGET+1;continue;}
   available->bank=bank;available->id=wanted[n].id;available->used=frame;available->loaded=true;bytes+=movie_bank_bytes(bank);return;
  }
  Sprite *sprite=NULL;Garment garment={0};unsigned page=0,width=0,height=0;
  if((wanted[n].id>>20)>=ASSET_FURNITURE){for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&slots[i].id==wanted[n].id&&slots[i].sprite){sprite=sprite_retain(slots[i].sprite);break;}if(!sprite)sprite=sprite_load(path);if(!sprite)return;const SpriteSequence *pose=sprite_pose(sprite,wanted[n].rotation,wanted[n].variation);if(!pose){sprite_free(sprite);return;}page=pose->page;width=sprite->textures[page].width;height=sprite->textures[page].height;
  }else{if(!garment_load(&garment,path))return;width=garment.width;height=garment.height;}
  size_t need=(size_t)width*height*4;needed[n]=(unsigned)need;if(!fits(n,need)){garment_free(&garment);sprite_free(sprite);return;}
  Slot *freeSlot=NULL;
  for(;;){freeSlot=NULL;unsigned victim=SLOTS;for(unsigned i=0;i<SLOTS;i++){Slot *s=&slots[i];if(!s->loaded){freeSlot=s;continue;}if(ranks[i]<=n)continue;if(victim==SLOTS||ranks[i]>ranks[victim]||(ranks[i]==ranks[victim]&&s->used<slots[victim].used))victim=i;}if(freeSlot&&bytes+need<=BUDGET)break;if(victim==SLOTS)break;release(&slots[victim]);}
  if(!freeSlot||bytes+need>BUDGET){garment_free(&garment);sprite_free(sprite);return;}
  void *data=NULL;size_t dataBytes=0;if(sprite)data=sprite_texture_read(sprite,page,&dataBytes);else{FILE *file=fopen(path,"rb");if(file){dataBytes=garment.textureBytes;data=malloc(dataBytes);if(!data||fseek(file,(long)garment.textureOffset,SEEK_SET)||fread(data,1,dataBytes,file)!=dataBytes){free(data);data=NULL;}fclose(file);}}
  Tex3DS_Texture packed=data?Tex3DS_TextureImport(data,dataBytes,&freeSlot->texture,NULL,false):NULL;free(data);if(!packed){garment_free(&garment);sprite_free(sprite);return;}
  bool valid=Tex3DS_GetNumSubTextures(packed)==1;if(valid)freeSlot->sub=*Tex3DS_GetSubTexture(packed,0);Tex3DS_TextureFree(packed);
  if(!valid||Tex3DS_SubTextureRotated(&freeSlot->sub)||freeSlot->texture.width!=width||freeSlot->texture.height!=height||freeSlot->texture.size!=need){C3D_TexDelete(&freeSlot->texture);garment_free(&garment);sprite_free(sprite);return;}
  C3D_TexSetFilter(&freeSlot->texture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&freeSlot->texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);freeSlot->garment=garment;freeSlot->sprite=sprite;freeSlot->id=wanted[n].id;freeSlot->page=page;freeSlot->used=frame;freeSlot->loaded=true;bytes+=need;return;
 }
}
static const GarmentFrame *frame_for(const Slot *s,unsigned rotation,unsigned variation,unsigned clock){return s->bank?NULL:s->sprite?sprite_frame(s->sprite,sprite_pose(s->sprite,rotation,variation),clock):garment_frame(&s->garment,rotation,clock);}
static float scale_for(const Slot *s){return s->sprite?s->sprite->scale:s->garment.scale;}
static void draw_frame(Slot *s,const GarmentFrame *f,float x,float y,float scale){if(!f||!f->w||!f->h)return;Tex3DS_SubTexture sub={.width=f->w,.height=f->h,.left=s->sub.left+(float)f->x/s->texture.width,.top=s->sub.top-(float)f->y/s->texture.height,.right=s->sub.left+(float)(f->x+f->w)/s->texture.width,.bottom=s->sub.top-(float)(f->y+f->h)/s->texture.height};C2D_DrawImageAt((C2D_Image){&s->texture,&sub},x+f->left*scale,y+f->top*scale,.5f,NULL,scale/scale_for(s),scale/scale_for(s));}
static void bank_pose(Slot *s,unsigned rotation,unsigned variation){
 if(s->action==rotation&&s->variation==variation)return;
 SWLScene *scene=movie_bank_scene(s->bank);const char *root=swl_frames(scene,"root.item")?"root.item":"root";
 swl_target(scene,root,rotation,NULL,false);char art[SWL_PATH];snprintf(art,sizeof art,"%s.art",root);if(!swl_frames(scene,art))snprintf(art,sizeof art,"root.art_mc");if(swl_frames(scene,art))swl_target(scene,art,variation,NULL,false);
 s->action=rotation;s->variation=variation;s->clock=0;
}
void wear_draw_pose(unsigned id,unsigned rotation,unsigned variation,unsigned clock,float x,float y,float scale){for(unsigned i=0;i<4;i++){Dynamic *d=&dynamic[i];if(d->bank&&d->id==id&&d->rotation==rotation&&d->variation==variation){if(movie_bank_embed(d->bank,(SWLMatrix){scale,0,0,scale,x,y}))return;break;}}Slot *s=find(id,rotation,variation);if(s&&s->bank){bank_pose(s,rotation,variation);movie_bank_embed(s->bank,(SWLMatrix){scale,0,0,scale,x,y});}else if(s)draw_frame(s,frame_for(s,rotation,variation,clock),x,y,scale);}
void wear_draw(unsigned id,unsigned action,unsigned clock,float x,float y,float scale){wear_draw_pose(id,action,1,clock,x,y,scale);}
void wear_fit(unsigned id,float x,float y,float width,float height){Slot *s=find(id,1,1);if(!s)return;if(s->bank){bank_pose(s,1,1);float b[4];if(swl_bounds(movie_bank_scene(s->bank),b)&&b[2]>b[0]&&b[3]>b[1]){float scale=fminf(width/(b[2]-b[0]),height/(b[3]-b[1]));movie_bank_embed(s->bank,(SWLMatrix){scale,0,0,scale,x+(width-(b[2]-b[0])*scale)/2-b[0]*scale,y+(height-(b[3]-b[1])*scale)/2-b[1]*scale});}return;}const GarmentFrame *f=frame_for(s,1,1,0);if(!f||!f->w||!f->h)return;float w=f->w/scale_for(s),h=f->h/scale_for(s),scale=fminf(width/w,height/h);draw_frame(s,f,x+(width-w*scale)/2-f->left*scale,y+(height-h*scale)/2-f->top*scale,scale);}
bool wear_pose_contains(unsigned id,unsigned rotation,unsigned variation,unsigned clock,float x,float y,float scale,float px,float py){Slot *s=find(id,rotation,variation);if(!s)return false;if(s->bank){bank_pose(s,rotation,variation);float b[4];return swl_bounds(movie_bank_scene(s->bank),b)&&px>=x+b[0]*scale&&py>=y+b[1]*scale&&px<x+b[2]*scale&&py<y+b[3]*scale;}const GarmentFrame *f=frame_for(s,rotation,variation,clock);return f&&f->w&&f->h&&px>=x+f->left*scale&&py>=y+f->top*scale&&px<x+(f->left+f->w/scale_for(s))*scale&&py<y+(f->top+f->h/scale_for(s))*scale;}
bool wear_contains(unsigned id,float x,float y,float scale,float px,float py){return wear_pose_contains(id,1,1,0,x,y,scale,px,py);}
bool wear_pose_info(unsigned id,unsigned rotation,unsigned *rotations,unsigned *variations){for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&slots[i].id==id){Slot *s=&slots[i];if(s->bank){SWLScene *scene=movie_bank_scene(s->bank);bank_pose(s,rotation,1);const char *root=swl_frames(scene,"root.item")?"root.item":"root";char art[SWL_PATH];snprintf(art,sizeof art,"%s.art",root);if(!swl_frames(scene,art))snprintf(art,sizeof art,"root.art_mc");if(rotations)*rotations=swl_frames(scene,root);if(variations){*variations=swl_frames(scene,art);if(!*variations)*variations=1;}return true;}if(s->sprite){if(rotations)*rotations=sprite_rotations(s->sprite);if(variations)*variations=sprite_variations(s->sprite,rotation);return true;}}return false;}
bool wear_pose_view(unsigned id,unsigned rotation,unsigned variation,unsigned clock,SWLGPUAtlas *atlas,SWLArt *art){Slot *s=find(id,rotation,variation);if(!s)return false;const GarmentFrame *f=frame_for(s,rotation,variation,clock);if(!f)return false;*atlas=(SWLGPUAtlas){&s->texture,&s->sub};*art=(SWLArt){.id=id,.x=f->x,.y=f->y,.w=f->w,.h=f->h,.left=f->left,.top=f->top,.width=f->w/scale_for(s),.height=f->h/scale_for(s)};return true;}
