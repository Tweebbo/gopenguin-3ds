#pragma once
#include <stdint.h>
typedef struct {uint16_t id;uint8_t category;const char *subject;} PostcardInfo;
static const PostcardInfo postcardInfo[]={
 {47,1,"EPF Recruitment"},
 {217,2,"Let's Explore!"},
 {218,2,"My Stamp Book"},
 {37,2,"Turn Green"},
 {3,2,"Penguins Time Forgot"},
 {78,2,"Happy New Year"},
 {3,3,"Penguins Time Forgot"},
 {35,3,"Long Time No See"},
 {37,3,"Turn Green"},
 {56,3,"Hey There"},
 {88,3,"My Tour"},
 {99,3,"Find the Pin"},
 {200,3,"Campfire Story"},
 {202,3,"Check out my igloo"},
 {206,3,"Stamp Book"},
 {2,4,"Puffle Performance"},
 {43,4,"Happy Puffles"},
 {44,4,"Best Friends"},
 {45,4,"Cool Puffles"},
 {1,5,"Re-decorated Igloo"},
 {17,5,"Cool Igloo"},
 {27,5,"Party at my Igloo"},
 {61,5,"Igloo Party"},
 {78,5,"Happy New Year"},
 {202,5,"Check out my igloo"},
 {10,6,"Mancala"},
 {19,6,"Great Game"},
 {34,6,"Puffle Rescue"},
 {51,6,"Jet Pack"},
 {52,6,"Ski Hill"},
 {75,6,"Card-Jitsu"},
 {5,7,"Pizza Parlor"},
 {20,7,"Snow Forts"},
 {29,7,"Tip The Berg"},
 {49,7,"Dance Party"},
 {53,7,"Lighthouse"},
 {55,7,"Dojo"},
};
#define POSTCARD_COUNT 37
typedef struct {uint16_t first;uint8_t count,category;} PostcardPage;
static const PostcardPage postcardPages[]={
 {1,5,2},
 {6,6,3},
 {12,3,3},
 {15,4,4},
 {19,6,5},
 {25,6,6},
 {31,6,7},
};
#define POSTCARD_PAGES 7
