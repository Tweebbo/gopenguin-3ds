#include "game_fish_model.h"
#include <string.h>

void fish_model_init(FishModel *m,bool gray,unsigned ticks){memset(m,0,sizeof *m);m->running=true;m->gray=gray;m->lives=3;m->level=1;m->hookEmpty=true;m->speed=4000;m->mouseY=240;m->hookPrevious=240;m->lineExtra=gray?35:0;m->oneupAlpha=100;m->ticks=m->startTicks=ticks;m->timer=ticks*(1000.0/24);}
void fish_model_store(FishModel *m,bool gray){
 static const unsigned capacity[5]={6,8,6,16,18};
 if(m->iceboxBlock<5&&m->currentFish==capacity[m->iceboxBlock]){m->currentFish=1;m->iceboxBlock++;}else m->currentFish++;
 m->fishCaught++;m->consecutive++;if(gray){m->grayFish++;m->grayCaught++;}else m->orangeFish++;
}
unsigned fish_model_coins(const FishModel *m){return m->orangeFish*4+m->grayFish*8+(m->won?100:0);}
