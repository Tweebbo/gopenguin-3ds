#pragma once
#include <stdbool.h>
typedef struct {
 void *user;
 unsigned (*frame)(void *user,const char *path);
 void (*target)(void *user,const char *path,unsigned frame,bool playing);
} CaveMineHooks;
bool cave_mine_throw(const CaveMineHooks *hooks,float x,float y);
