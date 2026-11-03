#pragma once
#include <stdbool.h>

#define WAVES_MAX_SOUNDS 16
typedef enum {WAVES_FREESTYLE,WAVES_SURVIVAL,WAVES_COMPETITION,WAVES_LESSON} WavesMode;
typedef enum {WAVES_PLAYING,WAVES_RESULT} WavesPhase;

typedef struct {
 WavesMode mode;
 WavesPhase phase;
 unsigned wave,lives,combo,maxCombo,soundCount;
 double time,distance,score,trickScore,ride,y,vy,spin,flip,sharkX;
 bool air,success;
 const char *sounds[WAVES_MAX_SOUNDS];
} WavesModel;

void waves_model_init(WavesModel *model,WavesMode mode);
void waves_model_start(WavesModel *model);
bool waves_model_jump(WavesModel *model);
void waves_model_update(WavesModel *model,double milliseconds,bool up,bool down,bool left,bool right);
void waves_model_finish(WavesModel *model,bool success);
double waves_model_surface(const WavesModel *model);
unsigned waves_model_coins(const WavesModel *model);
