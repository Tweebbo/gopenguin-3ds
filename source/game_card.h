#pragma once
#include "game_card_model.h"
#ifdef GP_NINJA_GAMES
/* Read-only state for the isolated native/debug acceptance harness. */
const CardModel *game_card_model(const void *game);
#endif
