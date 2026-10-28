#include "movie_meta.h"
#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct MovieMeta {json_t *root,*controls,*assignments,*symbols,*textValues;unsigned frames;char clip[SWL_PATH];struct {char path[SWL_PATH];unsigned count;} secrets[32];unsigned nSecrets;};
static unsigned number(json_t *root,const char *key){json_t *n=json_object_get(root,key);json_int_t value=json_integer_value(n);return json_is_integer(n)&&value>=0&&value<=0x7fffffff?(unsigned)value:0;}
static const char *string(json_t *root,const char *key){const char *s=json_string_value(json_object_get(root,key));return s?s:"";}
static bool commands_valid(json_t *commands){if(!json_is_array(commands)||json_array_size(commands)>32)return false;for(size_t i=0;i<json_array_size(commands);i++){json_t *c=json_array_get(commands,i);unsigned op=number(c,"op");if(!json_is_object(c)||!((op>=1&&op<=4)||(op>=10&&op<=14)||op==24)||strlen(string(c,"target"))>=SWL_PATH||strlen(string(c,"room"))>=48)return false;json_t *frame=json_object_get(c,"frame");if(frame&&!json_is_integer(frame)&&!json_is_string(frame))return false;if(json_is_string(frame)&&strlen(json_string_value(frame))>=64)return false;}return true;}
static bool finite_number(json_t *v,double limit){return json_is_number(v)&&isfinite(json_number_value(v))&&fabs(json_number_value(v))<=limit;}
static bool fonts_valid(json_t *fonts){
 if(!fonts)return true;
 json_t *styles=json_object_get(fonts,"styles"),*fields=json_object_get(fonts,"fields");if(!json_is_array(styles)||json_array_size(styles)>64||!json_is_object(fields)||json_object_size(fields)>8192)return false;
 for(size_t i=0;i<json_array_size(styles);i++){json_t *s=json_array_get(styles,i),*glyphs=json_object_get(s,"glyphs"),*kern=json_object_get(s,"kern");if(!json_is_object(glyphs)||json_object_size(glyphs)>256||!json_is_object(kern)||json_object_size(kern)>65536)return false;const char *key;json_t *v;json_object_foreach(glyphs,key,v){(void)key;if(!number(v,"art")||!finite_number(json_object_get(v,"advance"),2048))return false;}json_object_foreach(kern,key,v){(void)key;if(!finite_number(v,2048))return false;}}
 const char *key;json_t *f;json_object_foreach(fields,key,f){(void)key;json_t *bounds=json_object_get(f,"bounds"),*margins=json_object_get(f,"margins"),*outline=json_object_get(f,"nativeOutline");if(number(f,"style")>=json_array_size(styles)||!json_is_array(bounds)||json_array_size(bounds)!=4||!json_is_array(margins)||json_array_size(margins)!=2||!finite_number(json_object_get(f,"size"),512)||!finite_number(json_object_get(f,"leading"),512))return false;for(unsigned i=0;i<4;i++)if(!finite_number(json_array_get(bounds,i),1000000))return false;for(unsigned i=0;i<2;i++)if(!finite_number(json_array_get(margins,i),10000))return false;if(outline&&!finite_number(json_object_get(outline,"width"),64))return false;}
 return true;
}
MovieMeta *movie_meta_load(const void *bytes,size_t length){
 if(!bytes||!length||length>2*1024*1024)return NULL;
 json_error_t error;json_t *root=json_loadb(bytes,length,JSON_REJECT_DUPLICATES,&error);if(!root)return NULL;
 MovieMeta *m=calloc(1,sizeof *m);if(!m){json_decref(root);return NULL;}m->root=root;m->textValues=json_object();if(!m->textValues)goto bad;m->controls=json_object_get(root,"controls");m->assignments=json_object_get(root,"assignments");m->symbols=json_object_get(root,"symbols");m->frames=number(root,"frames");const char *clip=string(root,"clip");
 if(!*clip||strlen(clip)>=SWL_PATH||!m->frames||m->frames>4096||!json_is_array(m->controls)||json_array_size(m->controls)>2048||!json_is_array(m->assignments)||json_array_size(m->assignments)>8192||!json_is_object(m->symbols))goto bad;
 if(!fonts_valid(json_object_get(root,"nativeFonts")))goto bad;
 strcpy(m->clip,clip);
 for(size_t i=0;i<json_array_size(m->controls);i++){json_t *c=json_array_get(m->controls,i),*bounds=json_object_get(c,"bounds");if(!number(c,"id")||!commands_valid(json_object_get(c,"commands"))||!json_is_array(bounds)||json_array_size(bounds)>1024)goto bad;for(size_t j=0;j<json_array_size(bounds);j++){json_t *b=json_array_get(bounds,j);if(json_is_null(b))continue;if(!json_is_array(b)||json_array_size(b)!=4)goto bad;for(unsigned k=0;k<4;k++){json_t *v=json_array_get(b,k);double n=json_number_value(v);if(!json_is_number(v)||!isfinite(n)||fabs(n)>1000000)goto bad;}}}
 for(size_t i=0;i<json_array_size(m->assignments);i++){json_t *a=json_array_get(m->assignments,i);if(strlen(string(a,"scope"))>=SWL_PATH||strlen(string(a,"path"))>=SWL_PATH||!number(a,"frame")||!commands_valid(json_object_get(a,"commands")))goto bad;}
 return m;
bad:movie_meta_free(m);return NULL;
}
void movie_meta_free(MovieMeta *m){if(m){json_decref(m->root);json_decref(m->textValues);free(m);}}
unsigned movie_meta_frames(const MovieMeta *m){return m->frames;}
unsigned movie_meta_number(const MovieMeta *m,const char *section,const char *key){return number(json_object_get(m->root,section),key);}
static int base64_value(unsigned char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;}
void *movie_meta_blob(const MovieMeta *m,const char *section,const char *key,size_t limit,size_t *length){
 const char *text=string(json_object_get(m->root,section),key);size_t n=strlen(text);if(!n||n%4)return NULL;size_t padding=text[n-1]=='='?(text[n-2]=='='?2:1):0,bytes=n/4*3-padding;if(bytes>limit)return NULL;unsigned char *out=malloc(bytes?bytes:1);if(!out)return NULL;size_t used=0;
 for(size_t i=0;i<n;i+=4){int a=base64_value(text[i]),b=base64_value(text[i+1]),c=text[i+2]=='='?0:base64_value(text[i+2]),d=text[i+3]=='='?0:base64_value(text[i+3]);if(a<0||b<0||c<0||d<0||((text[i+2]=='='||text[i+3]=='=')&&i+4!=n)||(text[i+2]=='='&&text[i+3]!='=')||(i+4==n&&padding==2&&(b&15))||(i+4==n&&padding==1&&(c&3))){free(out);return NULL;}unsigned value=(unsigned)a<<18|(unsigned)b<<12|(unsigned)c<<6|(unsigned)d;out[used++]=(unsigned char)(value>>16);if(used<bytes)out[used++]=(unsigned char)(value>>8);if(used<bytes)out[used++]=(unsigned char)value;}
 if(used!=bytes){free(out);return NULL;}if(length)*length=bytes;return out;
}
const char *movie_meta_clip(const MovieMeta *m){return m->clip;}
unsigned movie_meta_source(const MovieMeta *m,unsigned symbol){char id[20];snprintf(id,sizeof id,"%u",symbol);json_t *value=json_object_get(m->symbols,id);return json_is_integer(value)?(unsigned)json_integer_value(value):symbol;}
bool movie_meta_text(MovieMeta *m,const char *path,const char *value){if(!m||!path||!value||strlen(path)>=SWL_PATH||strlen(value)>1024)return false;const char *old=json_string_value(json_object_get(m->textValues,path));if(old&&!strcmp(old,value))return false;if(!old&&json_object_size(m->textValues)>=128)return false;return json_object_set_new(m->textValues,path,json_string(value))==0;}
static double decimal(json_t *root,const char *key){return json_number_value(json_object_get(root,key));}
static json_t *glyph(json_t *style,unsigned code){char key[12];snprintf(key,sizeof key,"%u",code);json_t *glyphs=json_object_get(style,"glyphs"),*g=json_object_get(glyphs,key);return g?g:json_object_get(glyphs,"63");}
static float kern(json_t *style,unsigned a,unsigned b){char key[24];snprintf(key,sizeof key,"%u:%u",a,b);return (float)decimal(json_object_get(style,"kern"),key);}
static unsigned decode(const unsigned char **cursor){const unsigned char *p=*cursor;unsigned n=*p++;if(n>=0xc2&&n<=0xdf&&(*p&0xc0)==0x80){n=((n&31)<<6)|(*p++&63);}else if(n>=0x80)n='?';*cursor=p;return n;}
static float line_width(json_t *style,const unsigned *codes,unsigned first,unsigned end){float width=0;for(unsigned i=first;i<end;i++){if(i>first)width+=kern(style,codes[i-1],codes[i]);width+=(float)decimal(glyph(style,codes[i]),"advance");}return width;}
int movie_meta_glyphs(MovieMeta *m,const SWLScene *scene,const char *path,unsigned id,SWLArt *out,unsigned capacity){
 char key[20];snprintf(key,sizeof key,"%u",id);json_t *fonts=json_object_get(m->root,"nativeFonts"),*field=json_object_get(json_object_get(fonts,"fields"),key);
 if(!field)return -1;
 const char *value=json_string_value(json_object_get(m->textValues,path));
 if(!value){const char *key;json_t *v;json_object_foreach(m->textValues,key,v){size_t a=strlen(key),b=strlen(path);if(*key=='.'&&b>=a&&!strcmp(path+b-a,key)){value=json_string_value(v);break;}}}
 if(!value)return -1;
 json_t *style=json_array_get(json_object_get(fonts,"styles"),number(field,"style")),*bounds=json_object_get(field,"bounds");
 if(!field||!style||json_array_size(bounds)!=4)return -1;
 float left=json_number_value(json_array_get(bounds,0)),top=json_number_value(json_array_get(bounds,1)),right=json_number_value(json_array_get(bounds,2)),bottom=json_number_value(json_array_get(bounds,3));
 if(!isfinite(left)||!isfinite(top)||!isfinite(right)||!isfinite(bottom)||right<=left||bottom<=top)return 0;
 json_t *margins=json_object_get(field,"margins");float lm=json_number_value(json_array_get(margins,0)),rm=json_number_value(json_array_get(margins,1)),maximum=right-left-4-lm-rm,size=(float)decimal(field,"size"),height=size*1.15f+(float)decimal(field,"leading");
 if(!isfinite(size)||!isfinite(height)||size<=0||size>512||height<=0)return 0;
 unsigned codes[1024],count=0;const unsigned char *cursor=(const unsigned char *)value;while(*cursor&&count<1024){unsigned c=decode(&cursor);if(c=='\r'){if(*cursor=='\n')cursor++;c='\n';}if(c=='\n'&&!json_is_true(json_object_get(field,"multiline")))c=' ';codes[count++]=c;}
 float pad=(float)decimal(json_object_get(field,"nativeOutline"),"width");if(!isfinite(pad)||pad<0||pad>64)return 0;
 unsigned first=0,line=0,used=0;
 while(first<count){
  unsigned end=first,lastSpace=first;bool wrap=json_is_true(json_object_get(field,"wordWrap"));
  while(end<count&&codes[end]!='\n'){if(codes[end]==' ')lastSpace=end;if(wrap&&maximum>0&&end>first&&line_width(style,codes,first,end+1)>maximum){if(lastSpace>first)end=lastSpace;break;}end++;}
  float width=line_width(style,codes,first,end),x=left+2+lm,y=top+2+size*.9f+line*height;const char *align=string(field,"align");if(!strcmp(align,"right"))x=right-2-rm-width;else if(!strcmp(align,"center"))x=(left+right-width)/2;
  if(y-size>bottom)break;
  float startX=x;for(unsigned pass=0;pass<(pad>0?2u:1u);pass++){x=startX;for(unsigned i=first;i<end;i++){
   if(i>first)x+=kern(style,codes[i-1],codes[i]);
   json_t *g=glyph(style,codes[i]);SWLArt art;
   if(swl_art(scene,number(g,pad>0&&pass==0?"stroke":"art"),&art)){
    float gx=art.left+x,gy=art.top+y,x1=fmaxf(left-pad,gx),y1=fmaxf(top-pad,gy),x2=fminf(right+pad,gx+art.width),y2=fminf(bottom+pad,gy+art.height);
    if(x2>x1&&y2>y1){if(used>=capacity)return (int)capacity+1;float sx=art.w/art.width,sy=art.h/art.height;unsigned a=(unsigned)lroundf((x1-gx)*sx),b=(unsigned)lroundf((y1-gy)*sy),c=(unsigned)lroundf((x2-gx)*sx),d=(unsigned)lroundf((y2-gy)*sy);if(c>a&&d>b){art.x+=a;art.y+=b;art.w=c-a;art.h=d-b;art.left=x1;art.top=y1;art.width=x2-x1;art.height=y2-y1;out[used++]=art;}}
   }
   x+=(float)decimal(g,"advance");
  }}
  first=end;if(first<count&&(codes[first]=='\n'||codes[first]==' '))first++;line++;
 }
 return (int)used;
}
static void parent(char *path){char *last=NULL;for(char *p=path;*p;p++)if(*p=='.'||*p=='#')last=p;if(last)*last=0;else *path=0;}
static bool target(char *out,const char *scope,const char *relative,const char *root){snprintf(out,SWL_PATH,"%s",scope);const char *part=relative;while(*part){const char *end=strchr(part,'.');size_t n=end?(size_t)(end-part):strlen(part);if(n==7&&!memcmp(part,"_parent",7))parent(out);else if(n==5&&!memcmp(part,"_root",5))snprintf(out,SWL_PATH,"%s",root);else if(n&&!(n==4&&!memcmp(part,"this",4))){size_t used=strlen(out);if(used+n+2>=SWL_PATH)return false;if(used)out[used++]='.';memcpy(out+used,part,n);out[used+n]=0;}if(!end)break;part=end+1;}return true;}
static bool run(MovieMeta *m,SWLScene *scene,json_t *commands,const char *scope,const MovieHooks *hooks,void *user){
 for(size_t i=0;i<json_array_size(commands);i++){
  json_t *c=json_array_get(commands,i);unsigned op=number(c,"op");
  if(op==10){if(hooks->buy)hooks->buy(user,number(c,"kind"),number(c,"id"));continue;}
  if(op==11){if(hooks->adopt)hooks->adopt(user,number(c,"id"));continue;}
  if(op==12){if(hooks->close)hooks->close(user);continue;}
  if(op==14){if(hooks->room)hooks->room(user,string(c,"room"));continue;}
  if(op==24)continue;
  char path[SWL_PATH];if(!target(path,scope,string(c,"target"),m->clip))return false;
  if(op==13){unsigned index=0;for(;index<m->nSecrets;index++)if(!strcmp(m->secrets[index].path,path))break;if(index==m->nSecrets){if(index==32)return false;strcpy(m->secrets[m->nSecrets++].path,path);}unsigned count=++m->secrets[index].count;swl_target(scene,path,count%3?2:3,NULL,false);continue;}
  json_t *value=json_object_get(c,"frame");const char *label=json_is_string(value)?json_string_value(value):NULL;unsigned frame=number(c,"frame");
  if(op==3||op==4){frame=swl_frame(scene,path);frame=op==3?frame+1:frame>1?frame-1:1;label=NULL;}
  if(!strcmp(path,m->clip)&&hooks->page){if(label){json_t *labels=json_object_get(m->root,"labels");frame=(unsigned)json_integer_value(json_object_get(labels,label));}if(frame){if(frame>m->frames)frame=m->frames;hooks->page(user,frame);}continue;}
  if(!swl_target(scene,path,frame,label,op==2))return false;
 }
 return true;
}
bool movie_meta_click(MovieMeta *m,SWLScene *scene,float x,float y,const MovieHooks *hooks,void *user){
 if(!m||!scene||!hooks||!isfinite(x)||!isfinite(y))return false;
 unsigned count=swl_hits(scene);
 for(unsigned i=count;i>0;i--){const SWLHit *hit=swl_hit(scene,i-1);if(!hit)continue;json_t *control=NULL;for(size_t j=0;j<json_array_size(m->controls);j++){json_t *c=json_array_get(m->controls,j);if(number(c,"id")==hit->id){control=c;break;}}if(!control)continue;
  SWLInstance instance;if(!swl_instance(scene,hit->path,&instance))continue;json_t *bounds=json_object_get(control,"bounds"),*b=json_array_get(bounds,instance.frame?instance.frame-1:0);if(!json_is_array(b))continue;
  const SWLMatrix *matrix=&hit->matrix;float determinant=matrix->a*matrix->d-matrix->b*matrix->c;if(fabsf(determinant)<1e-8f)continue;float dx=x-matrix->x,dy=y-matrix->y,lx=(matrix->d*dx-matrix->c*dy)/determinant,ly=(-matrix->b*dx+matrix->a*dy)/determinant;
  if(lx<json_number_value(json_array_get(b,0))||ly<json_number_value(json_array_get(b,1))||lx>json_number_value(json_array_get(b,2))||ly>json_number_value(json_array_get(b,3)))continue;
  SWLHit copy=*hit;if(hooks->custom&&hooks->custom(user,copy.id,copy.path))return true;
  const char *scope=NULL;json_t *commands=NULL;
  for(size_t j=0;j<json_array_size(m->assignments);j++){json_t *a=json_array_get(m->assignments,j);if(strcmp(string(a,"path"),copy.path))continue;SWLInstance owner;const char *path=string(a,"scope");if(swl_instance(scene,path,&owner)&&owner.frame==number(a,"frame")&&movie_meta_source(m,owner.symbol)==number(a,"symbol")){scope=path;commands=json_object_get(a,"commands");break;}}
  char buffer[SWL_PATH];if(!commands){strcpy(buffer,copy.path);parent(buffer);scope=buffer;commands=json_object_get(control,"commands");}
  if(json_array_size(commands)){run(m,scene,commands,scope,hooks,user);return true;}
 }
 return false;
}
