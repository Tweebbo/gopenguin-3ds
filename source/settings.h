#pragma once
#include "penguin_net.h"
typedef struct {PNetConfig network;char session[160],name[64],assets[192],donate[192];bool audio;} Settings;
bool settings_load(Settings *settings,const char *path);
bool settings_save(const Settings *settings,const char *path);
