#pragma once
#define GP_NINJA_GAMES 1
#ifdef GP_DEBUG_GAMES
#define GP_CARD_BANK "games/debug-card.gpk"
#define GP_CARD_ACTORS_BANK "games/debug-card-actors.gpk"
#define GP_FIRE_BANK "games/debug-fire.gpk"
#define GP_CARD_PREVIEW_PREFIX "games/debug-card-preview/"
#define GP_NPC_MINIMAP_FORMAT "debug-mini-low-%s"
#else
#define GP_CARD_BANK "games/card.gpk"
#define GP_CARD_ACTORS_BANK "games/card-actors.gpk"
#define GP_FIRE_BANK "games/fire.gpk"
#define GP_CARD_PREVIEW_PREFIX "games/card-preview/"
#define GP_NPC_MINIMAP_FORMAT "npc-mini-low-%s"
#endif
