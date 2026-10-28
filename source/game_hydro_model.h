#pragma once
// Hydro-Hopper rules at 24 Hz, ported from the web HydroModel (src/games/hydro.ts).
// Independent of libctru so the host tests can compare it with the TypeScript model.
#include <stdbool.h>
#include <stdint.h>

#define HYDRO_MAX_HAZARDS 64
#define HYDRO_MAX_EVENTS 16

typedef enum {HYDRO_SOUND_JUMP,HYDRO_SOUND_SPLASH,HYDRO_SOUND_SCORE,HYDRO_SOUND_CRASH,HYDRO_SOUND_BONUS} HydroSound;
typedef enum {HYDRO_BOAT_SCORE,HYDRO_BOAT_BUMP} HydroBoat;
typedef struct {unsigned n,x,kind,clip,frame;double y;bool flip,play,scored,extraLife,buoy;} HydroHazard;
typedef double (*HydroRandom)(void *user);
typedef struct {
 unsigned score,level,lives,jumpPoint,missPoint,variant;
 HydroHazard hazards[HYDRO_MAX_HAZARDS];unsigned count;
 unsigned hazardCounter,frameCounter,playerFrame;
 double x,y;int tilex1,tilex2,tiley;
 bool jumping,crashed,invincible;unsigned invincibleCounter;
 unsigned penguinFrame;bool penguinPlaying;
 bool over,levelDone,crashedOnce;double crashX,crashY;unsigned crashes;
 HydroSound sounds[HYDRO_MAX_EVENTS];unsigned soundCount;
 HydroBoat boat[HYDRO_MAX_EVENTS];unsigned boatCount;
 bool overflow;
 HydroRandom random;void *randomUser;
} HydroModel;

void hydro_model_init(HydroModel *model,HydroRandom random,void *user);
/** startGame: keeps score/level/lives; the hand item picks the board (711/712: 20 points a jump). */
void hydro_model_start(HydroModel *model,unsigned hand);
/** updatePlayer: a game-space x (0..400) picks one of the player clip's 41 positions. */
void hydro_model_mouse(HydroModel *model,double gameX);
void hydro_model_jump(HydroModel *model);
void hydro_model_step(HydroModel *model);
void hydro_model_next_level(HydroModel *model);
/** Spawns the next hazard immediately; used by the rule tests to stage collisions. */
void hydro_model_new_hazard(HydroModel *model);
unsigned hydro_model_variant(unsigned hand);
/** Total and concurrent obstacle budget of the current difficulty. */
unsigned hydro_model_total(const HydroModel *model);
unsigned hydro_model_concurrent(const HydroModel *model);
double hydro_model_hazard_y(unsigned clip,unsigned frame);
void hydro_model_player_art(unsigned playerFrame,double *x,double *y);
