#include "sprite.h"
#include "game_port.h"
#include "package.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
 assert(argc>=3);GPPackage package;
 if(package_open(&package,argv[1])){
  const char *error=NULL;GameBank *bank=game_bank_load(argv[1],2u*1024u*1024u,&error);if(!bank)fprintf(stderr,"%s: %s\n",argv[1],error);assert(bank);SWLScene *scene=game_bank_scene(bank);
  for(int i=2;i<argc;i++){assert(swl_target(scene,"root",(unsigned)atoi(argv[i]),NULL,false));for(unsigned t=0;t<3;t++)assert(swl_tick(scene));assert(!swl_error(scene));}
  game_bank_free(bank);return 0;
 }
 Sprite *sprite=sprite_load(argv[1]);assert(sprite);
 for(int i=2;i<argc;i++)for(unsigned variation=1;variation<=sprite_variations(sprite,(unsigned)atoi(argv[i]));variation++){
  const SpriteSequence *pose=sprite_pose(sprite,(unsigned)atoi(argv[i]),variation);assert(pose);assert(pose->count&&pose->loop<pose->count);
  unsigned clocks[]={0,1,pose->count-1,pose->count,pose->count+1000};
  for(unsigned n=0;n<5;n++){const GarmentFrame *frame=sprite_frame(sprite,pose,clocks[n]);assert(frame>=sprite->frames+pose->start&&frame<sprite->frames+pose->start+pose->count);}
  const SpriteTexture *texture=&sprite->textures[pose->page];assert(texture->width*texture->height*4<=2u*1024u*1024u);size_t length;void *bytes=sprite_texture_read(sprite,pose->page,&length);assert(bytes&&length==texture->bytes);free(bytes);
 }
 sprite_free(sprite);return 0;
}
