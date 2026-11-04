#pragma once
#include "game_mission_flow_data.h"
const MissionData *game_mission_data(void *game);
unsigned game_mission_step(void *game);
unsigned game_mission_order_progress(void *game);
const char *game_mission_code(void *game);
void game_mission_press_order(void *game,unsigned index);
void game_mission_type_code(void *game,char value);
void game_mission_clear_code(void *game);
