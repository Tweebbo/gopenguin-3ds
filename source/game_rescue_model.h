#pragma once
#include <stdbool.h>
#include "game_rescue_data.h"

typedef enum {RESCUE_PLAYING,RESCUE_DONE,RESCUE_WON} RescuePhase;
#define RESCUE_PLATFORMS 16
#define RESCUE_HAZARDS 16

typedef struct {uint8_t x,y,value,phase,range,kind;int offset;} RescueMover;
typedef struct {uint8_t x,y;uint16_t charge;} RescueTurret;
typedef struct {uint8_t x,y;int8_t dx,dy;uint16_t elapsed;} RescueShot;

typedef struct {
 RescuePhase phase;
 unsigned level,carried,rescued,moves,score;
 unsigned width,height,x,y;
 double time;
 unsigned platformCount,hazardCount;
 int riding;
 RescueMover platforms[RESCUE_PLATFORMS],hazards[RESCUE_HAZARDS];
 RescueTurret turrets[16];unsigned turretCount;
 RescueShot shots[16];unsigned shotCount;
 bool collected[RESCUE_PUFFLES];
 uint8_t broken[RESCUE_MAX_W*RESCUE_MAX_H];
 uint16_t crackMs[RESCUE_MAX_W*RESCUE_MAX_H];
 uint16_t map[RESCUE_MAX_W*RESCUE_MAX_H];
 uint8_t parts[RESCUE_MAX_W*RESCUE_MAX_H];
 uint8_t parts2[RESCUE_MAX_W*RESCUE_MAX_H];
} RescueModel;

void rescue_model_init(RescueModel *model);
void rescue_model_start(RescueModel *model);
void rescue_model_load(RescueModel *model);
bool rescue_model_step(RescueModel *model,int dx,int dy);
void rescue_model_next(RescueModel *model);
const RescueLevelData *rescue_model_level(const RescueModel *model);
unsigned rescue_model_total(const RescueModel *model);
bool rescue_model_passable(const RescueModel *model,unsigned x,unsigned y);
void rescue_model_tick(RescueModel *model,double milliseconds);
unsigned rescue_model_platform_count(const RescueModel *model);
bool rescue_model_platform_cell(const RescueModel *model,unsigned index,unsigned *x,unsigned *y,unsigned *frame);
unsigned rescue_model_hazard_count(const RescueModel *model);
bool rescue_model_hazard_cell(const RescueModel *model,unsigned index,unsigned *x,unsigned *y);
