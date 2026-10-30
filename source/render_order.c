#include "render_order.h"
#include <string.h>
void render_order_sort(RenderOrderItem *items,unsigned count,RenderOrderItem *scratch){
 unsigned ordered=1;while(ordered<count&&items[ordered-1].depth<=items[ordered].depth)ordered++;if(ordered==count||count<2)return;
 if(count<24){for(unsigned i=1;i<count;i++){RenderOrderItem value=items[i];unsigned j=i;while(j&&items[j-1].depth>value.depth){items[j]=items[j-1];j--;}items[j]=value;}return;}
 RenderOrderItem *source=items,*target=scratch;
 for(unsigned width=1;width<count;width*=2){
  for(unsigned base=0;base<count;base+=2*width){
   unsigned middle=base+width<count?base+width:count,end=base+2*width<count?base+2*width:count,left=base,right=middle;
   for(unsigned out=base;out<end;out++)target[out]=left<middle&&(right>=end||source[left].depth<=source[right].depth)?source[left++]:source[right++];
  }
  RenderOrderItem *swap=source;source=target;target=swap;
 }
 if(source!=items)memcpy(items,source,count*sizeof *items);
}
