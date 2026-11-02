#include "package.h"
#include "movie_meta.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static unsigned bought,lastItem;
static void buy(void *user,unsigned kind,unsigned id){(void)user;assert(kind==0);lastItem=id;bought++;}
int main(void){
 GPPackage package;size_t size;const char *error=NULL;
 assert(package_open(&package,"romfs/books/1030.gpk"));
 void *bytes=package_read(&package,0,SWL_MAX_FILE,&size);assert(bytes);
 SWLScene *scene=swl_load(bytes,size,&error);free(bytes);assert(scene);
 bytes=package_read(&package,1,2*1024*1024,&size);assert(bytes);
 MovieMeta *meta=movie_meta_load(bytes,size);free(bytes);assert(meta);
 MovieHooks hooks={.buy=buy};const float scale=177.f/480;
 for(unsigned tick=0;tick<100;tick++){
  const SWLHit *hit=NULL;
  for(unsigned i=0;i<swl_hits(scene);i++)if(swl_hit(scene,i)->id==639)hit=swl_hit(scene,i);
  assert(hit);SWLMatrix v=hit->matrix;
  float x=v.a*7.5f+v.c*.85f+v.x,y=v.b*7.5f+v.d*.85f+v.y;
  unsigned before=bought;assert(movie_meta_touch(meta,scene,x,y,6/scale,&hooks,NULL));assert(bought==before+1&&lastItem==472);
  // The original hit area misses a stylus tap four display pixels above the button.
  float length=sqrtf(v.c*v.c+v.d*v.d),localY=-17.3f-4/(scale*length);
  x=v.a*7.5f+v.c*localY+v.x;y=v.b*7.5f+v.d*localY+v.y;
  before=bought;assert(!movie_meta_click(meta,scene,x,y,&hooks,NULL));assert(bought==before);
  assert(movie_meta_touch(meta,scene,x,y,6/scale,&hooks,NULL));assert(bought==before+1&&lastItem==472);
  localY=-17.3f-9/(scale*length);x=v.a*7.5f+v.c*localY+v.x;y=v.b*7.5f+v.d*localY+v.y;
  before=bought;movie_meta_touch(meta,scene,x,y,6/scale,&hooks,NULL);assert(bought==before);
  assert(swl_tick(scene));
 }
 assert(bought==200);
 movie_meta_free(meta);swl_free(scene);
 puts("PASS: Bee Antennae centre and four-pixel near misses across 100 frames; distant taps do not buy.");return 0;
}
