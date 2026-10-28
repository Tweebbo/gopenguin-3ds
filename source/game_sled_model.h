#pragma once
// Sled Racing at 24 Hz, ported from the web SledModel (src/games/sled-model.ts):
// SledRacer.swf's movePlayer, the key listener and handleSendMove. No libctru.
#include <stdbool.h>

#define SLED_RACERS 4
#define SLED_MAX_SENDS 16
#define SLED_MAP_ROWS 1000
typedef struct {char name[40];bool named,finished,stopped,crashed,jumping;unsigned colour,sled,art;double s,h,m,x,x2,y,ground;int row;} SledRacer;
/** zm codes: 0 finished, 1 crashed, 2 jumped, x+100 moved, speed+400 boosted. */
typedef struct {double code,y,time;} SledSend;
typedef struct {
 const signed char (*map)[10];unsigned mapRows;const unsigned char *hill;unsigned hillLength,racerCount;
 SledRacer racers[SLED_RACERS];int me;double gameTime,currentX;bool started,over;unsigned place;
 char finish[SLED_RACERS][40];unsigned finished;
 SledSend sends[SLED_MAX_SENDS];unsigned sendCount;
 double speeds[64];unsigned speedCount;
 signed char rows[SLED_MAP_ROWS][10];
} SledModel;

void sled_model_init(SledModel *model,unsigned waddle);
/** handleUpdateGame: "name|colour|hand|username" per seat. */
void sled_model_players(SledModel *model,unsigned count,const char *const *players,const char *username);
void sled_model_turn(SledModel *model,int direction);
void sled_model_step(SledModel *model);
void sled_model_remote(SledModel *model,int id,double code,double y,double time);
unsigned sled_model_sled(unsigned hand);
