#pragma once
#include "swl.h"
/* The original floor swaps three colour panels every 20 frames. Keep its lit
 * first pattern on 3DS; the spotlights, mirror balls and DJ keep their timelines. */
static inline bool room_dance_stable_floor(SWLScene *scene){return swl_target(scene,"background_mc.dancefloor_mc#1",1,NULL,false);}
