#pragma once
#include "game_features.h"
#ifdef GP_NINJA_GAMES
#include "island.h"
#include "swl.h"
typedef struct {unsigned room,frame;const char *key,*name,*path,*game,*controller;float x,y;} RoomNpcSpec;
typedef struct {const RoomNpcSpec *spec;IslandNav *navigation;bool approaching;} RoomNpc;
const RoomNpcSpec *room_npc_spec(unsigned room);
bool room_npc_enter(RoomNpc *npc,unsigned room,SWLScene *scene,const char *folder);
void room_npc_free(RoomNpc *npc);
bool room_npc_click(RoomNpc *npc,int x,int y,float *targetX,float *targetY);
bool room_npc_approaching(RoomNpc *npc,float x,float y,bool walking,bool cancelled);
const char *room_npc_near(const RoomNpc *npc,float x,float y);
const char *room_npc_route(unsigned room,const char *game);
#endif
