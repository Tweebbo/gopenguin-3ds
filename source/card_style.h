#pragma once
#include "common.h"
typedef enum {GP_CARD_BLUE,GP_CARD_GOLD,GP_CARD_RED} GPCardColour;
typedef struct {unsigned id;GPCardColour card,badge;} GPCardStyle;
bool gp_card_styles_parse(const char *json,size_t bytes,GPCardStyle out[64],unsigned *count);
GPCardStyle gp_card_style(unsigned id);
void gp_card_styles_poll(const char *api,bool visible);
bool gp_card_styles_stop_within(unsigned ms);
unsigned gp_card_fill(unsigned id);
unsigned gp_card_ink(unsigned id);
