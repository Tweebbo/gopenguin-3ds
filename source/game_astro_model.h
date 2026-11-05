#pragma once
#include <stdbool.h>
#include "game_astro_data.h"

#define ASTRO_MAX_SOUNDS 32
#define ASTRO_GUN_Y 459.95

/** Same phases as src/games/astro-model.ts: title, instruction screen, level, level-done clip,
 *  restart clip, secret-plane clip, lose and win frames. */
typedef enum {ASTRO_TITLE,ASTRO_INTRO,ASTRO_PLAYING,ASTRO_DONE,ASTRO_RESTART,ASTRO_SECRET,ASTRO_OVER,ASTRO_WON} AstroPhase;

typedef struct {unsigned clipFrame,wallFrame;bool playing,hit;} AstroPart;
typedef struct {
 const AstroObjectData *data;
 unsigned clipFrame,wallFrame,frame;
 bool playing,hit,wallPlaying;
 double matrix[6];
 AstroPart parts[ASTRO_MAX_PARTS];
} AstroObject;
typedef struct {bool active;double x,y;int rotation;} AstroBullet;

typedef struct {
 AstroPhase phase;
 unsigned level,secretSet,score,prevScore,totalShots,livesCollected,levelsStarted,shotsFrame,hitCounter;
 int lives;
 bool levelRestarted,counted,instructions,left,right,fireHeld;
 double gunX;unsigned gunFrame;
 AstroBullet bullet,turretBullet;
 AstroObject objects[ASTRO_MAX_OBJECTS];
 unsigned objectCount;
 unsigned overlay,introTicks,planeFrame,pendingSecret;
 const char *overlayText;
 const char *sounds[ASTRO_MAX_SOUNDS];
 unsigned soundCount;
 unsigned stamps[12];
 unsigned stampCount;
} AstroModel;

void astro_model_init(AstroModel *model);
/** Title choice: 1 (Enter) or 10/20/30 (keys 1-3). */
void astro_model_start(AstroModel *model,unsigned level);
void astro_model_load(AstroModel *model);
void astro_model_press(AstroModel *model,int direction);
void astro_model_release(AstroModel *model,int direction);
void astro_model_trigger(AstroModel *model,bool held);
void astro_model_confirm(AstroModel *model);
void astro_model_step(AstroModel *model);
bool astro_model_can_fire(const AstroModel *model);
unsigned astro_model_index(const AstroModel *model);
const AstroLevelData *astro_model_level(const AstroModel *model);
const AstroIntroData *astro_model_intro(const AstroModel *model);
/** Frame of sprite 235 to show: 1 title, a level or intro label, 601 lose, 602 win. */
unsigned astro_model_display_frame(const AstroModel *model);
unsigned astro_model_shots(const AstroModel *model);
unsigned astro_model_score(const AstroModel *model);
/** Current wall bounds of a moving target; false for static objects. */
bool astro_model_wall_box(const AstroModel *model,const AstroObject *object,double out[4]);
