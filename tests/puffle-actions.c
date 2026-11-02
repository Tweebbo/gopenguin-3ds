#include "../source/puffles.h"
#include "../source/puffle_actions.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool loaded;
static bool pose_ready(const Puffle *pet,unsigned frame){(void)pet;(void)frame;return loaded;}
int main(void){
 Puffles pets;Demo world={0};const unsigned items[]={27,37,3,4,5,8};
 for(unsigned type=0;type<9;type++)for(unsigned action=0;action<6;action++){
  puffles_init(&pets,NULL,NULL);puffles_room(&pets,10,10);
  char row[100],item[10];snprintf(row,sizeof row,"1|Care|%u|100|100|100|100|100|100|0|0|0|0",type);snprintf(item,sizeof item,"%u",items[action]);
  char *args[]={"10",row,item};PNetMessage message={.count=3,.args=args};strcpy(message.command,"pcare");assert(puffles_apply(&pets,&message,&world));
  Puffle *pet=puffles_find(&pets,10,1);assert(pet&&pet->clock==0&&pet->frame==puffle_action_frames[action]);
  pet->frame=1;pet->clock=0;pets.poseReady=pose_ready;loaded=false;
  for(unsigned tick=0;tick<500;tick++)puffles_step(&pets,&world,1.f/24);
  assert(pet->frame==1&&pet->reactionTicks==0&&pet->reaction==action+1);loaded=true;puffles_step(&pets,&world,1.f/24);
  for(unsigned tick=2;tick<=puffle_action_lengths[type][action];tick++){
   puffles_step(&pets,&world,1.f/24);assert(pet->reaction==action+1&&pet->frame==puffle_action_frames[action]&&pet->clock==tick);
  }
  assert(puffles_apply(&pets,&message,&world));assert(pet->clock==0&&pet->reactionTicks==0);
  for(unsigned tick=0;tick<=puffle_action_lengths[type][action];tick++)puffles_step(&pets,&world,1.f/24);
  assert(!pet->reaction&&pet->frame==pet->direction);
 }
 puts("PASS 54 native care actions: original pose, full duration, restart and return to idle");return 0;
}
