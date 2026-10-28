#pragma once
#include <stdbool.h>

#define PIZZ_SAUCE_SLOTS 290
#define PIZZ_TOPPING_SLOTS 299
#define PIZZ_DROP_SLOTS 10
typedef struct {unsigned char sauce,cheese,topping[4];} PizzOrder;
typedef enum {PIZZ_ORDER,PIZZ_PLAY,PIZZ_REMOVE,PIZZ_PLACE} PizzPhase;
typedef double (*PizzRandom)(void *user);
typedef struct {unsigned slot,kind,variant,age;double x,y;bool active;} PizzSplat;
typedef struct {unsigned slot,kind,variant;double x,y;bool active;} PizzTopping;
typedef struct {unsigned kind,variant;double x,y,vx,vy,rotation;bool active;} PizzDrop;
typedef struct {
 double x,y,speed,boost,mouseX,mouseY,strengthX,strengthY,oldX[2],oldY[2];
 PizzPhase phase;unsigned order,made,ordersMade,failures,score,tips,consecutive;
 int handicap;
 bool candy,normal,hot,cheese,perfect,held;
 unsigned counts[4],splatCount,toppingCount,dropCount,serial,ingredient,variant,result,popup,popupSerial,overflow;
 PizzSplat splats[PIZZ_SAUCE_SLOTS];PizzTopping toppings[PIZZ_TOPPING_SLOTS];PizzDrop drops[PIZZ_DROP_SLOTS];
 const char *sounds[4];unsigned soundCount;
 PizzRandom random;void *user;
} PizzModel;

void pizz_model_init(PizzModel *model,bool candy,PizzRandom random,void *user);
void pizz_model_pickup(PizzModel *model,unsigned ingredient);
void pizz_model_step(PizzModel *model);
bool pizz_model_hit(const PizzModel *model,double x,double y);
const PizzOrder *pizz_model_order(const PizzModel *model);
const char *pizz_model_label(const PizzModel *model,unsigned field);
unsigned pizz_model_coins(const PizzModel *model);
