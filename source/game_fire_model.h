#pragma once
#include <stdbool.h>
#define FIRE_PLAYERS 4u
#define FIRE_HAND 5u
#define FIRE_DEFERRED 16u
typedef enum {FIRE_MENU,FIRE_QUEUE,FIRE_HANDSHAKE,FIRE_BOARD,FIRE_SPIN_WAIT,FIRE_MOVE,FIRE_TRUMP,FIRE_OPPONENT,FIRE_SELECT,FIRE_WAIT,FIRE_REVEAL,FIRE_OVER,FIRE_ERROR} FirePhase;
typedef struct {unsigned count;char args[10][160];} FireDeferred;
typedef struct {
 FirePhase phase;int seat;unsigned players,active,round,mode,rank,percentage;
 unsigned colour[4],energy[4],position[4],spin[3],hand[5],played[4],result[4],standing[4];
 char names[4][40],trump;bool participants[4],opponents[4];
 bool menuReady,started,requestGet,requestReady,readySent,inputPending,rankAwarded;
 unsigned deferredCount;FireDeferred deferred[FIRE_DEFERRED];
 float elapsed;char message[160];
} FireModel;
void fire_model_init(FireModel *model);
void fire_model_begin(FireModel *model,bool sensei);
bool fire_model_packet(FireModel *model,const char *command,unsigned count,char *const *args);
bool fire_model_playable(const FireModel *model,unsigned slot);
bool fire_model_destination(const FireModel *model,unsigned tile);
