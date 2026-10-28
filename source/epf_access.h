#pragma once
#include "island.h"
typedef struct {void (*target)(void *user,const char *path,unsigned frame,bool playing);void *user;} EpfAccessHooks;
typedef struct {
 const IslandNav *navigation;unsigned room,frame,revision;
 bool roleReady,agent,shortcut,open,running;
} EpfAccess;
void epf_access_enter(EpfAccess *access,const IslandNav *navigation,unsigned room,float x,float y,bool roleReady,bool agent,const EpfAccessHooks *hooks);
void epf_access_role(EpfAccess *access,bool ready,bool agent,const EpfAccessHooks *hooks);
void epf_access_tick(EpfAccess *access,const EpfAccessHooks *hooks);
bool epf_access_blocked(const void *access,int x,int y);
bool epf_access_trigger(const EpfAccess *access,int x,int y,IslandTrigger *trigger);
bool epf_access_button(const EpfAccess *access,int x,int y,IslandButton *button);
