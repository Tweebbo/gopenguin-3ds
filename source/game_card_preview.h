#pragma once
#ifdef GP_NINJA_GAMES
#include "game_controller.h"
typedef struct {unsigned id;char resource[80];} CardPreview;
void card_preview_update(GamePort *port,CardPreview *preview,unsigned id);
void card_preview_stats(GamePort *port,const CardPreview *preview,int value,char colour,bool blocked);
void card_preview_hand(GamePort *port,unsigned slot,int value,char element,char colour,bool focused,bool blocked);
bool card_preview_resource_name(const char *name);
#endif
