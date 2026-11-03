#pragma once
#include <stdbool.h>
#include "game_jetpack_data.h"

typedef enum {JETPACK_PLAYING,JETPACK_DONE,JETPACK_OVER,JETPACK_WON} JetpackPhase;

typedef struct {
 JetpackPhase phase;
 unsigned level,lives,coins,score,levelScore,grace;
 unsigned width,height;
 double x,y,rotation,upSpeed,fuel;
 bool landed;
 unsigned map[JETPACK_MAX_W*JETPACK_MAX_H];
} JetpackModel;

void jetpack_model_init(JetpackModel *model);
void jetpack_model_start(JetpackModel *model);
void jetpack_model_load(JetpackModel *model);
void jetpack_model_step(JetpackModel *model,bool thrust,bool left,bool right,bool down);
void jetpack_model_next(JetpackModel *model);
void jetpack_model_reset(JetpackModel *model);
const JetpackLevelData *jetpack_model_level(const JetpackModel *model);
unsigned jetpack_model_total(const JetpackModel *model);
