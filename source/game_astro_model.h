#pragma once
#include <stdbool.h>
#include "game_astro_data.h"

#define ASTRO_MAX_SOUNDS 16
#define ASTRO_GUN_Y 459.95

typedef enum {ASTRO_READY,ASTRO_PLAYING,ASTRO_DONE,ASTRO_LOST,ASTRO_OVER,ASTRO_WON} AstroPhase;

typedef struct {
 const AstroObjectData *data;
 unsigned frame;
 bool hit;
} AstroObject;

typedef struct {
 AstroPhase phase;
 unsigned level,score,shots,hits;
 int lives;
 double gunX,bulletY,velocity;
 bool bullet;
 AstroObject objects[ASTRO_MAX_OBJECTS];
 unsigned objectCount;
 const char *sounds[ASTRO_MAX_SOUNDS];
 unsigned soundCount;
} AstroModel;

void astro_model_init(AstroModel *model);
void astro_model_start(AstroModel *model);
void astro_model_load(AstroModel *model);
void astro_model_press(AstroModel *model,int direction);
void astro_model_release(AstroModel *model);
bool astro_model_fire(AstroModel *model);
void astro_model_step(AstroModel *model);
void astro_model_next(AstroModel *model);
void astro_model_restart(AstroModel *model);
const AstroLevelData *astro_model_level(const AstroModel *model);
unsigned astro_model_score(const AstroModel *model);
