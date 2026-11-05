#pragma once
#include <stdbool.h>
#include "game_dance_data.h"

typedef enum {DANCE_PLAYING,DANCE_RESULT} DancePhase;
typedef struct {
 DancePhase phase;
 unsigned song,difficulty,score,combo,maxCombo,judged,moves;
 double position,duration;
 uint8_t noteState[DANCE_MAX_NOTES];
 uint8_t released[DANCE_MAX_NOTES];
 double pressed[DANCE_MAX_NOTES];
 unsigned counts[6],rating;
 DanceChartData expertChart;
 DanceNote expertNotes[DANCE_MAX_NOTES];
} DanceModel;

void dance_model_init(DanceModel *model);
void dance_model_start(DanceModel *model,unsigned song,unsigned difficulty);
void dance_model_start_seed(DanceModel *model,unsigned song,unsigned difficulty,uint32_t seed);
void dance_model_update(DanceModel *model,double milliseconds);
int dance_model_hit(DanceModel *model,unsigned direction);
void dance_model_release(DanceModel *model,unsigned direction);
const DanceChartData *dance_model_chart(const DanceModel *model);
double dance_model_accuracy(const DanceModel *model);
const char *dance_model_grade(const DanceModel *model);
unsigned dance_model_coins(const DanceModel *model);
double dance_model_audio_offset(const DanceModel *model);
