#include "mining.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 unsigned look[6]={429};Mining m={0};mining_enter(&m,813);
 assert(mining_eligible(813,look,26,false,540,400));
 assert(!mining_eligible(813,look,25,false,540,400));assert(!mining_eligible(813,look,26,true,540,400));assert(!mining_eligible(100,look,26,false,540,400));assert(!mining_eligible(813,look,26,false,NAN,400));
 for(unsigned i=0;i<8;i++)assert(!mining_update(&m,.25,true,look,26,false,540,400));
 assert(mining_update(&m,.2,true,look,26,false,540,400));
 for(unsigned i=0;i<8;i++)assert(!mining_update(&m,.25,true,look,26,false,540,400));
 assert(mining_reply(&m,10));assert(m.earned==10&&m.rewardX==540&&m.rewardTime>0);assert(!mining_reply(&m,10));
 mining_enter(&m,813);for(unsigned i=0;i<9;i++)mining_update(&m,.25,true,look,26,false,540,400);
 assert(m.pending>0);
 mining_enter(&m,100);mining_enter(&m,813);assert(!mining_reply(&m,100));assert(!m.rewardTime);
 for(unsigned i=0;i<9;i++)mining_update(&m,.25,true,look,26,false,540,400);
 assert(mining_reply(&m,100));assert(m.earned==100);
 for(unsigned i=0;i<6;i++)mining_update(&m,.25,true,look,26,false,540,400);
 mining_update(&m,.1,true,look,25,false,540,400);assert(m.elapsed==0);
 for(unsigned i=0;i<8;i++)assert(!mining_update(&m,.25,true,look,26,false,540,400));
 assert(mining_update(&m,.2,true,look,26,false,540,400));assert(!mining_reply(&m,0));assert(m.pending==0);
 look[3]=240;assert(!mining_eligible(813,look,26,false,540,400));
 puts("PASS mining: exact outfit/area/action, timing, one pending request, authoritative 10/100 feedback, zero reward, interruptions and stale room visits");return 0;
}
