#pragma once
// Puffle Roundup rules at 24 Hz, ported from the web RoundupModel
// (src/games/roundup-model.ts: Waddle's 2010 rules). No libctru.
#include <stdbool.h>

#define ROUNDUP_HERD 10
typedef struct {unsigned type,direction;double x,y,oldX,oldY;bool moving,captured,escaped;} RoundupPet;
typedef struct {
 double elapsed;unsigned remaining,captured,escaped,coins;bool done;
 RoundupPet pets[ROUNDUP_HERD];
} RoundupModel;

void roundup_model_init(RoundupModel *model,const unsigned types[ROUNDUP_HERD],const double positions[ROUNDUP_HERD][2]);
/** One tick with the herding pointer in game-clip coordinates. */
void roundup_model_step(RoundupModel *model,double mouseX,double mouseY);
/** The original's hit masks, in game-clip coordinates (the clip sits at 80.3, 52 on the stage). */
bool roundup_wall(double x,double y);
bool roundup_pen(double x,double y);
/** A round's herd: level table, then Fisher-Yates with the given uniform source; map index 0-based. */
void roundup_model_herd(double (*random)(void *),void *user,unsigned types[ROUNDUP_HERD],unsigned *map);
