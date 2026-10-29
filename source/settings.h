#pragma once
#include "penguin_net.h"
typedef struct {PNetConfig network;char session[160],name[64],assets[192],donate[192];bool audio,batterySaver;} Settings;
bool settings_load(Settings *settings,const char *path);
bool settings_save(const Settings *settings,const char *path);
bool settings_select_account(Settings *settings,const char *session,const char *name,const char *path);
bool settings_remember_name(Settings *settings,const char *name,const char *path);
