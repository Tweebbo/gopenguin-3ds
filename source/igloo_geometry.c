#include "igloo_geometry.h"
#include <stdlib.h>
enum {MASK_BYTES=45600};
void igloo_geometry_free(IglooGeometry *g){if(!g)return;island_free(g->navigation);free(g->floor);free(g->wall);free(g->trash);free(g);}
IglooGeometry *igloo_geometry_load(const MovieMeta *meta,const char **error){
 if(error)*error="Invalid igloo geometry.";
 IglooGeometry *g=calloc(1,sizeof *g);if(!g)return NULL;size_t bytes=0;void *nav=movie_meta_blob(meta,"igloo","navigation",4u*1024u*1024u,&bytes);if(!nav)goto fail;
 g->navigation=island_load_memory(nav,bytes,error);free(nav);if(!g->navigation)goto fail;
 g->floor=movie_meta_blob(meta,"igloo","room_area",MASK_BYTES,&bytes);if(!g->floor||bytes!=MASK_BYTES)goto fail;
 g->wall=movie_meta_blob(meta,"igloo","wall_area",MASK_BYTES,&bytes);if(!g->wall||bytes!=MASK_BYTES)goto fail;
 g->trash=movie_meta_blob(meta,"igloo","trash_area",MASK_BYTES,&bytes);if(!g->trash||bytes!=MASK_BYTES)goto fail;
 g->floorFrame=movie_meta_number(meta,"igloo","floorFrame");if(!g->floorFrame||g->floorFrame>4096)goto fail;
 if(error)*error=NULL;
 return g;
fail:igloo_geometry_free(g);return NULL;
}
static bool hit(const unsigned char *mask,int x,int y){if(!mask||x<0||y<0||x>=760||y>=480)return false;unsigned bit=(unsigned)y*760+(unsigned)x;return (mask[bit>>3]>>(bit&7))&1;}
bool igloo_geometry_legal(const IglooGeometry *g,unsigned type,int x,int y){return g&&x>20&&x<740&&y>20&&y<460&&hit(type==2?g->wall:g->floor,x,y);}
bool igloo_geometry_trash(const IglooGeometry *g,int x,int y){return g&&hit(g->trash,x,y);}
