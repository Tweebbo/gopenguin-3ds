#pragma once
#include <stdbool.h>

typedef struct {
 bool running,over,won,gray,hookEmpty,isHit,hookFalling,lineCut,hasLostLife,levelBlock,sharkActive,crabActive;
 bool sharkMoving[2],kickerMoving[2];
 int lives;
 unsigned level,fishCaught,orangeFish,grayFish,grayCaught,consecutive,jellyHits,lineCuts,ticks,startTicks,group,iceboxBlock,currentFish;
 double speed,timer,mouseY,hookPrevious,cutPoint,lineExtra,oneupAlpha;
} FishModel;

void fish_model_init(FishModel *model,bool gray,unsigned ticks);
void fish_model_store(FishModel *model,bool gray);
unsigned fish_model_coins(const FishModel *model);
struct GamePort;
const FishModel *fish_game_model(const struct GamePort *port);
