#pragma once
#include <stdbool.h>
typedef struct {
 unsigned room,requestRoom,visit,requestVisit,earned;float elapsed,pending,requestX,requestY,rewardX,rewardY,rewardTime;
} Mining;
bool mining_eligible(unsigned room,const unsigned look[6],unsigned action,bool moving,float x,float y);
void mining_enter(Mining *mining,unsigned room);
bool mining_update(Mining *mining,float seconds,bool enabled,const unsigned look[6],unsigned action,bool moving,float x,float y);
bool mining_reply(Mining *mining,unsigned earned);
