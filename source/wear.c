#include "wear.h"
#include "wear_index.h"
#include "asset_cache.h"
#include "garment.h"
#include "sprite.h"
#include "movie_bank.h"
#include "package.h"
#include "wear_loader.h"
#include "wear_budget.h"
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
enum {SLOTS=128,WANTED=256};
typedef struct {unsigned id,page;u64 used;C3D_Tex texture;Tex3DS_SubTexture sub;Garment garment;Sprite *sprite;MovieBank *bank;unsigned action,variation;float clock;bool loaded;} Slot;
typedef struct {unsigned id,rotation,variation;} Request;
static Slot slots[SLOTS];static Request wanted[WANTED];static unsigned count,needed[WANTED],ranks[SLOTS],pages[WANTED];static u64 frame,signature;static size_t bytes;
static size_t budget;
static bool trimUnwanted;
static u64 retryAt[WANTED];
static WearIndex residentIndex;static bool indexDirty=true;
static uint16_t wantedIndex[512];
_Static_assert(SLOTS==WEAR_INDEX_SLOTS,"Wear index must cover every resident texture");
static void index_ready(void){if(indexDirty){unsigned ids[SLOTS];for(unsigned i=0;i<SLOTS;i++)ids[i]=slots[i].loaded?slots[i].id:0;wear_index_build(&residentIndex,ids);indexDirty=false;}}
typedef struct {unsigned id,rotation,variation;MovieBank *bank;float clock;bool failed;} Dynamic;
static Dynamic dynamic[4];static u64 dynamicTime;
static void dynamic_release(Dynamic *d){if(d->bank){bytes-=movie_bank_bytes(d->bank);movie_bank_free(d->bank);}memset(d,0,sizeof *d);}
static size_t slot_bytes(const Slot *slot){return slot->bank?movie_bank_bytes(slot->bank):slot->texture.size;}
static void release(Slot *slot){if(slot->loaded){indexDirty=true;bytes-=slot_bytes(slot);if(slot->bank)movie_bank_free(slot->bank);else C3D_TexDelete(&slot->texture);garment_free(&slot->garment);sprite_free(slot->sprite);}memset(slot,0,sizeof *slot);}
void wear_free(void){wear_loader_cancel();for(unsigned i=0;i<SLOTS;i++)release(&slots[i]);for(unsigned i=0;i<4;i++)dynamic_release(&dynamic[i]);memset(needed,0,sizeof needed);memset(retryAt,0,sizeof retryAt);signature=0;dynamicTime=0;budget=0;}
size_t wear_bytes(void){return bytes;}
size_t wear_budget(void){return budget;}
void wear_trim_unwanted(void){trimUnwanted=true;}
void wear_request_stop(void){wear_loader_request_stop();}
bool wear_stop_within(unsigned ms){return wear_loader_stop_within(ms);}
void wear_begin(void){count=0;frame++;memset(wantedIndex,0,sizeof wantedIndex);}
void wear_want_pose(unsigned id,unsigned rotation,unsigned variation){if(!id||count==WANTED)return;unsigned hash=(id*2654435761u^rotation*2246822519u^variation*3266489917u)&511;while(wantedIndex[hash]){Request *r=&wanted[wantedIndex[hash]-1];if(r->id==id&&r->rotation==rotation&&r->variation==variation)return;hash=(hash+1)&511;}wantedIndex[hash]=(uint16_t)(count+1);wanted[count++]=(Request){id,rotation,variation};}
void wear_want(unsigned id){wear_want_pose(id,1,1);}
static unsigned page_for(const Slot *slot,unsigned rotation,unsigned variation){if(!slot->sprite)return 0;const SpriteSequence *pose=sprite_pose(slot->sprite,rotation,variation);return pose?pose->page:UINT_MAX;}
static Slot *find(unsigned id,unsigned rotation,unsigned variation){index_ready();for(int i=wear_index_first(&residentIndex,id);i>=0;i=wear_index_next(&residentIndex,(unsigned)i)){Slot *s=&slots[i];if(s->page==page_for(s,rotation,variation))return s;}return NULL;}
static bool fits(unsigned rank,size_t need){size_t retained=bytes;bool slot=false;for(unsigned i=0;i<SLOTS;i++)if(!slots[i].loaded)slot=true;else if(ranks[i]>rank){slot=true;retained-=slot_bytes(&slots[i]);}return slot&&retained<=budget&&need<=budget-retained;}
static bool dynamic_update(void){
 u64 now=osGetTime();float dt=dynamicTime?fminf(.1f,(now-dynamicTime)/1000.0f):0;dynamicTime=now;
 for(unsigned i=0;i<SLOTS;i++)if(slots[i].bank){Slot *s=&slots[i];SWLScene *scene=movie_bank_scene(s->bank);s->clock+=dt;while(s->clock>=1/swl_rate(scene)){swl_tick(scene);s->clock-=1/swl_rate(scene);}}
 for(unsigned i=0;i<4;i++){Dynamic *d=&dynamic[i];if(!d->id)continue;bool keep=false;for(unsigned n=0;n<count;n++)if(wanted[n].id==d->id&&wanted[n].rotation==d->rotation&&wanted[n].variation==d->variation){keep=true;break;}if(!keep){dynamic_release(d);continue;}if(d->bank){SWLScene *scene=movie_bank_scene(d->bank);d->clock+=dt;while(d->clock>=1/swl_rate(scene)){swl_tick(scene);d->clock-=1/swl_rate(scene);}}}
 for(unsigned n=0;n<count;n++){Request r=wanted[n];Slot *s=find(r.id,r.rotation,r.variation);if(!s||!s->sprite||!s->sprite->nativeTimeline||(r.id>>20)!=ASSET_FURNITURE||(r.id&0xfffff)>=100000)continue;
  Dynamic *slot=NULL;bool exists=false;for(unsigned i=0;i<4;i++){if(!dynamic[i].id)slot=&dynamic[i];else if(dynamic[i].id==r.id&&dynamic[i].rotation==r.rotation&&dynamic[i].variation==r.variation)exists=true;}if(exists||!slot)continue;
  char path[256];if(!asset_cache_request(asset_key(ASSET_MOVIE,300000+(r.id&0xfffff)),path,sizeof path))continue;const size_t limit=2u*1024u*1024u;if(!fits(n,limit))continue;
  while(bytes+limit>budget){unsigned victim=SLOTS;for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&ranks[i]>n&&(victim==SLOTS||ranks[i]>ranks[victim]||(ranks[i]==ranks[victim]&&slots[i].used<slots[victim].used)))victim=i;if(victim==SLOTS)break;release(&slots[victim]);}
  slot->id=r.id;slot->rotation=r.rotation;slot->variation=r.variation;const char *error=NULL;slot->bank=movie_bank_load(path,limit,&error);if(!slot->bank){slot->failed=true;continue;}bytes+=movie_bank_bytes(slot->bank);
  SWLScene *scene=movie_bank_scene(slot->bank);const char *root=swl_frames(scene,"root.item")?"root.item":"root";swl_target(scene,root,r.rotation,NULL,false);char art[SWL_PATH];snprintf(art,sizeof art,"%s.art",root);if(!swl_frames(scene,art))snprintf(art,sizeof art,"root.art_mc");if(swl_frames(scene,art))swl_target(scene,art,r.variation,NULL,false);return true;
 }
 return false;
}
static unsigned victim_for(unsigned rank){
 unsigned victim=SLOTS;
 for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&ranks[i]>rank&&(victim==SLOTS||ranks[i]>ranks[victim]||(ranks[i]==ranks[victim]&&slots[i].used<slots[victim].used)))victim=i;
 return victim;
}
static Slot *make_room(unsigned rank,size_t need){
 if(!fits(rank,need))return NULL;
 for(;;){
  Slot *empty=NULL;for(unsigned i=0;i<SLOTS;i++)if(!slots[i].loaded){empty=&slots[i];break;}
  if(empty&&bytes<=budget-need)return empty;
  unsigned victim=victim_for(rank);if(victim==SLOTS)return NULL;release(&slots[victim]);
 }
}
static void publish(WearLoadResult *result){
 unsigned rank=WANTED;
 for(unsigned n=0;n<count;n++)if(wanted[n].id==result->request.id){
  const SpriteSequence *pose=result->sprite?sprite_pose(result->sprite,wanted[n].rotation,wanted[n].variation):NULL;
  if(!result->sprite||(pose&&pose->page==result->page)){rank=n;break;}
 }
 if(rank==WANTED||find(wanted[rank].id,wanted[rank].rotation,wanted[rank].variation))return;
 if(result->status==WEAR_LOAD_WAITING){retryAt[rank]=now_ms()+50;return;}
 if(result->status==WEAR_LOAD_FAILED){retryAt[rank]=now_ms()+5000;return;}
 size_t need=result->status==WEAR_LOAD_MOVIE?2u*1024u*1024u:result->texture.bytes;needed[rank]=(unsigned)need;
 Slot *slot=make_room(rank,need);if(!slot)return;
 if(result->status==WEAR_LOAD_MOVIE){
  const char *error=NULL;slot->bank=movie_bank_load(result->path,need,&error);
  if(!slot->bank){retryAt[rank]=now_ms()+5000;return;}
 }else{
  WearTexture *t=&result->texture;
  if(!C3D_TexInit(&slot->texture,t->width,t->height,GPU_RGBA8)){retryAt[rank]=now_ms()+1000;return;}
  if(slot->texture.size!=need){C3D_TexDelete(&slot->texture);retryAt[rank]=now_ms()+5000;return;}
  memcpy(slot->texture.data,t->pixels,need);C3D_TexFlush(&slot->texture);
  slot->sub=(Tex3DS_SubTexture){t->sub.width,t->sub.height,t->sub.left,t->sub.top,t->sub.right,t->sub.bottom};
  C3D_TexSetFilter(&slot->texture,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&slot->texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
  slot->garment=result->garment;memset(&result->garment,0,sizeof result->garment);slot->sprite=result->sprite;result->sprite=NULL;
 }
 slot->id=result->request.id;slot->page=result->page;slot->used=frame;slot->loaded=true;indexDirty=true;bytes+=slot_bytes(slot);
}
void wear_update(void){
 budget=wear_budget_limit(bytes,linearSpaceFree());
 u64 hash=14695981039346656037ull;for(unsigned n=0;n<count;n++){hash=(hash^wanted[n].id)*1099511628211ull;hash=(hash^wanted[n].rotation)*1099511628211ull;hash=(hash^wanted[n].variation)*1099511628211ull;}if(hash!=signature){signature=hash;memset(needed,0,sizeof needed);memset(retryAt,0,sizeof retryAt);}
 index_ready();for(unsigned i=0;i<SLOTS;i++)ranks[i]=WANTED;
 for(unsigned n=0;n<count;n++){int first=wear_index_first(&residentIndex,wanted[n].id);pages[n]=first>=0?page_for(&slots[first],wanted[n].rotation,wanted[n].variation):UINT_MAX;for(int i=first;i>=0;i=wear_index_next(&residentIndex,(unsigned)i))if(slots[i].page==pages[n]&&ranks[i]>n){ranks[i]=n;slots[i].used=frame;}}
 if(trimUnwanted){wear_loader_cancel();for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&ranks[i]==WANTED)release(&slots[i]);trimUnwanted=false;}
 while(bytes>budget){
  unsigned victim=victim_for(0);
  if(victim==SLOTS)for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded){victim=i;break;}
  if(victim!=SLOTS){release(&slots[victim]);continue;}
  unsigned d=4;while(d&&!dynamic[d-1].bank)d--;
  if(!d)break;
  dynamic_release(&dynamic[d-1]);
 }
 if(dynamic_update())return;
 WearLoadResult result={0};
 if(wear_loader_take(&result)){publish(&result);wear_loader_result_free(&result);}
 if(wear_loader_busy())return;
 for(unsigned n=0;n<count;n++){
  if(find(wanted[n].id,wanted[n].rotation,wanted[n].variation)||now_ms()<retryAt[n]||(needed[n]&&!fits(n,needed[n])))continue;
  WearLoadRequest request={wanted[n].id,wanted[n].rotation,wanted[n].variation};
  if(wear_loader_start())wear_loader_request(request);
  else{wear_loader_prepare(&result,request,NULL,NULL);publish(&result);wear_loader_result_free(&result);}
  return;
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
bool wear_pose_ready(unsigned id,unsigned rotation,unsigned variation){return find(id,rotation,variation)!=NULL;}
void wear_fit(unsigned id,float x,float y,float width,float height){Slot *s=find(id,1,1);if(!s)return;if(s->bank){bank_pose(s,1,1);float b[4];if(swl_bounds(movie_bank_scene(s->bank),b)&&b[2]>b[0]&&b[3]>b[1]){float scale=fminf(width/(b[2]-b[0]),height/(b[3]-b[1]));movie_bank_embed(s->bank,(SWLMatrix){scale,0,0,scale,x+(width-(b[2]-b[0])*scale)/2-b[0]*scale,y+(height-(b[3]-b[1])*scale)/2-b[1]*scale});}return;}const GarmentFrame *f=frame_for(s,1,1,0);if(!f||!f->w||!f->h)return;float w=f->w/scale_for(s),h=f->h/scale_for(s),scale=fminf(width/w,height/h);draw_frame(s,f,x+(width-w*scale)/2-f->left*scale,y+(height-h*scale)/2-f->top*scale,scale);}
bool wear_pose_contains(unsigned id,unsigned rotation,unsigned variation,unsigned clock,float x,float y,float scale,float px,float py){Slot *s=find(id,rotation,variation);if(!s)return false;if(s->bank){bank_pose(s,rotation,variation);float b[4];return swl_bounds(movie_bank_scene(s->bank),b)&&px>=x+b[0]*scale&&py>=y+b[1]*scale&&px<x+b[2]*scale&&py<y+b[3]*scale;}const GarmentFrame *f=frame_for(s,rotation,variation,clock);return f&&f->w&&f->h&&px>=x+f->left*scale&&py>=y+f->top*scale&&px<x+(f->left+f->w/scale_for(s))*scale&&py<y+(f->top+f->h/scale_for(s))*scale;}
bool wear_contains(unsigned id,float x,float y,float scale,float px,float py){return wear_pose_contains(id,1,1,0,x,y,scale,px,py);}
bool wear_pose_info(unsigned id,unsigned rotation,unsigned *rotations,unsigned *variations){for(unsigned i=0;i<SLOTS;i++)if(slots[i].loaded&&slots[i].id==id){Slot *s=&slots[i];if(s->bank){SWLScene *scene=movie_bank_scene(s->bank);bank_pose(s,rotation,1);const char *root=swl_frames(scene,"root.item")?"root.item":"root";char art[SWL_PATH];snprintf(art,sizeof art,"%s.art",root);if(!swl_frames(scene,art))snprintf(art,sizeof art,"root.art_mc");if(rotations)*rotations=swl_frames(scene,root);if(variations){*variations=swl_frames(scene,art);if(!*variations)*variations=1;}return true;}if(s->sprite){if(rotations)*rotations=sprite_rotations(s->sprite);if(variations)*variations=sprite_variations(s->sprite,rotation);return true;}}return false;}
bool wear_pose_view(unsigned id,unsigned rotation,unsigned variation,unsigned clock,SWLGPUAtlas *atlas,SWLArt *art){Slot *s=find(id,rotation,variation);if(!s)return false;const GarmentFrame *f=frame_for(s,rotation,variation,clock);if(!f)return false;*atlas=(SWLGPUAtlas){&s->texture,&s->sub};*art=(SWLArt){.id=id,.x=f->x,.y=f->y,.w=f->w,.h=f->h,.left=f->left,.top=f->top,.width=f->w/scale_for(s),.height=f->h/scale_for(s)};return true;}
