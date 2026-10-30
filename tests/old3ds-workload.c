#include "../source/demo.h"
#include "../source/render_order.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {RenderOrderItem item;unsigned position;} Expected;
static unsigned seed=7301;
static unsigned random_value(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static int compare(const void *a,const void *b){const Expected *x=a,*y=b;if(x->item.depth<y->item.depth)return -1;if(x->item.depth>y->item.depth)return 1;return x->position<y->position?-1:x->position>y->position;}
static void previous_sort(RenderOrderItem *items,unsigned count){for(unsigned i=1;i<count;i++){unsigned j=i;while(j&&items[j-1].depth>items[j].depth){RenderOrderItem swap=items[j-1];items[j-1]=items[j];items[j]=swap;j--;}}}
int main(void){
 Demo world;demo_init(&world,NULL);world.count=CAST_COUNT;
 for(unsigned i=0;i<CAST_COUNT;i++)world.actors[i].id=1000+i*29;
 demo_index(&world);
 for(unsigned pass=0;pass<1000;pass++){
  unsigned n=random_value()%world.count,id=world.actors[n].id;assert(demo_actor(&world,id)==&world.actors[n]);
  world.actors[n].id=1000000+pass;assert(demo_actor(&world,world.actors[n].id)==&world.actors[n]);assert(demo_actor(&world,id)==NULL);demo_index(&world);
 }
 unsigned removed=world.actors[20].id,moved=world.actors[--world.count].id;world.actors[20]=world.actors[world.count];assert(demo_actor(&world,moved)==&world.actors[20]);assert(demo_actor(&world,removed)==NULL);demo_index(&world);assert(demo_actor(&world,0x7fffff00u)==NULL);
 world.actors[0].id=0x7fffff00u;demo_index(&world);assert(demo_actor(&world,0x7fffff00u)==&world.actors[0]);
 RenderOrderItem items[320],scratch[320];Expected expected[320];
 for(unsigned trial=0;trial<10000;trial++){
  unsigned count=random_value()%300;
  for(unsigned i=0;i<count;i++){RenderOrderItem item={random_value(),(float)(random_value()%40)};items[i]=item;expected[i]=(Expected){item,i};}
  qsort(expected,count,sizeof *expected,compare);render_order_sort(items,count,scratch);
  for(unsigned i=0;i<count;i++){assert(items[i].depth==expected[i].item.depth);assert(items[i].tag==expected[i].item.tag);}
 }
 volatile unsigned checksum=0;const unsigned queries=4000000;clock_t start=clock();
 for(unsigned q=0;q<queries;q++){unsigned id=world.actors[q%world.count].id;for(unsigned i=0;i<world.count;i++)if(world.actors[i].id==id){checksum+=i;break;}}
 double linear=1000.0*(clock()-start)/CLOCKS_PER_SEC;start=clock();
 for(unsigned q=0;q<queries;q++)checksum+=(unsigned)(demo_actor(&world,world.actors[q%world.count].id)-world.actors);
 double indexed=1000.0*(clock()-start)/CLOCKS_PER_SEC;
 RenderOrderItem input[299];for(unsigned i=0;i<299;i++)input[i]=(RenderOrderItem){i,(float)(random_value()%480000)};
 start=clock();for(unsigned n=0;n<10000;n++){memcpy(items,input,sizeof input);previous_sort(items,299);checksum+=items[n%299].tag;}
 double previous=1000.0*(clock()-start)/CLOCKS_PER_SEC;start=clock();
 for(unsigned n=0;n<10000;n++){memcpy(items,input,sizeof input);render_order_sort(items,299,scratch);checksum+=items[n%299].tag;}
 double sorted=1000.0*(clock()-start)/CLOCKS_PER_SEC;
 printf("PASS roster replacements, stale-index fallback, removal/compaction, local NPC IDs and 10000 stable depth-order cases\n");
 printf("Host benchmark only: lookup linear=%.2fms indexed=%.2fms; depth-sort previous=%.2fms optimized=%.2fms; queries=%u checksum=%u extra-index-bytes=%zu\n",linear,indexed,previous,sorted,queries,checksum,sizeof(ActorIndex));return 0;
}
