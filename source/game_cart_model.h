#pragma once
// Cart Surfer at 24 Hz, ported from the web CartModel (src/games/cart-model.ts):
// five tracks, authored trick durations and scoring. No libctru.
#include <stdbool.h>

#define CART_MAX_TRACK 40
typedef enum {CART_LEFT,CART_RIGHT,CART_UP,CART_DOWN,CART_SPACE,CART_NO_KEY} CartKey;
typedef enum {CART_PLAYING,CART_RETRY,CART_LOST,CART_WON} CartPhase;
typedef enum {CART_NONE,CART_SURFING,CART_WHEELIE,CART_JUMPING} CartStance;
typedef enum {CART_NO_MOVE,CART_LEAN,CART_SLIDE,CART_GRIND} CartMove;
typedef enum {CART_NO_TRICK,CART_OLIE,CART_HANDSTAND,CART_BACKFLIP,CART_SPIN,CART_FLAP,CART_SLAM,CART_RUNNING} CartTrick;
typedef struct {
 unsigned score,lives,segment,segmentTick,pose,poseTick,phaseTick;
 CartPhase phase;CartStance stance;CartMove move;CartKey key;
 CartTrick trick,lastTrick;unsigned moveTick,bonus,safe;
 unsigned track[CART_MAX_TRACK],length;
} CartModel;

void cart_model_init(CartModel *model,const unsigned *track,unsigned length);
void cart_model_press(CartModel *model,CartKey key);
void cart_model_release(CartModel *model,CartKey key);
void cart_model_step(CartModel *model);
unsigned cart_model_tile(const CartModel *model);
unsigned cart_model_coins(const CartModel *model);
