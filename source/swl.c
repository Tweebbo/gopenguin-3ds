#include "swl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {ART,TL,FRAME,ENTRY,CMD,LABEL,STATE,STRING,TABLES};
enum {DRAW,ROOT,MASK_BEGIN,MASK_READY,MASK_END,COLOR_BEGIN,COLOR_END};
enum {HASH_SIZE=2048,DRAW_HASH=8192,INSTANCE_HASH=16384,COUNTERS=8,OVERRIDES=768,OVERRIDE_HASH=2048};
_Static_assert(OVERRIDES*2<=OVERRIDE_HASH&&(OVERRIDE_HASH&(OVERRIDE_HASH-1))==0,"Override hash capacity");
static const unsigned caps[TABLES]={8192,8192,65536,131072,65536,16384,1024,1024*1024};
typedef struct {
 char path[SWL_PATH];uint32_t hash,symbol,frame,random,names[COUNTERS],values[COUNTERS];
 bool playing,seen,active;
} Head;
typedef struct {char path[SWL_PATH],label[64];uint32_t frame;bool hasFrame,playing,hasVisible,visible,hasColor,hasMatrix;SWLColor color;SWLMatrix matrix;} Override;
typedef struct {SWLMatrix matrix;SWLColor color;uint32_t token,art;float depth;int previous;uint8_t type,layer;} Draw;
typedef struct {uint64_t hash;SWLInstance instance;} Instance;
struct SWLScene {
 uint8_t *data;const uint8_t *table[TABLES];uint32_t count[TABLES],root,pages,version,bits;
 unsigned stride[TABLES],used,overrides,drawCount,oldCount,actionDepth,maskDepth,colorDepth;
 float width,height,rate;Head *heads;uint16_t hash[HASH_SIZE],overrideHash[OVERRIDE_HASH];Override *override;
 Draw *draw,*old;uint16_t oldHash[DRAW_HASH];bool dirty,building;const char *error;size_t fileBytes;
 Instance *instances;SWLHit *hits;uint16_t instanceHash[INSTANCE_HASH];unsigned instanceCount,hitCount;
 SWLSound sound;void *soundUser;SWLText text;void *textUser;SWLArt *glyphs;unsigned glyphCount;SWLStats stats;
 char viewPath[SWL_PATH];unsigned viewFrame;
};
static const SWLColor identity={{1,1,1,1},{0,0,0,0}};
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float f32(const uint8_t *p){uint32_t u=u32(p);float f;memcpy(&f,&u,4);return f;}
static const uint8_t *row(const SWLScene *s,unsigned t,unsigned n){return s->table[t]+(size_t)n*s->stride[t];}
static bool range(uint32_t first,uint32_t count,uint32_t total){return first<=total&&count<=total-first;}
static bool string_valid(const SWLScene *s,uint32_t n){return n<s->count[STRING]&&memchr(s->table[STRING]+n,0,s->count[STRING]-n);}
static const char *string_at(const SWLScene *s,uint32_t n){return (const char *)s->table[STRING]+n;}
static uint32_t hash_path(const char *p){uint32_t h=2166136261u;for(;*p;p++)h=(h^(uint8_t)*p)*16777619u;return h;}
static uint64_t instance_hash(const char *p){uint64_t h=14695981039346656037ull;for(;*p;p++)h=(h^(uint8_t)*p)*1099511628211ull;return h;}
static const uint8_t *symbol(const SWLScene *s,unsigned t,uint32_t id){
 unsigned lo=0,hi=s->count[t];while(lo<hi){unsigned mid=lo+(hi-lo)/2;const uint8_t *r=row(s,t,mid);uint32_t n=u32(r);if(n==id)return r;if(n<id)lo=mid+1;else hi=mid;}return NULL;
}
static bool fail(SWLScene *s,const char *message){s->error=message;return false;}
static bool prepare(SWLScene *s);
static bool finite_matrix(SWLMatrix m){const float v[]={m.a,m.b,m.c,m.d,m.x,m.y};for(unsigned i=0;i<6;i++)if(!isfinite(v[i])||fabsf(v[i])>1e9f)return false;return true;}
static bool finite_color(SWLColor c){for(unsigned i=0;i<4;i++)if(!isfinite(c.mult[i])||!isfinite(c.add[i])||fabsf(c.mult[i])>1e9f||fabsf(c.add[i])>1e9f)return false;return true;}
static bool color_identity(SWLColor c){for(unsigned i=0;i<4;i++)if(c.mult[i]!=1||c.add[i]!=0)return false;return true;}
static SWLColor color_multiply(SWLColor p,SWLColor c){SWLColor out;for(unsigned i=0;i<4;i++){out.mult[i]=p.mult[i]*c.mult[i];out.add[i]=p.mult[i]*c.add[i]+p.add[i];}return out;}
SWLMatrix swl_matrix_multiply(SWLMatrix p,SWLMatrix q){return (SWLMatrix){p.a*q.a+p.c*q.b,p.b*q.a+p.d*q.b,p.a*q.c+p.c*q.d,p.b*q.c+p.d*q.d,p.a*q.x+p.c*q.y+p.x,p.b*q.x+p.d*q.y+p.y};}
static bool validate(SWLScene *s){
 if(!isfinite(s->width)||!isfinite(s->height)||s->width<=0||s->height<=0||s->width>4096||s->height>4096||!isfinite(s->rate)||s->rate<=0||s->rate>120)return fail(s,"Invalid stage");
 if(!s->pages||s->pages>SWL_MAX_PAGES||(size_t)s->pages*SWL_ATLAS_WIDTH*SWL_ATLAS_HEIGHT*s->bits/8>6u*1024u*1024u||!symbol(s,TL,s->root)||!s->count[STRING]||s->table[STRING][0]||s->table[STRING][s->count[STRING]-1])return fail(s,"Invalid scene header");
 for(unsigned t=ART;t<=TL;t++)for(unsigned i=0;i<s->count[t];i++){
  const uint8_t *r=row(s,t,i);if(i&&u32(row(s,t,i-1))>=u32(r))return fail(s,"Duplicate or unsorted symbol");
  if(t==ART){unsigned x=u32(r+8),y=u32(r+12),w=u32(r+16),h=u32(r+20);if(symbol(s,TL,u32(r))||u32(r+4)>=s->pages||!w||!h||!range(x,w,SWL_ATLAS_WIDTH)||!range(y,h,SWL_ATLAS_HEIGHT))return fail(s,"Invalid atlas rectangle");for(unsigned j=24;j<40;j+=4)if(!isfinite(f32(r+j))||fabsf(f32(r+j))>1000000)return fail(s,"Invalid art bounds");if(f32(r+32)<=0||f32(r+36)<=0)return fail(s,"Empty art bounds");}
  else if(!u32(r+8)||!range(u32(r+4),u32(r+8),s->count[FRAME]))return fail(s,"Invalid timeline range");
 }
 for(unsigned i=0;i<s->count[FRAME];i++){const uint8_t *r=row(s,FRAME,i);if(!range(u32(r),u32(r+4),s->count[ENTRY])||!range(u32(r+8),u32(r+12),s->count[CMD]))return fail(s,"Invalid frame range");}
 for(unsigned i=0;i<s->count[ENTRY];i++){
  const uint8_t *r=row(s,ENTRY,i);unsigned flags=u32(r+12);
  if((!symbol(s,ART,u32(r))&&!symbol(s,TL,u32(r)))||!string_valid(s,u32(r+8))||flags>(s->version==2?2u:0u)||(!flags&&u32(r+16))||((flags&1)&&u32(r+16)<u32(r+4))||(flags==2&&!u32(r+16)))return fail(s,"Invalid scene entry");
  if(strlen(string_at(s,u32(r+8)))>=SWL_PATH)return fail(s,"Entry name exceeds budget");
  for(unsigned j=20;j<s->stride[ENTRY];j+=4)if(!isfinite(f32(r+j))||fabsf(f32(r+j))>1000000)return fail(s,"Invalid transform");
 }
 for(unsigned i=0;i<s->count[CMD];i++){
  const uint8_t *r=row(s,CMD,i);unsigned op=u32(r);
  if(op<1||op>(s->version==2?10u:6u))return fail(s,"Invalid command");
  if(s->version==1){if(u32(r+8))return fail(s,"Unsupported legacy target");}
  else if(!string_valid(s,u32(r+16))||!string_valid(s,u32(r+20))||(u32(r+24)!=0&&u32(r+24)!=1&&u32(r+24)!=2&&u32(r+24)!=4)||u32(r+28)||((u32(r+24)&5)&&!string_valid(s,u32(r+4)))||((u32(r+24)&2)&&(!u32(r+4)||u32(r+4)>65535||(op!=3&&op!=4))))return fail(s,"Invalid command arguments");
 }
 for(unsigned i=0;i<s->count[LABEL];i++){const uint8_t *r=row(s,LABEL,i),*t=symbol(s,TL,u32(r));if(!t||!string_valid(s,u32(r+4))||u32(r+8)<1||u32(r+8)>u32(t+8))return fail(s,"Invalid label");}
 for(unsigned i=0;i<s->count[STATE];i++){const uint8_t *r=row(s,STATE,i);if(!string_valid(s,u32(r))||strlen(string_at(s,u32(r)))>=SWL_PATH||!u32(r+4)||(u32(r+8)&~7u)||u32(r+12))return fail(s,"Invalid initial state");}
 return true;
}
static SWLScene *load_owned(uint8_t *data,size_t bytes,const char **error){
 if(error)*error=NULL;
 if(!data||bytes<64||bytes>SWL_MAX_FILE){if(error)*error="Invalid scene size";free(data);return NULL;}
 unsigned version=u32(data+4);
 if((version!=1&&version!=2)||memcmp(data,version==1?"SWL1":"SWL2",4)||u32(data+8)!=bytes){if(error)*error="Invalid scene header";free(data);return NULL;}
 SWLScene *s=calloc(1,sizeof *s);if(!s){free(data);if(error)*error="Out of memory";return NULL;}
 s->data=data;s->version=version;s->fileBytes=bytes;s->heads=calloc(SWL_MAX_HEADS,sizeof *s->heads);s->override=calloc(OVERRIDES,sizeof *s->override);s->draw=calloc(SWL_MAX_DRAWS,sizeof *s->draw);s->old=calloc(SWL_MAX_DRAWS,sizeof *s->old);
 s->instances=calloc(SWL_MAX_INSTANCES,sizeof *s->instances);s->hits=calloc(SWL_MAX_HITS,sizeof *s->hits);s->glyphs=calloc(1024,sizeof *s->glyphs);
 if(!s->heads||!s->override||!s->draw||!s->old||!s->instances||!s->hits||!s->glyphs){s->error="Out of memory";goto bad;}
 const unsigned strides[]={40,12,16,version==1?76:80,version==1?12:32,12,16,1};memcpy(s->stride,strides,sizeof strides);
 s->width=f32(data+12);s->height=f32(data+16);s->rate=f32(data+20);s->root=u32(data+24);s->pages=u32(data+28);s->bits=s->version==2&&(s->pages&0x80000000u)?16:32;if(s->version==2)s->pages&=0x7fffffffu;
 size_t offset=64;for(unsigned t=0;t<TABLES;t++){s->count[t]=u32(data+32+t*4);if(s->count[t]>caps[t]||(size_t)s->count[t]>(bytes-offset)/s->stride[t]){s->error="Table exceeds budget";goto bad;}s->table[t]=data+offset;offset+=(size_t)s->count[t]*s->stride[t];}
 if(offset!=bytes){s->error="Trailing scene bytes";goto bad;}
 if(!validate(s)||!prepare(s))goto bad;
 return s;
bad:if(error)*error=s->error;swl_free(s);return NULL;
}
SWLScene *swl_load(const void *data,size_t bytes,const char **error){
 if(!data||bytes<64||bytes>SWL_MAX_FILE){if(error)*error="Invalid scene size";return NULL;}uint8_t *copy=malloc(bytes);if(!copy){if(error)*error="Out of memory";return NULL;}memcpy(copy,data,bytes);return load_owned(copy,bytes,error);
}
SWLScene *swl_load_file(const char *path,const char **error){
 FILE *f=fopen(path,"rb");if(!f){if(error)*error="Could not open scene";return NULL;}if(fseek(f,0,SEEK_END)){fclose(f);if(error)*error="Could not seek scene";return NULL;}long bytes=ftell(f);if(bytes<64||(unsigned long)bytes>SWL_MAX_FILE||fseek(f,0,SEEK_SET)){fclose(f);if(error)*error="Invalid scene size";return NULL;}
 uint8_t *data=malloc((size_t)bytes);if(!data){fclose(f);if(error)*error="Out of memory";return NULL;}size_t got=fread(data,1,bytes,f);fclose(f);if(got!=(size_t)bytes){free(data);if(error)*error="Truncated scene";return NULL;}return load_owned(data,got,error);
}
void swl_free(SWLScene *s){if(s){free(s->data);free(s->heads);free(s->override);free(s->draw);free(s->old);free(s->instances);free(s->hits);free(s->glyphs);free(s);}}
unsigned swl_pages(const SWLScene *s){return s->pages;}
float swl_rate(const SWLScene *s){return s->rate;}
const char *swl_error(const SWLScene *s){return s->error;}
SWLStats swl_stats(const SWLScene *s){SWLStats v=s->stats;v.heads=s->used;v.bytes=sizeof *s+s->fileBytes+SWL_MAX_HEADS*sizeof(Head)+OVERRIDES*sizeof(Override)+2*SWL_MAX_DRAWS*sizeof(Draw)+SWL_MAX_INSTANCES*sizeof(Instance)+SWL_MAX_HITS*sizeof(SWLHit)+1024*sizeof(SWLArt);return v;}
static Head *head_at(const SWLScene *s,const char *path){uint32_t h=hash_path(path);for(unsigned n=0;n<HASH_SIZE;n++){unsigned slot=(h+n)&(HASH_SIZE-1),v=s->hash[slot];if(!v)return NULL;Head *head=&s->heads[v-1];if(head->hash==h&&!strcmp(head->path,path))return head->active?head:NULL;}return NULL;}
static void rehash(SWLScene *s){memset(s->hash,0,sizeof s->hash);for(unsigned i=0;i<s->used;i++)if(s->heads[i].active){uint32_t h=s->heads[i].hash;for(unsigned n=0;n<HASH_SIZE;n++){unsigned slot=(h+n)&(HASH_SIZE-1);if(!s->hash[slot]){s->hash[slot]=i+1;break;}}}}
static unsigned clamp_frame(const uint8_t *t,unsigned n){return n<1?1:n>u32(t+8)?u32(t+8):n;}
static unsigned resolve(const SWLScene *s,uint32_t id,unsigned frame,const char *label){if(label){for(unsigned i=0;i<s->count[LABEL];i++){const uint8_t *r=row(s,LABEL,i);if(u32(r)==id&&!strcmp(label,string_at(s,u32(r+4))))return u32(r+8);}return 0;}const uint8_t *t=symbol(s,TL,id);return t?clamp_frame(t,frame):0;}
static unsigned override_slot(const SWLScene *s,const char *path){uint32_t h=hash_path(path);for(unsigned n=0;n<OVERRIDE_HASH;n++){unsigned slot=(h+n)&(OVERRIDE_HASH-1),v=s->overrideHash[slot];if(!v||!strcmp(path,s->override[v-1].path))return slot;}return OVERRIDE_HASH;}
static Override *override_at(SWLScene *s,const char *path,bool create){unsigned slot=override_slot(s,path);if(slot<OVERRIDE_HASH&&s->overrideHash[slot])return &s->override[s->overrideHash[slot]-1];if(!create)return NULL;if(strlen(path)>=SWL_PATH||s->overrides>=OVERRIDES||slot==OVERRIDE_HASH){fail(s,"Instance override budget exceeded");return NULL;}Override *o=&s->override[s->overrides++];memcpy(o->path,path,strlen(path)+1);s->overrideHash[slot]=(uint16_t)s->overrides;return o;}
static void reset_children(SWLScene *s,const char *path){size_t len=strlen(path);bool changed=false;for(unsigned i=0;i<s->used;i++)if(s->heads[i].active&&((!len&&s->heads[i].path[0])||(!strncmp(s->heads[i].path,path,len)&&(s->heads[i].path[len]=='.'||s->heads[i].path[len]=='#')))){s->heads[i].active=false;changed=true;}if(changed)rehash(s);}
static bool relative_path(SWLScene *s,char *out,const char *from,const char *target){
 if(strlen(from)>=SWL_PATH)return fail(s,"Instance path too long");
 strcpy(out,from);const char *part=target;
 while(*part){const char *end=strchr(part,'.');size_t n=end?(size_t)(end-part):strlen(part);if(n==4&&!memcmp(part,"this",4)){}
  else if(n==7&&!memcmp(part,"_parent",7)){char *last=NULL;for(char *p=out;*p;p++)if(*p=='.'||*p=='#')last=p;if(last)*last=0;else out[0]=0;}
  else{size_t len=strlen(out);if(len+n+2>SWL_PATH)return fail(s,"Target path too long");if(len)out[len++]='.';memcpy(out+len,part,n);out[len+n]=0;}
  if(!end)break;
  part=end+1;
 }return true;
}
static bool run(SWLScene *s,Head *h);
static bool goto_internal(SWLScene *s,const char *path,unsigned frame,const char *label,bool playing,bool require){
 Head *h=head_at(s,path);if(require&&!h)return false;if(label&&strlen(label)>=64)return fail(s,"Frame label too long");unsigned resolved=h?resolve(s,h->symbol,frame,label):frame;if(h&&!resolved)return false;
 Override *o=override_at(s,path,true);if(!o)return false;o->hasFrame=true;o->frame=frame;o->playing=playing;snprintf(o->label,sizeof o->label,"%s",label?label:"");
 if(h){bool changed=h->frame!=resolved;if(changed){reset_children(s,path);memset(h->names,0,sizeof h->names);memset(h->values,0,sizeof h->values);}h->frame=resolved;h->playing=playing;if(changed&&!run(s,h))return false;}
 s->dirty=true;return true;
}
static bool run(SWLScene *s,Head *h){
 if(++s->actionDepth>32){s->actionDepth--;return fail(s,"Timeline action recursion exceeds budget");}
 const uint8_t *t=symbol(s,TL,h->symbol),*f=row(s,FRAME,u32(t+4)+h->frame-1);
 for(unsigned i=0;i<u32(f+12)&&!s->error;i++){
  const uint8_t *c=row(s,CMD,u32(f+8)+i);unsigned op=u32(c),arg=u32(c+4),previous=h->frame;
  if(s->version==2&&(u32(c+24)&2)){h->random^=h->random<<13;h->random^=h->random>>17;h->random^=h->random<<5;arg=1+h->random%arg;}
  if(s->version==2&&(u32(c+24)&4)){
   char path[SWL_PATH];if(!relative_path(s,path,h->path,string_at(s,arg)))break;
   Head *other=head_at(s,path);arg=other?other->frame:1;
   if(!other)for(unsigned k=0;k<s->count[STATE];k++){const uint8_t *r=row(s,STATE,k);if(!strcmp(path,string_at(s,u32(r)))&&(u32(r+8)&2)){arg=u32(r+4);break;}}
  }
  const char *target=s->version==2?string_at(s,u32(c+16)):"",*label=s->version==2&&(u32(c+24)&1)?string_at(s,arg):NULL;
  if(op==10){if(s->sound)s->sound(s->soundUser,h->path,arg,u32(c+8),u32(c+12));continue;}
  if(op==7||op==8){unsigned name=u32(c+20),slot=0;for(;slot<COUNTERS;slot++)if(h->names[slot]==name||!h->names[slot])break;if(slot==COUNTERS){fail(s,"Counter budget exceeded");break;}if(op==7){h->names[slot]=name;h->values[slot]=arg;}else{bool again=h->names[slot]&&h->values[slot]<arg;if(again)h->values[slot]++;h->frame=clamp_frame(t,u32(c+(again?8:12)));h->playing=again;}continue;}
  if(*target){char path[SWL_PATH];if(!relative_path(s,path,h->path,target))break;Head *other=head_at(s,path);unsigned next=arg;if(op==1||op==2)next=other?other->frame:1;if(op==5||op==6)next=other?other->frame+(op==5?1:other->frame>1?-1:0):1;goto_internal(s,path,next,label,op==2||op==4,false);continue;}
  if(op==1)h->playing=false;
  else if(op==2)h->playing=true;
  else if(op==3||op==4){unsigned next=resolve(s,h->symbol,arg,label);if(!next){fail(s,"Unknown frame label");break;}h->frame=next;h->playing=op==4;}
  else if(op==5||op==6){h->frame=clamp_frame(t,op==5?h->frame+1:h->frame>1?h->frame-1:1);h->playing=false;}
  if(h->frame!=previous&&!run(s,h))break;
 }
 s->actionDepth--;return !s->error;
}
static Head *get_head(SWLScene *s,const char *path,uint32_t id){
 Head *h=head_at(s,path);if(h&&h->symbol==id){h->seen=true;return h;}if(h)reset_children(s,path);
 if(!h)for(unsigned i=0;i<s->used;i++)if(!s->heads[i].active){h=&s->heads[i];break;}
 if(!h){if(s->used>=SWL_MAX_HEADS){fail(s,"Instance budget exceeded");return NULL;}h=&s->heads[s->used++];}
 memset(h,0,sizeof *h);memcpy(h->path,path,strlen(path)+1);h->hash=hash_path(path);h->random=h->hash^0xa341316cu;h->symbol=id;h->frame=1;h->seen=h->active=true;h->playing=u32(symbol(s,TL,id)+8)>1;rehash(s);
 for(unsigned i=0;i<s->count[STATE];i++){const uint8_t *r=row(s,STATE,i);if(strcmp(path,string_at(s,u32(r))))continue;unsigned flags=u32(r+8);if(flags&2){h->frame=clamp_frame(symbol(s,TL,id),u32(r+4));h->playing=(flags&4)!=0;}}
 Override *o=override_at(s,path,false);if(o&&o->hasFrame){unsigned frame=resolve(s,id,o->frame,o->label[0]?o->label:NULL);if(!frame){fail(s,"Unknown target label");return NULL;}h->frame=frame;h->playing=o->playing;}
 if(!run(s,h))return NULL;
 return h;
}
static bool hidden(SWLScene *s,const char *path){Override *o=override_at(s,path,false);if(o&&o->hasVisible)return !o->visible;for(unsigned i=0;i<s->count[STATE];i++){const uint8_t *r=row(s,STATE,i);if((u32(r+8)&1)&&!strcmp(path,string_at(s,u32(r))))return true;}return false;}
static bool append(SWLScene *s,unsigned type,unsigned layer,float depth,SWLMatrix matrix,SWLColor color,uint32_t art,uint32_t token){
 if(s->drawCount>=SWL_MAX_DRAWS)return fail(s,"Draw command budget exceeded");
 if(!finite_matrix(matrix)||!finite_color(color))return fail(s,"Composed transform exceeds budget");
 Draw *d=&s->draw[s->drawCount++];*d=(Draw){.matrix=matrix,.color=color,.token=token,.art=art,.depth=depth,.previous=-1,.type=type,.layer=layer};
 if(type==DRAW){s->stats.draws++;unsigned slot=(token^art*2654435761u)&(DRAW_HASH-1);while(s->oldHash[slot]){unsigned i=s->oldHash[slot]-1;if(s->old[i].token==token&&s->old[i].art==art){d->previous=(int)i;break;}slot=(slot+1)&(DRAW_HASH-1);}}
 return true;
}
static bool walk(SWLScene *s,uint32_t id,const char *path,SWLMatrix matrix,SWLColor color,unsigned depth,unsigned layer,float renderDepth,bool masking);
static bool entry(SWLScene *s,const uint8_t *r,const char *parent,SWLMatrix matrix,unsigned depth,unsigned layer,float renderDepth,bool force,bool masking){
 const char *name=string_at(s,u32(r+8));char path[SWL_PATH];
 if(*name){size_t a=strlen(parent),b=strlen(name),dot=a?1:0;if(a+dot+b>=sizeof path)return fail(s,"Instance path exceeds budget");memcpy(path,parent,a);if(dot)path[a]='.';memcpy(path+a+dot,name,b+1);}
 else{int n=snprintf(path,sizeof path,"%s#%u",parent,(unsigned)u32(r+4));if(n<0||(size_t)n>=sizeof path)return fail(s,"Instance path exceeds budget");}
 if(!force&&hidden(s,path))return true;
 unsigned offset=s->version==2?24:20;SWLMatrix local={f32(r+offset),f32(r+offset+4),f32(r+offset+8),f32(r+offset+12),f32(r+offset+16),f32(r+offset+20)};SWLColor color=identity;
 if(!masking)for(unsigned i=0;i<4;i++){color.mult[i]=f32(r+offset+24+i*4);color.add[i]=f32(r+offset+40+i*4);}
 Override *override=override_at(s,path,false);if(override){if(override->hasMatrix)local=override->matrix;if(override->hasColor&&!masking)color=override->color;}
 if(color.mult[3]==0&&color.add[3]==0)return true;
 SWLMatrix world=swl_matrix_multiply(matrix,local);
 if((u32(r+12)&2)&&!masking){if(s->hitCount>=SWL_MAX_HITS)return fail(s,"Interactive entry budget exceeded");SWLHit *hit=&s->hits[s->hitCount++];hit->id=u32(r+16);hit->matrix=world;memcpy(hit->path,path,strlen(path)+1);}
 return walk(s,u32(r),path,world,color,depth+1,layer,renderDepth,masking);
}
static bool list(SWLScene *s,unsigned first,unsigned count,const char *path,SWLMatrix matrix,unsigned depth,unsigned layer,float renderDepth,bool masking){
 for(unsigned i=0;i<count;i++){
  const uint8_t *r=row(s,ENTRY,first+i);unsigned localLayer=layer;float z=renderDepth;
  if(!depth){localLayer=!strcmp(string_at(s,u32(r+8)),"foreground_mc")?1:0;z=s->version==2?f32(r+20):localLayer?900001:(float)u32(r+4);if(!append(s,ROOT,localLayer,z,matrix,identity,0,0))return false;}
  if(u32(r+12)&1){
   if(++s->maskDepth>SWL_MAX_MASKS)return fail(s,"Mask nesting exceeds budget");
   if(s->maskDepth>s->stats.masks)s->stats.masks=s->maskDepth;
   if(!append(s,MASK_BEGIN,localLayer,z,matrix,identity,0,0)||!entry(s,r,path,matrix,depth,localLayer,z,true,true)||!append(s,MASK_READY,localLayer,z,matrix,identity,0,0))return false;
   unsigned start=i+1;while(i+1<count&&u32(row(s,ENTRY,first+i+1)+4)<=u32(r+16))i++;
   if(!list(s,first+start,i-start+1,path,matrix,depth+1,localLayer,z,masking)||!append(s,MASK_END,localLayer,z,matrix,identity,0,0))return false;
   s->maskDepth--;
  }else if(!entry(s,r,path,matrix,depth,localLayer,z,false,masking))return false;
 }return true;
}
static bool walk(SWLScene *s,uint32_t id,const char *path,SWLMatrix matrix,SWLColor color,unsigned depth,unsigned layer,float renderDepth,bool masking){
 if(depth>SWL_MAX_DEPTH)return fail(s,"Timeline nesting exceeds budget");
 if(!finite_matrix(matrix))return fail(s,"Composed matrix exceeds budget");
 const uint8_t *t=symbol(s,TL,id);
 if(s->instanceCount>=SWL_MAX_INSTANCES)return fail(s,"Visible instance budget exceeded");
 uint64_t hash=instance_hash(path);unsigned slot=(unsigned)hash&(INSTANCE_HASH-1);while(s->instanceHash[slot])slot=(slot+1)&(INSTANCE_HASH-1);Instance *instance=&s->instances[s->instanceCount];instance->hash=hash;instance->instance=(SWLInstance){id,1,matrix};s->instanceHash[slot]=(uint16_t)(++s->instanceCount);
 if(s->text&&!masking){int n=s->text(s->textUser,path,id,s->glyphs+s->glyphCount,1024-s->glyphCount);if(n>=0){if((unsigned)n>1024-s->glyphCount)return fail(s,"Dynamic text exceeds glyph budget");for(int i=0;i<n;i++)if(!append(s,DRAW,layer,renderDepth,matrix,color,0x80000000u|s->glyphCount++,hash_path(path)+(unsigned)i))return false;return true;}}
 if(!t){const uint8_t *r=symbol(s,ART,id);if(!r)return fail(s,"Missing art");return append(s,DRAW,layer,renderDepth,matrix,color,(unsigned)((r-s->table[ART])/s->stride[ART]),hash_path(path));}
 const uint8_t *first=row(s,FRAME,u32(t+4));Head *h=NULL;
 if(u32(t+8)>1||u32(first+12)){h=get_head(s,path,id);if(!h)return false;}
 unsigned frame=h?h->frame:1;
 if(s->viewFrame&&!strcmp(path,s->viewPath)){if(s->viewFrame>u32(t+8))return fail(s,"Display frame outside timeline");frame=s->viewFrame;}
 instance->instance.frame=frame;
 bool group=!color_identity(color)&&!masking;unsigned begin=s->drawCount;
 if(group){if(++s->colorDepth>8)return fail(s,"Color nesting exceeds budget");if(s->colorDepth>s->stats.colorGroups)s->stats.colorGroups=s->colorDepth;if(!append(s,COLOR_BEGIN,layer,renderDepth,matrix,color,0,0))return false;}
 const uint8_t *f=row(s,FRAME,u32(t+4)+frame-1);if(!list(s,u32(f),u32(f+4),path,matrix,depth,layer,renderDepth,masking))return false;
 if(group){
  if(s->drawCount==begin+2&&s->draw[begin+1].type==DRAW){Draw d=s->draw[begin+1];d.color=color_multiply(color,d.color);s->draw[begin]=d;s->drawCount--;}
  else if(!append(s,COLOR_END,layer,renderDepth,matrix,color,0,0))return false;
  s->colorDepth--;
 }
 return true;
}
static bool prepare(SWLScene *s){
 if(s->building)return true;
 s->building=true;Draw *tmp=s->old;s->old=s->draw;s->draw=tmp;s->oldCount=s->drawCount;s->drawCount=0;s->stats.draws=s->stats.masks=s->stats.colorGroups=0;s->maskDepth=s->colorDepth=0;
 memset(s->oldHash,0,sizeof s->oldHash);for(unsigned i=0;i<s->oldCount;i++)if(s->old[i].type==DRAW){Draw *d=&s->old[i];unsigned slot=(d->token^d->art*2654435761u)&(DRAW_HASH-1);while(s->oldHash[slot]){Draw *old=&s->old[s->oldHash[slot]-1];if(old->token==d->token&&old->art==d->art)break;slot=(slot+1)&(DRAW_HASH-1);}if(!s->oldHash[slot])s->oldHash[slot]=(uint16_t)(i+1);}
 bool ok=false;for(unsigned pass=0;pass<32;pass++){
  s->dirty=false;s->drawCount=0;s->stats.draws=s->stats.masks=s->stats.colorGroups=0;s->maskDepth=s->colorDepth=0;
  s->instanceCount=s->hitCount=s->glyphCount=0;memset(s->instanceHash,0,sizeof s->instanceHash);
  for(unsigned i=0;i<s->used;i++)s->heads[i].seen=false;
  ok=walk(s,s->root,"",(SWLMatrix){1,0,0,1,0,0},identity,0,0,0,false);if(!ok||!s->dirty)break;
  if(pass==31)ok=fail(s,"Timeline targets did not settle");
 }
 s->stats.preparations++;s->building=false;s->dirty=false;return ok;
}
void swl_begin(SWLScene *s){(void)s;}
bool swl_view_frame(SWLScene *s,const char *path,unsigned frame){
 if(!s||!path||strlen(path)>=SWL_PATH)return false;
 if(s->viewFrame==frame&&(!frame||!strcmp(s->viewPath,path)))return true;
 s->viewFrame=frame;snprintf(s->viewPath,sizeof s->viewPath,"%s",path);s->dirty=true;return true;
}
bool swl_prepare(SWLScene *s){return !s->dirty||prepare(s);}
bool swl_goto(SWLScene *s,const char *path,unsigned frame,const char *label,bool playing){return goto_internal(s,path,frame,label,playing,true);}
bool swl_target(SWLScene *s,const char *path,unsigned frame,const char *label,bool playing){return goto_internal(s,path,frame,label,playing,false);}
bool swl_color(SWLScene *s,const char *path,SWLColor color){if(!finite_color(color))return false;Override *o=override_at(s,path,true);if(!o)return false;if(!o->hasColor||memcmp(&o->color,&color,sizeof color)){o->color=color;o->hasColor=true;s->dirty=true;}return true;}
bool swl_transform(SWLScene *s,const char *path,SWLMatrix matrix){if(!finite_matrix(matrix))return false;Override *o=override_at(s,path,true);if(!o)return false;if(!o->hasMatrix||memcmp(&o->matrix,&matrix,sizeof matrix)){o->matrix=matrix;o->hasMatrix=true;s->dirty=true;}return true;}
bool swl_instance(SWLScene *s,const char *path,SWLInstance *out){if(s->dirty&&!prepare(s))return false;uint64_t hash=instance_hash(path);unsigned slot=(unsigned)hash&(INSTANCE_HASH-1);while(s->instanceHash[slot]){const Instance *instance=&s->instances[s->instanceHash[slot]-1];if(instance->hash==hash){*out=instance->instance;return true;}slot=(slot+1)&(INSTANCE_HASH-1);}return false;}
static unsigned pose_frame(const SWLScene *s,unsigned id,const char *path){
 const uint8_t *timeline=symbol(s,TL,id);if(!timeline)return 1;
 Head *head=head_at(s,path);if(head&&head->symbol==id)return head->frame;
 unsigned slot=override_slot(s,path);
 if(slot<OVERRIDE_HASH&&s->overrideHash[slot]){const Override *o=&s->override[s->overrideHash[slot]-1];if(o->hasFrame){unsigned frame=resolve(s,id,o->frame,*o->label?o->label:NULL);if(frame)return frame;}}
 for(unsigned i=0;i<s->count[STATE];i++){const uint8_t *r=row(s,STATE,i);if((u32(r+8)&2)&&!strcmp(path,string_at(s,u32(r))))return clamp_frame(timeline,u32(r+4));}
 return 1;
}
bool swl_pose(const SWLScene *s,const char *path,SWLInstance *out){
 if(!s||!path||!out||strlen(path)>=SWL_PATH)return false;
 unsigned id=s->root,depth=0;SWLMatrix matrix={1,0,0,1,0,0};char prefix[SWL_PATH]={0};const char *cursor=path;
 while(*cursor){
  if(++depth>SWL_MAX_DEPTH)return false;
  if(*cursor=='.'){cursor++;if(!*cursor||*cursor=='.'||*cursor=='#')return false;}
  const char *end=cursor;if(*end=='#')end++;while(*end&&*end!='.'&&*end!='#')end++;
  size_t length=(size_t)(end-cursor);if(!length)return false;
  const uint8_t *timeline=symbol(s,TL,id);if(!timeline)return false;
  unsigned frame=pose_frame(s,id,prefix);const uint8_t *f=row(s,FRAME,u32(timeline+4)+frame-1),*found=NULL;
  unsigned wantedDepth=0;bool byDepth=*cursor=='#';
  if(byDepth){if(length<2||length>11)return false;uint64_t n=0;for(size_t i=1;i<length;i++){if(cursor[i]<'0'||cursor[i]>'9')return false;n=n*10+(unsigned)(cursor[i]-'0');if(n>UINT32_MAX)return false;}wantedDepth=(unsigned)n;}
  for(unsigned i=0;i<u32(f+4);i++){const uint8_t *entry=row(s,ENTRY,u32(f)+i);const char *name=string_at(s,u32(entry+8));if(byDepth?u32(entry+4)==wantedDepth:strlen(name)==length&&!memcmp(name,cursor,length)){found=entry;break;}}
  if(!found)return false;
  size_t used=strlen(prefix),dot=used&&!byDepth?1:0;if(used+dot+length>=sizeof prefix)return false;if(dot)prefix[used++]='.';memcpy(prefix+used,cursor,length);prefix[used+length]=0;
  unsigned offset=s->version==2?24:20;SWLMatrix local={f32(found+offset),f32(found+offset+4),f32(found+offset+8),f32(found+offset+12),f32(found+offset+16),f32(found+offset+20)};
  unsigned slot=override_slot(s,prefix);if(slot<OVERRIDE_HASH&&s->overrideHash[slot]){const Override *o=&s->override[s->overrideHash[slot]-1];if(o->hasMatrix)local=o->matrix;}
  matrix=swl_matrix_multiply(matrix,local);if(!finite_matrix(matrix))return false;id=u32(found);cursor=end;
 }
 *out=(SWLInstance){id,pose_frame(s,id,prefix),matrix};return true;
}
unsigned swl_frames(SWLScene *s,const char *path){SWLInstance instance;if(!swl_instance(s,path,&instance))return 0;const uint8_t *t=symbol(s,TL,instance.symbol);return t?u32(t+8):0;}
unsigned swl_hits(SWLScene *s){return s->dirty&&!prepare(s)?0:s->hitCount;}
const SWLHit *swl_hit(SWLScene *s,unsigned index){return index<swl_hits(s)?&s->hits[index]:NULL;}
bool swl_visible(SWLScene *s,const char *path,bool visible){Override *o=override_at(s,path,true);if(!o)return false;o->hasVisible=true;o->visible=visible;s->dirty=true;return true;}
unsigned swl_frame(const SWLScene *s,const char *path){Head *h=head_at(s,path);if(h)return h->frame;unsigned slot=override_slot(s,path),v=slot<OVERRIDE_HASH?s->overrideHash[slot]:0;if(v&&s->override[v-1].hasFrame)return s->override[v-1].frame;for(unsigned i=0;i<s->count[STATE];i++){const uint8_t *r=row(s,STATE,i);if(!strcmp(string_at(s,u32(r)),path)&&(u32(r+8)&2))return u32(r+4);}return 0;}
bool swl_art(const SWLScene *s,unsigned id,SWLArt *art){const uint8_t *r=symbol(s,ART,id);if(!r)return false;*art=(SWLArt){u32(r),u32(r+4),u32(r+8),u32(r+12),u32(r+16),u32(r+20),f32(r+24),f32(r+28),f32(r+32),f32(r+36)};return true;}
void swl_text_handler(SWLScene *s,SWLText handler,void *user){s->text=handler;s->textUser=user;s->dirty=true;}
void swl_invalidate(SWLScene *s){s->dirty=true;}
void swl_sound_handler(SWLScene *s,SWLSound handler,void *user){s->sound=handler;s->soundUser=user;}
bool swl_restart(SWLScene *s){s->used=0;s->error=NULL;s->drawCount=s->oldCount=0;memset(s->hash,0,sizeof s->hash);return prepare(s);}
bool swl_tick(SWLScene *s){
 if(s->dirty&&!prepare(s))return false;
 unsigned count=s->used;bool changed=false;
 for(unsigned i=0;i<count;i++){Head *h=&s->heads[i];if(!h->active||!h->seen||!h->playing)continue;changed=true;const uint8_t *t=symbol(s,TL,h->symbol);h->frame=h->frame>=u32(t+8)?1:h->frame+1;if(!run(s,h))return false;}
 if(!changed){for(unsigned i=0;i<s->drawCount;i++)s->draw[i].previous=-1;return !s->error;}
 return !s->error&&prepare(s);
}
bool swl_render(SWLScene *s,SWLMatrix view,int layer,float interpolation,const SWLRenderer *renderer,void *user){
 if(!renderer||!renderer->emit||!finite_matrix(view)||!isfinite(interpolation))return fail(s,"Invalid renderer or view");
 if(s->dirty&&!prepare(s))return false;
 float alpha=fmaxf(0,fminf(1,interpolation));SWLColor colors[SWL_MAX_DEPTH+1];unsigned group=0;colors[0]=identity;
 for(unsigned i=0;i<s->drawCount;i++){
  const Draw *d=&s->draw[i];if(layer>=0&&d->layer!=(unsigned)layer)continue;
  if(d->type==ROOT){if(renderer->depth&&!renderer->depth(user,d->depth))return fail(s,"Depth callback failed");continue;}
  if(d->type>=MASK_BEGIN&&d->type<=MASK_END){if(!renderer->mask)return fail(s,"Renderer has no mask support");if(!renderer->mask(user,d->type-MASK_BEGIN+1))return fail(s,"Mask rendering failed");continue;}
  if(d->type==COLOR_BEGIN||d->type==COLOR_END){
   if(renderer->color){if(!renderer->color(user,d->type==COLOR_BEGIN?1:3,&d->color))return fail(s,"Color compositing failed");}
   else if(d->type==COLOR_BEGIN){if(group>=SWL_MAX_DEPTH)return fail(s,"Color stack exceeded");colors[group+1]=color_multiply(colors[group],d->color);group++;}else if(group)group--;
   continue;
  }
  SWLMatrix local=d->matrix;if(alpha<1&&d->previous>=0){const SWLMatrix *p=&s->old[d->previous].matrix;local=(SWLMatrix){p->a+(local.a-p->a)*alpha,p->b+(local.b-p->b)*alpha,p->c+(local.c-p->c)*alpha,p->d+(local.d-p->d)*alpha,p->x+(local.x-p->x)*alpha,p->y+(local.y-p->y)*alpha};}
  SWLMatrix matrix=swl_matrix_multiply(view,local);SWLColor color=color_multiply(colors[group],d->color);SWLArt art;
  if(d->art&0x80000000u)art=s->glyphs[d->art&0x7fffffffu];
  else{const uint8_t *r=row(s,ART,d->art);art=(SWLArt){u32(r),u32(r+4),u32(r+8),u32(r+12),u32(r+16),u32(r+20),f32(r+24),f32(r+28),f32(r+32),f32(r+36)};}
  if(!renderer->emit(user,&art,&matrix,&color))return fail(s,"Renderer rejected entry");
 }
 return true;
}
bool swl_draw(SWLScene *s,SWLMatrix view,int layer,SWLEmit emit,void *user){return swl_render(s,view,layer,1,&(SWLRenderer){.emit=emit},user);}
typedef struct {float bounds[4];bool any,painting[SWL_MAX_MASKS];unsigned masks;} Bounds;
static bool bounds_emit(void *user,const SWLArt *art,const SWLMatrix *m,const SWLColor *color){Bounds *b=user;(void)color;for(unsigned i=0;i<b->masks;i++)if(b->painting[i])return true;for(unsigned i=0;i<4;i++){float x=art->left+(i&1?art->width:0),y=art->top+(i&2?art->height:0),px=m->a*x+m->c*y+m->x,py=m->b*x+m->d*y+m->y;if(!b->any){b->bounds[0]=b->bounds[2]=px;b->bounds[1]=b->bounds[3]=py;b->any=true;}else{b->bounds[0]=fminf(b->bounds[0],px);b->bounds[1]=fminf(b->bounds[1],py);b->bounds[2]=fmaxf(b->bounds[2],px);b->bounds[3]=fmaxf(b->bounds[3],py);}}return true;}
static bool bounds_mask(void *user,unsigned event){Bounds *b=user;if(event==1){if(b->masks==SWL_MAX_MASKS)return false;b->painting[b->masks++]=true;}else if(event==2){if(!b->masks)return false;b->painting[b->masks-1]=false;}else if(event==3){if(!b->masks)return false;b->masks--;}return true;}
bool swl_bounds(SWLScene *s,float bounds[4]){Bounds b={0};if(!swl_render(s,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=bounds_emit,.mask=bounds_mask},&b)||!b.any)return false;memcpy(bounds,b.bounds,sizeof b.bounds);return true;}
