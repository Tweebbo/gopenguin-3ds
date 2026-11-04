#pragma once
#include <stdbool.h>
#include "game_aqua_data.h"

typedef enum {AQUA_PLAYING,AQUA_DONE,AQUA_OVER,AQUA_WON} AquaPhase;

typedef struct {
 AquaPhase phase;
 unsigned level,lives,treasure,cargo,claw,score,time,grace,width,height;
 double x,y,air,puffleX,puffleY;
 bool pufflePinched;
 uint16_t map[AQUA_MAX_W*AQUA_MAX_H];
} AquaModel;

void aqua_model_init(AquaModel *model);
void aqua_model_start(AquaModel *model);
void aqua_model_load(AquaModel *model);
void aqua_model_step(AquaModel *model,int dx,int dy,bool action);
void aqua_model_next(AquaModel *model);
const AquaLevelData *aqua_model_level(const AquaModel *model);
unsigned aqua_model_total(const AquaModel *model);
