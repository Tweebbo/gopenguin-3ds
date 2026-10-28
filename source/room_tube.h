#pragma once
#include <stdbool.h>

typedef struct {unsigned room;int x,y;} RoomTubeDestination;
typedef struct {
 unsigned (*frame)(void *user,const char *path);
 void (*target)(void *user,const char *path,unsigned frame,const char *label,bool playing);
 void *user;
} RoomTubeHooks;
typedef struct {
 unsigned room,frame,colour;bool inside,blocked,active,ready,taken,blur;
} RoomTube;

void room_tube_enter(RoomTube *tube,unsigned room);
bool room_tube_supported(unsigned room);
/* Observe only the local actor; other player IDs never change this sequence. */
bool room_tube_observe(RoomTube *tube,unsigned player,unsigned mine,bool inside,unsigned colour,const RoomTubeHooks *hooks);
void room_tube_tick(RoomTube *tube,const RoomTubeHooks *hooks);
void room_tube_cancel(RoomTube *tube,const RoomTubeHooks *hooks);
bool room_tube_locked(const RoomTube *tube);
unsigned room_tube_frame(const RoomTube *tube);
bool room_tube_take(RoomTube *tube,RoomTubeDestination *destination);
