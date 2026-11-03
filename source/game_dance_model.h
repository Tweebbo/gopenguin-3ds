#pragma once
#include <stdbool.h>
#include "game_dance_data.h"

typedef enum {DANCE_PLAYING,DANCE_RESULT} DancePhase;
typedef struct {
 DancePhase phase;
 unsigned song,difficulty,score,combo,maxCombo,judged,moves;
 double position,duration;
 uint8_t noteState[DANCE_MAX_NOTES];
 unsigned counts[5];
} DanceModel;

void dance_model_init(DanceModel *model);
void dance_model_start(DanceModel *model,unsigned song,unsigned difficulty);
void dance_model_update(DanceModel *model,double milliseconds);
int dance_model_hit(DanceModel *model,unsigned direction);
const DanceChartData *dance_model_chart(const DanceModel *model);
double dance_model_accuracy(const DanceModel *model);
const char *dance_model_grade(const DanceModel *model);
unsigned dance_model_coins(const DanceModel *model);
