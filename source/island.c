#include "island.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {MASK_BYTES=45600,MAX_NAV=4*1024*1024};
struct IslandNav {uint8_t *data;const uint8_t *triggers,*buttons,*hovers,*events,*strings,*masks;uint32_t id,nTriggers,nButtons,nHovers,nEvents,stringBytes,nMasks;float x,y;};
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float f32(const uint8_t *p){uint32_t n=u32(p);float f;memcpy(&f,&n,4);return f;}
static bool string_valid(IslandNav *n,unsigned off){return off<n->stringBytes&&memchr(n->strings+off,0,n->stringBytes-off);}
static const char *string_at(const IslandNav *n,unsigned off){return (const char *)n->strings+off;}
const IslandRoom *island_room(unsigned id){for(unsigned i=0;i<ISLAND_ROOMS;i++)if(islandRooms[i].id==id)return &islandRooms[i];return NULL;}
const IslandRoom *island_key(const char *key){for(unsigned i=0;i<ISLAND_ROOMS;i++)if(!strcmp(key,islandRooms[i].key))return &islandRooms[i];return NULL;}
static IslandNav *load_owned(uint8_t *data,size_t bytes,const char **error){
 if(error)*error=NULL;
 if(!data||bytes<48||bytes>MAX_NAV){free(data);if(error)*error="Invalid room navigation size";return NULL;}
 IslandNav *n=calloc(1,sizeof *n);if(!n){free(data);return NULL;}n->data=data;const uint8_t *p=data;
 unsigned version=u32(p+4);if(!((version==1&&!memcmp(p,"GPN1",4))||(version==2&&!memcmp(p,"GPN2",4)))||u32(p+8)!=(size_t)bytes||(version==1&&u32(p+44)))goto bad;
 n->nEvents=version==2?u32(p+44):0;if(n->nEvents>1024)goto bad;
 n->id=u32(p+12);n->x=f32(p+16);n->y=f32(p+20);n->nTriggers=u32(p+24);n->nButtons=u32(p+28);n->nHovers=u32(p+32);n->stringBytes=u32(p+36);n->nMasks=u32(p+40);
 if(n->nTriggers>128||n->nButtons>128||n->nHovers>512||!n->nMasks||n->nMasks>257||!n->stringBytes||n->stringBytes>65536||!isfinite(n->x)||!isfinite(n->y)||n->x<0||n->x>=760||n->y<0||n->y>=480)goto bad;
 size_t expected=48+n->nTriggers*36+n->nButtons*32+n->nHovers*16+n->nEvents*32+n->stringBytes+n->nMasks*MASK_BYTES;if(expected!=(size_t)bytes)goto bad;
 n->triggers=p+48;n->buttons=n->triggers+n->nTriggers*36;n->hovers=n->buttons+n->nButtons*32;n->events=n->hovers+n->nHovers*16;n->strings=n->events+n->nEvents*32;n->masks=n->strings+n->stringBytes;if(n->strings[0]||n->strings[n->stringBytes-1])goto bad;
 for(unsigned i=0;i<n->nTriggers;i++){const uint8_t *r=n->triggers+i*36;if(!isfinite(f32(r+4))||!isfinite(f32(r+8))||!string_valid(n,u32(r+16))||!string_valid(n,u32(r+20))||u32(r+32)>=n->nMasks)goto bad;}
 for(unsigned i=0;i<n->nButtons;i++){const uint8_t *r=n->buttons+i*32;if(!string_valid(n,u32(r))||!string_valid(n,u32(r+4))||!isfinite(f32(r+8))||!isfinite(f32(r+12))||u32(r+16)>n->nHovers||u32(r+20)>n->nHovers-u32(r+16)||u32(r+24)>=n->nMasks||u32(r+28))goto bad;}
 for(unsigned i=0;i<n->nHovers;i++)if(!string_valid(n,u32(n->hovers+i*16)))goto bad;
 for(unsigned i=0;i<n->nEvents;i++){const uint8_t *e=n->events+i*32;if(u32(e)<1||u32(e)>2||(u32(e)==2&&u32(e+4)>=n->nButtons)||!string_valid(n,u32(e+8))||!string_valid(n,u32(e+20))||!u32(e+24)||u32(e+28)>1)goto bad;}
 return n;
bad:if(error)*error="Invalid room navigation";island_free(n);return NULL;
}
IslandNav *island_load_memory(const void *bytes,size_t length,const char **error){if(!bytes||length<48||length>MAX_NAV){if(error)*error="Invalid room navigation size";return NULL;}uint8_t *copy=malloc(length);if(!copy)return NULL;memcpy(copy,bytes,length);return load_owned(copy,length,error);}
IslandNav *island_load(const char *path,const char **error){
 if(error)*error=NULL;
 FILE *file=fopen(path,"rb");if(!file){if(error)*error="Could not open room navigation";return NULL;}
 if(fseek(file,0,SEEK_END)){fclose(file);return NULL;}
 long length=ftell(file);if(length<48||length>MAX_NAV||fseek(file,0,SEEK_SET)){fclose(file);return NULL;}
 uint8_t *data=malloc((size_t)length);if(!data){fclose(file);return NULL;}
 bool ok=fread(data,1,(size_t)length,file)==(size_t)length;fclose(file);if(!ok){free(data);return NULL;}return load_owned(data,(size_t)length,error);
}
void island_set_id(IslandNav *n,unsigned id){if(n)n->id=id;}
void island_free(IslandNav *n){if(n){free(n->data);free(n);}}
unsigned island_id(const IslandNav *n){return n->id;}
void island_start(const IslandNav *n,float *x,float *y){*x=n->x;*y=n->y;}
static bool hit(const IslandNav *n,unsigned mask,int x,int y){if(x<0||x>=760||y<0||y>=480)return false;unsigned bit=y*760+x;return (n->masks[(size_t)mask*MASK_BYTES+(bit>>3)]>>(bit&7))&1;}
bool island_blocked(const void *nav,int x,int y){return hit(nav,0,x,y);}
bool island_valid(const IslandNav *n,float x,float y){return isfinite(x)&&isfinite(y)&&x>=20&&x<=740&&y>=20&&y<=430&&!hit(n,0,(int)lroundf(x),(int)lroundf(y));}
bool island_trigger(const IslandNav *n,int x,int y,IslandTrigger *t){for(unsigned i=0;i<n->nTriggers;i++){const uint8_t *r=n->triggers+i*36;if(hit(n,u32(r+32),x,y)){*t=(IslandTrigger){u32(r),u32(r+12),u32(r+24),u32(r+28),f32(r+4),f32(r+8),string_at(n,u32(r+16)),string_at(n,u32(r+20)),i};return true;}}return false;}
bool island_button(const IslandNav *n,int x,int y,IslandButton *b){for(unsigned i=0;i<n->nButtons;i++){const uint8_t *r=n->buttons+i*32;if(hit(n,u32(r+24),x,y)){*b=(IslandButton){string_at(n,u32(r)),string_at(n,u32(r+4)),f32(r+8),f32(r+12),u32(r+16),u32(r+20),i};return true;}}return false;}
IslandHover island_hover(const IslandNav *n,unsigned index){if(index>=n->nHovers)return (IslandHover){0};const uint8_t *r=n->hovers+index*16;return (IslandHover){string_at(n,u32(r)),u32(r+4),u32(r+8),u32(r+12)!=0};}
unsigned island_events(const IslandNav *n){return n->nEvents;}
IslandEvent island_event(const IslandNav *n,unsigned index){if(index>=n->nEvents)return (IslandEvent){0};const uint8_t *e=n->events+index*32;return (IslandEvent){u32(e),u32(e+4),u32(e+12),u32(e+16),u32(e+24),u32(e+28)!=0,string_at(n,u32(e+8)),string_at(n,u32(e+20))};}
