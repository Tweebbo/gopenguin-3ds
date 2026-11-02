#include "frame_hacks.h"
#include "frame_hacks_data.h"
#include "outfit_assets.h"
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

unsigned penguin_outfit_frame(const unsigned look[6],unsigned action){
 unsigned frame=penguin_secret_frame(look,action);if(frame<=30||frame>OUTFIT_MAX_ACTION)return action;
 for(unsigned i=0;i<6;i++)if(look[i]){
  unsigned lo=0,hi=sizeof outfit_items/sizeof *outfit_items;
  while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(outfit_items[mid]<look[i])lo=mid+1;else hi=mid;}
  if(lo==sizeof outfit_items/sizeof *outfit_items||outfit_items[lo]!=look[i])return action;
 }
 return frame;
}
