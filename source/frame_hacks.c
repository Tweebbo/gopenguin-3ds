#include <stdbool.h>
#include "frame_hacks.h"
#include "frame_hacks_data.h"
#include "outfit_assets.h"
#include "outfit_extra_assets.h"
unsigned penguin_secret_frame(const unsigned look[6],unsigned action){
 unsigned lo=0,hi=sizeof frame_hacks/sizeof *frame_hacks;
 while(lo<hi){unsigned middle=lo+(hi-lo)/2;if(frame_hacks[middle][0]<=action)lo=middle+1;else hi=middle;}
 for(unsigned i=lo;i>0&&frame_hacks[i-1][0]==action;i--){
  const unsigned short *h=frame_hacks[i-1];
  unsigned slot=0;while(slot<6&&h[slot+1]==look[slot])slot++;
  if(slot==6)return h[7];
 }
 return action;
}

static bool listed(const unsigned *list,unsigned count,unsigned value){
 unsigned lo=0,hi=count;
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(list[mid]<value)lo=mid+1;else hi=mid;}
 return lo<count&&list[lo]==value;
}
unsigned penguin_outfit_frame(const unsigned look[6],unsigned action){
 unsigned frame=penguin_secret_frame(look,action);if(frame<=30)return action;
 /* Frames 31-76 come from the 2010 penguin; 77-161 from the original per-action penguins (outfit_extra_assets.h). */
 bool extra=frame>OUTFIT_MAX_ACTION;
 if(extra){
  bool known=false;for(unsigned i=0;i<sizeof outfit_extra_actions;i++)if(outfit_extra_actions[i]==frame)known=true;
  if(!known||frame>OUTFIT_EXTRA_MAX_ACTION)return action;
 }
 for(unsigned i=0;i<6;i++)if(look[i]&&!(extra?listed(outfit_extra_items,sizeof outfit_extra_items/sizeof *outfit_extra_items,look[i]):listed(outfit_items,sizeof outfit_items/sizeof *outfit_items,look[i])))return action;
 return frame;
}
unsigned penguin_outfit_avatar(unsigned frame){return frame<=OUTFIT_MAX_ACTION?1:frame<=OUTFIT_EXTRA_AVATAR_SPLIT?OUTFIT_EXTRA_AVATAR:OUTFIT_EXTRA_AVATAR+1;}
unsigned penguin_outfit_item(unsigned item,unsigned frame){return !item?0:frame<=OUTFIT_MAX_ACTION?item:item+OUTFIT_EXTRA_BASE;}
