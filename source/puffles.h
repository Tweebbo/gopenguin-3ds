#pragma once
#include "penguin_net.h"
#include "demo.h"

#define PUFFLES_OWNED 20
#define PUFFLES_LIMIT (CAST_COUNT+2*PUFFLES_OWNED)
#define PUFFLES_COLOURS 9
typedef enum {PUFFLE_IDLE,PUFFLE_CHECK_NAME,PUFFLE_ADOPT,PUFFLE_WALK,PUFFLE_CARE} PuffleAction;
typedef struct {
 unsigned owner,id,type,health,food,rest,frame,clock,direction,reaction,reactionTicks;
 unsigned long revision;
 char name[49];
 float x,y,oldX,oldY;
 bool walking,following;
 unsigned settled;
} Puffle;
typedef bool (*PuffleSend)(void *user,const char *command,unsigned count,const char *const *args);
typedef struct {unsigned owner,id;unsigned long revision;} PuffleWalk;
typedef struct {
 Puffle pets[PUFFLES_LIMIT];unsigned count,mine,roomOwner;
 PuffleWalk walks[CAST_COUNT+1];unsigned nWalks;
 PuffleSend send;void *user;
 unsigned long revision,epoch,snapshotEpoch,snapshotRevision;
 unsigned snapshotOwner,actionId,actionItem,adoptType,coins;
 bool snapshotPending,coinsChanged;PuffleAction action;
 float accumulator,snapshotAge,actionAge;
 char adoptName[13],error[100];
} Puffles;

void puffles_init(Puffles *puffles,PuffleSend send,void *user);
void puffles_room(Puffles *puffles,unsigned mine,unsigned roomOwner);
bool puffles_request(Puffles *puffles,unsigned owner);
bool puffles_apply(Puffles *puffles,const PNetMessage *message,const Demo *world);
void puffles_step(Puffles *puffles,const Demo *world,float elapsed);
Puffle *puffles_find(Puffles *puffles,unsigned owner,unsigned id);
bool puffles_visible(const Puffles *puffles,const Puffle *puffle,const Demo *world);
void puffles_departed(Puffles *puffles,unsigned owner);
bool puffles_adopt(Puffles *puffles,unsigned type,const char *name);
bool puffles_walk(Puffles *puffles,unsigned id,bool walking);
bool puffles_care(Puffles *puffles,unsigned id,unsigned item);
bool puffles_take_coins(Puffles *puffles,unsigned *coins);
const char *puffles_colour(unsigned type);
