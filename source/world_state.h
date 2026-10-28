#pragma once
#include "demo.h"
#include "penguin_net.h"
unsigned world_apply(Demo *world,const PNetMessage *message,unsigned mine);
Actor *world_actor(Demo *world,unsigned id);
bool world_penguin(const char *text,Actor *actor);
