#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "overlay.h"

int main(void) {
  Overlay overlay;
  memset(&overlay, 0, sizeof overlay);

  for (unsigned i = 0; i < 20; i++) overlay_throw(&overlay, 0, 0, 100, 100);
  unsigned active = 0;
  for (unsigned i = 0; i < SNOWBALL_LIMIT; i++) active += overlay.balls[i].active;
  assert(SNOWBALL_LIMIT == 11);
  assert(active == SNOWBALL_LIMIT);

  memset(&overlay, 0, sizeof overlay);
  overlay_throw(&overlay, 10, 20, 110, 80);
  float x = 0, y = 0;
  for (unsigned i = 0; i < 20; i++) overlay_step(&overlay, 1.0f / 24.0f);
  assert(!overlay_ball(&overlay.balls[0], &x, &y));
  assert(!overlay_shadow(&overlay.balls[0], &x, &y));
  assert(!overlay_impact(&overlay.balls[0], &x, &y));

  for (unsigned i = 0; i < 6; i++) overlay_step(&overlay, 1.0f / 24.0f);
  assert(overlay_ball(&overlay.balls[0], &x, &y));
  assert(overlay_shadow(&overlay.balls[0], &x, &y));
  assert(!overlay_impact(&overlay.balls[0], &x, &y));

  for (unsigned i = 0; i < 2; i++) overlay_step(&overlay, 1.0f / 24.0f);
  assert(overlay_impact(&overlay.balls[0], &x, &y));
  assert(fabsf(x - 110) < 0.01f && fabsf(y - 80) < 0.01f);
  assert(!overlay_ball(&overlay.balls[0], &x, &y));

  for (unsigned i = 0; i < 4; i++) overlay_step(&overlay, 1.0f / 24.0f);
  assert(!overlay_impact(&overlay.balls[0], &x, &y));
  assert(!overlay.balls[0].active);

  puts("PASS snowball overlay: cap, shadow, impact and expiry");
  return 0;
}
