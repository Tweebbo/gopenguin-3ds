#include "frame_hacks.h"
#include "frame_hacks_data.h"
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
