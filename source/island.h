#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "island_rooms.h"
typedef struct IslandNav IslandNav;
typedef struct {unsigned room,item,minFrame,maxFrame;float x,y;const char *otherwise,*framePath;unsigned index;} IslandTrigger;
typedef struct {const char *path,*content;float x,y;unsigned firstHover,hovers,index;} IslandButton;
typedef struct {const char *path;unsigned over,out;bool playOver;} IslandHover;
typedef struct {unsigned type,id,min,max,frame;bool playing;const char *guard,*path;} IslandEvent;
IslandNav *island_load(const char *file,const char **error);
IslandNav *island_load_memory(const void *bytes,size_t length,const char **error);
void island_set_id(IslandNav *nav,unsigned id);
void island_free(IslandNav *nav);
const IslandRoom *island_room(unsigned id);
const IslandRoom *island_key(const char *key);
unsigned island_id(const IslandNav *nav);
void island_start(const IslandNav *nav,float *x,float *y);
bool island_blocked(const void *nav,int x,int y);
bool island_valid(const IslandNav *nav,float x,float y);
bool island_trigger(const IslandNav *nav,int x,int y,IslandTrigger *trigger);
bool island_button(const IslandNav *nav,int x,int y,IslandButton *button);
IslandHover island_hover(const IslandNav *nav,unsigned index);
unsigned island_events(const IslandNav *nav);
IslandEvent island_event(const IslandNav *nav,unsigned index);
