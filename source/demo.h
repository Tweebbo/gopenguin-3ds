#pragma once
#include <stdbool.h>
#include <stdint.h>
#define ROOM_W 760
#define ROOM_H 480
#define CAST_COUNT 80
typedef struct {float x,y,tx,ty,clock,bubbleTime;int direction,action,colour;unsigned id,look[9],emote,chatOffset,pendingFrame;char name[49],chat[512];bool walking,member;} Actor;
typedef struct {Actor actors[CAST_COUNT];unsigned count;uint8_t *mask;bool (*blocked)(const void *,int,int);const void *room;float time,bubbleTime,ballTime,ballX,ballY,ballTX,ballTY;int tab,phrase;bool zoom,stats;} Demo;
void demo_init(Demo *d,uint8_t *mask);
bool demo_valid(const Demo *d,float x,float y);
void demo_target(Demo *d,float x,float y);
void demo_action(Demo *d,int action);
void demo_step(Demo *d,float dt,float dx,float dy);
void demo_say(Demo *d,int phrase);
void demo_throw(Demo *d);
void actor_say(Actor *actor,const char *text);
