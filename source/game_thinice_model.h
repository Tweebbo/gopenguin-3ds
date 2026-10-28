#pragma once
// Thin Ice at 24 Hz, ported from the web ThinIceModel (src/games/thinice-model.ts):
// 19 stages from the original level XML, keys, locks, bags, block, warps, secret path. No libctru.
#include <stdbool.h>
#include <stdint.h>

#define THINICE_W 19
#define THINICE_H 15
#define THINICE_MAX_SOUNDS 8
typedef enum {ICE_UP,ICE_DOWN,ICE_LEFT,ICE_RIGHT} IceDirection; // web DELTA key order
typedef enum {ICE_ENTERING,ICE_PLAYING,ICE_SINKING,ICE_WON} IcePhase;
typedef enum {ICE_SOUND_MOVE,ICE_SOUND_ICE,ICE_SOUND_WARP,ICE_SOUND_KEY,ICE_SOUND_MONEY,ICE_SOUND_COMPLETE} IceSound;
typedef struct {int x,y;} IcePoint;
typedef double (*IceRandom)(void *user);
typedef struct {
 unsigned level;uint8_t board[THINICE_H][THINICE_W];
 IcePoint position,previous,start,block,previousBlock,warps[2];bool hasBlock;unsigned warpCount;
 bool warped,blockWarped;IcePhase phase;unsigned phaseTick,steps,blockSteps,keys,tiles,bags,tries,solved,firstTries;
 unsigned savedTiles,savedBags,solvedPoints,firstTryPoints,timePoints,countdown,timer,variant;
 bool lastSolved,sliding,held[4];IceDirection slide;
 IceSound sounds[THINICE_MAX_SOUNDS];unsigned soundCount;
 IceRandom random;void *user;
} ThinIceModel;

void thinice_model_init(ThinIceModel *model,IceRandom random,void *user);
void thinice_model_press(ThinIceModel *model,IceDirection direction);
void thinice_model_release(ThinIceModel *model,IceDirection direction);
void thinice_model_release_all(ThinIceModel *model);
void thinice_model_reset(ThinIceModel *model);
void thinice_model_step(ThinIceModel *model);
unsigned thinice_model_score(const ThinIceModel *model);
unsigned thinice_model_coins(const ThinIceModel *model);
unsigned thinice_model_goal(const ThinIceModel *model);
unsigned thinice_model_tile(const ThinIceModel *model,IcePoint point);
void thinice_model_render_position(const ThinIceModel *model,double alpha,double *x,double *y);
bool thinice_model_render_block(const ThinIceModel *model,double alpha,double *x,double *y);
