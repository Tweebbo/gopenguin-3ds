#pragma once
// Find Four board helpers shared with the host tests.
#include <stdbool.h>
#define FOUR_W 7
#define FOUR_H 6
/** Waddle's findFourWin (socket-server/world/find-four.ts): anchor x/y and direction 1-4. */
bool four_find_win(const unsigned map[FOUR_W][FOUR_H],int lastX,int lastY,unsigned *x,unsigned *y,unsigned *d);
