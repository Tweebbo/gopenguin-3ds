#include "frame_hacks.h"
#include <stdio.h>
int main(void){unsigned a,look[6];while(scanf("%u %u %u %u %u %u %u",&a,&look[0],&look[1],&look[2],&look[3],&look[4],&look[5])==7)printf("%u %u\n",penguin_secret_frame(look,a),penguin_outfit_frame(look,a));return 0;}
