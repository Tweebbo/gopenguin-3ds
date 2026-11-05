#pragma once
#include <stdbool.h>

/* Catchin' Waves rules (web: src/games/waves-model.ts), riding the original wave face.
 * Stage units (760x480 movie). y is the surfer's depth below the wave crest (0 = the lip,
 * larger = lower on the face, negative = above the crest in the air). The breaking wave
 * ("curl") chases from the left; x is the surfer's stage x. Payout: score / 10, at most 200. */
#define WAVES_MAX_SOUNDS 16
#define WAVES_FACE_TOP 18.0
#define WAVES_FACE_BOTTOM 330.0
typedef enum {WAVES_FREESTYLE,WAVES_SURVIVAL,WAVES_COMPETITION,WAVES_LESSON} WavesMode;
typedef enum {WAVES_PLAYING,WAVES_RESULT} WavesPhase;

typedef struct {
 WavesMode mode;
 WavesPhase phase;
 unsigned wave,lives,combo,maxCombo,soundCount,lastTrick,latch;
 double time,distance,score,trickScore,ride,y,vy,spin,flip,sharkX;
 double x,speed,curl,launchY,wipe,tube,trickShow;
 bool air,success,sharkHit;
 const char *sounds[WAVES_MAX_SOUNDS];
} WavesModel;

void waves_model_init(WavesModel *model,WavesMode mode);
void waves_model_start(WavesModel *model);
bool waves_model_jump(WavesModel *model);
void waves_model_update(WavesModel *model,double milliseconds,bool up,bool down,bool left,bool right);
void waves_model_finish(WavesModel *model,bool success);
/** Landing depth: the face under the surfer (the take-off depth while airborne). */
double waves_model_surface(const WavesModel *model);
/** Fraction of the current wave ridden, 0..1. */
double waves_model_progress(const WavesModel *model);
unsigned waves_model_coins(const WavesModel *model);
