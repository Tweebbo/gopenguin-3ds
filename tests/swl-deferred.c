#include "game_port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void hash_bytes(uint64_t *hash,const void *data,size_t size){const unsigned char *p=data;for(size_t i=0;i<size;i++)*hash=(*hash^p[i])*1099511628211ull;}
static bool emit(void *u,const SWLArt *a,const SWLMatrix *m,const SWLColor *c){uint64_t *h=u;hash_bytes(h,&a->id,sizeof a->id);hash_bytes(h,&a->left,4*sizeof(float));hash_bytes(h,m,sizeof *m);hash_bytes(h,c,sizeof *c);return true;}
static bool mask(void *u,unsigned e){hash_bytes(u,&e,sizeof e);return true;}
static uint64_t render(SWLScene *s){uint64_t h=14695981039346656037ull;assert(swl_render(s,(SWLMatrix){.5f,0,0,.5f,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&h));return h;}
int main(void){
 const char *error=NULL;GameBank *banks[2];SWLScene *scenes[2];
 for(unsigned i=0;i<2;i++){banks[i]=game_bank_load("romfs/games/pizzatron.gpk",GAME_PORT_TEXTURE_LIMIT,&error);assert(banks[i]);scenes[i]=game_bank_scene(banks[i]);assert(swl_target(scenes[i],"root",3,NULL,false));}
 for(unsigned tick=0;tick<120;tick++){
  assert(swl_tick(scenes[0]));assert(swl_tick_deferred(scenes[1]));
  for(unsigned i=0;i<2;i++){
   assert(swl_transform(scenes[i],"root.pizza",(SWLMatrix){1,0,0,1,100+tick,375}));
   assert(swl_target(scenes[i],"root.hand",2+tick%4,NULL,false));
   assert(swl_target(scenes[i],"root.pizza.base",1+tick%3,NULL,false));
  }
  assert(render(scenes[0])==render(scenes[1]));
  for(unsigned i=0;i<2;i++)assert(swl_view_frame(scenes[i],"root",8));
  assert(render(scenes[0])==render(scenes[1]));
  for(unsigned i=0;i<2;i++)assert(swl_view_frame(scenes[i],"root",0));
 }
 for(unsigned i=0;i<2;i++)game_bank_free(banks[i]);
 puts("PASS: eager/deferred timeline updates render identical original Pizzatron art, transforms, colours and masks on both screens for 120 frames.");return 0;
}
