#include "igloo_geometry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
/* Placement helpers on a synthetic igloo: floor is the ellipse below y=260, wall the band 100..200. */
/* The loader is not exercised here; these stubs satisfy igloo_geometry.c. */
IslandNav *island_load_memory(const void *bytes,size_t length,const char **error){(void)bytes;(void)length;(void)error;return NULL;}
void island_free(IslandNav *nav){(void)nav;}
void *movie_meta_blob(const MovieMeta *meta,const char *section,const char *key,size_t limit,size_t *length){(void)meta;(void)section;(void)key;(void)limit;(void)length;return NULL;}
unsigned movie_meta_number(const MovieMeta *meta,const char *section,const char *key){(void)meta;(void)section;(void)key;return 0;}
static void set(unsigned char *m,int x,int y){unsigned bit=(unsigned)y*760+(unsigned)x;m[bit>>3]|=(unsigned char)(1u<<(bit&7));}
int main(void){
 IglooGeometry g={0};g.floor=calloc(1,45600);g.wall=calloc(1,45600);g.trash=calloc(1,45600);assert(g.floor&&g.wall&&g.trash);
 for(int y=0;y<480;y++)for(int x=0;x<760;x++){
  double ex=(x-380)/300.0,ey=(y-360)/90.0;if(ex*ex+ey*ey<=1)set(g.floor,x,y);
  if(y>=100&&y<200&&x>=150&&x<610)set(g.wall,x,y);
 }
 int x,y;
 /* A legal point stays where it is; an illegal one moves to the closest legal point. */
 assert(igloo_geometry_nearest(&g,1,380,360,100,&x,&y)&&x==380&&y==360);
 assert(igloo_geometry_nearest(&g,1,380,220,200,&x,&y)&&igloo_geometry_legal(&g,1,x,y)&&abs(x-380)<=2&&y>=270&&y<=274);
 assert(igloo_geometry_nearest(&g,2,380,40,200,&x,&y)&&igloo_geometry_legal(&g,2,x,y)&&y>=100&&y<=102);
 assert(!igloo_geometry_nearest(&g,2,380,40,20,&x,&y));
 /* Centers lie in the middle of each area, not at the top-left corner of the scan. */
 assert(igloo_geometry_center(&g,1,&x,&y)&&abs(x-380)<8&&abs(y-360)<8);
 assert(igloo_geometry_center(&g,2,&x,&y)&&abs(x-380)<8&&abs(y-150)<8);
 /* Spawning avoids pieces already standing at the anchor. */
 int occupied[]={380,360,380,348};
 assert(igloo_geometry_spawn(&g,1,380,360,NULL,0,&x,&y)&&abs(x-380)<=12&&abs(y-360)<=12);
 assert(igloo_geometry_spawn(&g,1,380,360,occupied,2,&x,&y)&&igloo_geometry_legal(&g,1,x,y));
 for(unsigned n=0;n<2;n++){long dx=x-occupied[2*n],dy=y-occupied[2*n+1];assert(dx*dx+dy*dy>=40*40);}
 assert(igloo_geometry_spawn(&g,2,380,150,NULL,0,&x,&y)&&igloo_geometry_legal(&g,2,x,y));
 free(g.floor);free(g.wall);free(g.trash);puts("igloo-placement: ok");return 0;
}
