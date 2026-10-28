#pragma once
// Bean Counters classic mode at 24 Hz: the web BeansModel (src/games/beans-model.ts)
// plus the collision, spill and notice timing of Beans.tick() (src/games/beans.ts),
// with collision boxes precomputed from the original clips. No libctru.
#include <stdbool.h>

#define BEANS_MAX_FLIGHTS 8
#define BEANS_MAX_SPILLS 16
#define BEANS_MAX_SOUNDS 16
typedef enum {BEANS_PLAYING,BEANS_RETRY,BEANS_TRUCK,BEANS_WON,BEANS_LOST} BeansPhase;
typedef struct {unsigned kind,age,id;} BeansFlight;
typedef struct {unsigned kind,id,age,length;} BeansSpill;
typedef struct {const char *name;unsigned volume;} BeansSound;
typedef double (*BeansRandom)(void *user);
typedef struct {
 double x,oldX;unsigned held,placed,score,level,pose;int lives;
 BeansPhase phase;unsigned phaseTick;
 BeansFlight flights[BEANS_MAX_FLIGHTS];unsigned count;
 BeansSpill spills[BEANS_MAX_SPILLS];unsigned spillCount;
 unsigned impact,notify;double impactX,notifyX;
 BeansSound sounds[BEANS_MAX_SOUNDS];unsigned soundCount;
 unsigned serial,fish,bags[20],bagCount;bool oneUp,overflow;
 BeansRandom random;void *user;
} BeansModel;

void beans_model_init(BeansModel *model,BeansRandom random,void *user);
/** BeansModel.step: launches and phase timers. */
void beans_model_step(BeansModel *model,double x);
/** Beans.tick: step, spills, notices and collisions against the penguin. */
void beans_model_tick(BeansModel *model,double x);
void beans_model_catch(BeansModel *model,unsigned id);
/** onBagStack: a click or button press near the truck drops a held bag. */
void beans_model_drop(BeansModel *model);
unsigned beans_model_coins(const BeansModel *model);
/** The score sent with zo: Waddle divides by 10; a won game adds the 60-coin bonus. */
unsigned beans_model_payout(const BeansModel *model);
