#include "../source/swl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SWL_TEXT_TEST
#include "../source/movie_meta.h"
static MovieMeta *textMeta;
static SWLScene *textScene;
static unsigned textArt[1024],textArtCount;
static int dynamic_text(void *unused,const char *path,unsigned id,SWLArt *out,unsigned capacity){(void)unused;int n=movie_meta_glyphs(textMeta,textScene,path,id,out,capacity);if(n>0&&(unsigned)n<=capacity)for(int i=0;i<n&&textArtCount<1024;i++)textArt[textArtCount++]=out[i].id;return n;}
#endif

enum {W=400,H=240,S=SWL_ATLAS_WIDTH,AH=SWL_ATLAS_HEIGHT};
typedef struct {
 uint8_t *atlas[SWL_MAX_PAGES];float pixels[W*H*4],*groups[8],*masks[8];
 bool painting[8];unsigned group,mask,baseMasks[8],draws;SWLColor colors[8];
} Renderer;
static float clamp(float x){return fmaxf(0,fminf(1,x));}
static float clip(const Renderer *r,unsigned pixel){unsigned base=r->group?r->baseMasks[r->group-1]:0;for(unsigned i=r->mask;i>base;i--)if(!r->painting[i-1])return r->masks[i-1][pixel];return 1;}
static void blend(Renderer *r,unsigned pixel,const float rgba[4]){
 float alpha=rgba[3]*clip(r,pixel);
 for(unsigned i=r->mask;i>0;i--)if(r->painting[i-1]){float *p=&r->masks[i-1][pixel];*p=alpha+*p*(1-alpha);return;}
 float *p=(r->group?r->groups[r->group-1]:r->pixels)+pixel*4;
 for(unsigned c=0;c<3;c++)p[c]=rgba[c]*alpha+p[c]*(1-alpha);
 p[3]=alpha+p[3]*(1-alpha);
}
static bool mask_event(void *user,unsigned event){
 Renderer *r=user;
 if(event==1){if(r->mask>=8)return false;memset(r->masks[r->mask],0,W*H*sizeof(float));r->painting[r->mask++]=true;}
 else if(event==2){if(!r->mask)return false;r->painting[r->mask-1]=false;}
 else if(event==3){if(!r->mask)return false;r->mask--;}
 return true;
}
static bool color_event(void *user,unsigned event,const SWLColor *color){
 Renderer *r=user;
 if(event==1){if(r->group>=8)return false;r->colors[r->group]=*color;r->baseMasks[r->group]=r->mask;memset(r->groups[r->group],0,W*H*4*sizeof(float));r->group++;}
 else if(event==3){if(!r->group)return false;unsigned level=--r->group;const float *source=r->groups[level];const SWLColor *c=&r->colors[level];
  for(unsigned pixel=0;pixel<W*H;pixel++){const float *p=source+pixel*4;if(p[3]<=0)continue;float rgba[4];for(unsigned k=0;k<3;k++)rgba[k]=clamp(p[k]/p[3]*c->mult[k]+c->add[k]/255);rgba[3]=clamp(p[3]*c->mult[3]+c->add[3]/255);blend(r,pixel,rgba);}
 }
 return true;
}
static bool draw(void *user,const SWLArt *art,const SWLMatrix *m,const SWLColor *color){
#ifdef SWL_TEXT_TEST
 if(getenv("TEXT_ONLY")){bool match=false;for(unsigned i=0;i<textArtCount;i++)if(textArt[i]==art->id){match=true;break;}if(!match)return true;}
#endif
 Renderer *r=user;const uint8_t *atlas=r->atlas[art->page];float det=m->a*m->d-m->b*m->c;
 if(fabsf(det)<1e-10f)return true;
 float minx=W,miny=H,maxx=0,maxy=0;
 for(unsigned j=0;j<4;j++){float x=art->left+(j&1?art->width:0),y=art->top+(j&2?art->height:0);float px=m->a*x+m->c*y+m->x,py=m->b*x+m->d*y+m->y;minx=fminf(minx,px);miny=fminf(miny,py);maxx=fmaxf(maxx,px);maxy=fmaxf(maxy,py);}
 int left=(int)fmaxf(0,floorf(minx)),top=(int)fmaxf(0,floorf(miny)),right=(int)fminf(W,ceilf(maxx)),bottom=(int)fminf(H,ceilf(maxy));
 for(int y=top;y<bottom;y++)for(int x=left;x<right;x++){
  float dx=x+.5f-m->x,dy=y+.5f-m->y;
  float lx=(m->d*dx-m->c*dy)/det,ly=(-m->b*dx+m->a*dy)/det;
  float u=(lx-art->left)*art->w/art->width-.5f,v=(ly-art->top)*art->h/art->height-.5f;
  if(u<-.5f||v<-.5f||u>=art->w-.5f||v>=art->h-.5f)continue;
  int ix=(int)floorf(u),iy=(int)floorf(v);float tx=u-ix,ty=v-iy,rgba[4]={0};
  for(unsigned j=0;j<4;j++){
   int sx=ix+(int)(j&1)+(int)art->x,sy=iy+(int)(j>>1)+(int)art->y;if(sx<0||sy<0||sx>=S||sy>=AH)continue;
   const uint8_t *p=atlas+(sy*S+sx)*4;float weight=(j&1?tx:1-tx)*(j&2?ty:1-ty),a=p[3]/255.0f;
   rgba[3]+=a*weight;for(unsigned c=0;c<3;c++)rgba[c]+=p[c]/255.0f*a*weight;
  }
  float a=rgba[3];if(a<=0)continue;
  for(unsigned c=0;c<3;c++)rgba[c]=clamp(rgba[c]/a*color->mult[c]+color->add[c]/255.0f);
  rgba[3]=clamp(a*color->mult[3]+color->add[3]/255.0f);blend(r,(unsigned)(y*W+x),rgba);
 }
 r->draws++;return true;
}
int main(int argc,char **argv){
 if(argc<4){fprintf(stderr,"Usage: swl-render SCENE ATLAS_DIR OUTPUT_DIR [TICKS]\n");return 2;}
 const char *error=NULL;SWLScene *s=swl_load_file(argv[1],&error);if(!s){fprintf(stderr,"%s\n",error);return 1;}
 if(getenv("SWL_RENDER_FRAME")&&!swl_target(s,getenv("SWL_RENDER_ROOT")?getenv("SWL_RENDER_ROOT"):"root",(unsigned)strtoul(getenv("SWL_RENDER_FRAME"),NULL,10),NULL,false))return 1;
#ifdef SWL_TEXT_TEST
 if(argc<6)return 2;
 FILE *metadata=fopen(argv[5],"rb");if(!metadata)return 2;
 if(fseek(metadata,0,SEEK_END))return 2;
 long length=ftell(metadata);if(length<1||length>2*1024*1024||fseek(metadata,0,SEEK_SET))return 2;
 void *bytes=malloc((size_t)length);if(!bytes||fread(bytes,1,(size_t)length,metadata)!=(size_t)length)return 2;
 fclose(metadata);textMeta=movie_meta_load(bytes,(size_t)length);free(bytes);if(!textMeta)return 2;textScene=s;
 movie_meta_text(textMeta,".coinTotal.coins_txt","YOUR COINS: 12345");swl_text_handler(s,dynamic_text,NULL);
#endif
 Renderer *r=calloc(1,sizeof *r);if(!r){swl_free(s);return 1;}int status=1;
 for(unsigned i=0;i<8;i++){r->masks[i]=calloc(W*H,sizeof(float));r->groups[i]=calloc(W*H*4,sizeof(float));if(!r->masks[i]||!r->groups[i])goto done;}
 for(unsigned i=0;i<swl_pages(s);i++){
  char file[1024];snprintf(file,sizeof file,"%s/atlas-%u.rgba",argv[2],i);FILE *f=fopen(file,"rb");r->atlas[i]=malloc(S*AH*4);
  if(!f||!r->atlas[i]){if(f)fclose(f);goto done;}size_t count=fread(r->atlas[i],1,S*AH*4,f);fclose(f);if(count!=S*AH*4)goto done;
 }
 unsigned last=argc>4?(unsigned)strtoul(argv[4],NULL,10):96;if(last>10000)goto done;
 for(unsigned tick=0;tick<=last;tick++){
  memset(r->pixels,0,sizeof r->pixels);r->draws=0;swl_begin(s);
  float scale=getenv("SWL_RENDER_SCALE")?strtof(getenv("SWL_RENDER_SCALE"),NULL):400.0f/760;
  float viewX=getenv("SWL_RENDER_X")?strtof(getenv("SWL_RENDER_X"),NULL):W/2.0f-380*scale,viewY=getenv("SWL_RENDER_Y")?strtof(getenv("SWL_RENDER_Y"),NULL):H/2.0f-240*scale;
  if(!swl_render(s,(SWLMatrix){scale,0,0,scale,viewX,viewY},-1,1,&(SWLRenderer){.emit=draw,.mask=mask_event,.color=color_event},r)){fprintf(stderr,"%s\n",swl_error(s));goto done;}
  if(tick==0||tick==1||tick==12||tick==24||tick==48||tick==96){
   char file[1024];snprintf(file,sizeof file,"%s/native-%u.rgba",argv[3],tick);FILE *f=fopen(file,"wb");if(!f)goto done;
   uint8_t pixels[W*H*4];for(unsigned i=0;i<W*H;i++){float a=r->pixels[i*4+3];pixels[i*4+3]=(uint8_t)lroundf(clamp(a)*255);for(unsigned c=0;c<3;c++)pixels[i*4+c]=(uint8_t)lroundf(clamp(a>0?r->pixels[i*4+c]/a:0)*255);}
   bool ok=fwrite(pixels,1,sizeof pixels,f)==sizeof pixels;fclose(f);if(!ok)goto done;
   printf("tick=%u draws=%u\n",tick,r->draws);
  }
  if(!swl_tick(s)){fprintf(stderr,"%s\n",swl_error(s));goto done;}
 }
 status=0;
done:
 for(unsigned i=0;i<SWL_MAX_PAGES;i++)free(r->atlas[i]);
 for(unsigned i=0;i<8;i++){free(r->masks[i]);free(r->groups[i]);}
 free(r);swl_free(s);
#ifdef SWL_TEXT_TEST
 movie_meta_free(textMeta);
#endif
 return status;
}
