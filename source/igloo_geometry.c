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
bool igloo_geometry_nearest(const IglooGeometry *g,unsigned type,int x,int y,int radius,int *outX,int *outY){
 if(!g)return false;
 if(igloo_geometry_legal(g,type,x,y)){*outX=x;*outY=y;return true;}
 // Square rings of growing size; within a ring keep the point closest to (x,y).
 for(int r=2;r<=radius;r+=2){
  long best=-1;int bx=0,by=0;
  for(int i=-r;i<=r;i+=2){
   const int points[4][2]={{x+i,y-r},{x+i,y+r},{x-r,y+i},{x+r,y+i}};
   for(unsigned p=0;p<4;p++){int px=points[p][0],py=points[p][1];if(!igloo_geometry_legal(g,type,px,py))continue;long d=(long)(px-x)*(px-x)+(long)(py-y)*(py-y);if(best<0||d<best){best=d;bx=px;by=py;}}
  }
  if(best>=0){*outX=bx;*outY=by;return true;}
 }
 return false;
}
bool igloo_geometry_center(const IglooGeometry *g,unsigned type,int *outX,int *outY){
 if(!g)return false;
 long sx=0,sy=0,n=0;
 for(int y=24;y<460;y+=4)for(int x=24;x<740;x+=4)if(igloo_geometry_legal(g,type,x,y)){sx+=x;sy+=y;n++;}
 if(!n)return false;
 return igloo_geometry_nearest(g,type,(int)(sx/n),(int)(sy/n),760,outX,outY);
}
bool igloo_geometry_spawn(const IglooGeometry *g,unsigned type,int ax,int ay,const int *occupied,unsigned count,int *outX,int *outY){
 if(!g)return false;
 // Score every legal grid point: distance to the anchor plus a penalty for each nearby piece.
 long best=-1;int bx=0,by=0;
 for(int y=24;y<460;y+=12)for(int x=24;x<740;x+=12){
  if(!igloo_geometry_legal(g,type,x,y))continue;
  long score=(long)(x-ax)*(x-ax)+(long)(y-ay)*(y-ay);
  for(unsigned n=0;n<count;n++){long dx=x-occupied[2*n],dy=y-occupied[2*n+1],d=dx*dx+dy*dy;if(d<60*60)score+=(60L*60L-d)*8;}
  if(best<0||score<best){best=score;bx=x;by=y;}
 }
 if(best<0)return false;
 *outX=bx;*outY=by;return true;
}
