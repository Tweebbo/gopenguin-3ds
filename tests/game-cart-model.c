// Host test for the Cart Surfer model: held directions after tricks and overlapping keys.
// Build: cc -std=c11 -Wall -Wextra -Werror -Isource tests/game-cart-model.c source/game_cart_model.c -o /tmp/x && /tmp/x
#include "game_cart_model.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c);exit(1);}}while(0)

static const unsigned STRAIGHT[]={1,1,1,1,1,1,1,1,6};
static const unsigned CORNER[]={1,2,1,1,1,1,6}; /* tile 2: a corner that needs Right held */

int main(void){
 CartModel m;
 /* Left held, Up tapped (D-pad diagonal), Left released: the lean ends, no timeout crash. */
 cart_model_init(&m,STRAIGHT,9);
 cart_model_press(&m,CART_LEFT);CHECK(m.move==CART_LEAN);
 cart_model_press(&m,CART_UP);CHECK(m.key==CART_UP);
 cart_model_release(&m,CART_LEFT);CHECK(m.move==CART_NO_MOVE);
 for(int i=0;i<60;i++)cart_model_step(&m);
 CHECK(m.lives==4&&m.phase==CART_PLAYING);
 cart_model_release(&m,CART_UP);CHECK(m.key==CART_NO_KEY);

 /* Owner release keeps the original behaviour. */
 cart_model_init(&m,STRAIGHT,9);
 cart_model_press(&m,CART_RIGHT);cart_model_release(&m,CART_RIGHT);
 CHECK(m.move==CART_NO_MOVE&&m.key==CART_NO_KEY&&m.pose==1);

 /* Right pressed during a jump trick is ignored by the model; the held key applies after the trick. */
 cart_model_init(&m,CORNER,7);
 for(int i=0;i<30;i++)cart_model_step(&m); /* intro pose ends */
 cart_model_press(&m,CART_SPACE);cart_model_release(&m,CART_SPACE);CHECK(m.stance==CART_JUMPING);
 cart_model_press(&m,CART_UP);CHECK(m.trick==CART_FLAP);
 cart_model_press(&m,CART_RIGHT);CHECK(m.key!=CART_RIGHT); /* dropped during the trick */
 bool reached=false;
 for(int i=0;i<200&&m.phase==CART_PLAYING&&!reached;i++){
  cart_repeat_direction(&m,false,true);
  cart_model_step(&m);
  if(m.segment>=2)reached=true;
 }
 CHECK(reached);CHECK(m.lives==4);
 CHECK(m.key==CART_RIGHT);

 /* Without the repeat the same input crashes in the corner (the reported bug). */
 cart_model_init(&m,CORNER,7);
 for(int i=0;i<30;i++)cart_model_step(&m);
 cart_model_press(&m,CART_SPACE);cart_model_release(&m,CART_SPACE);
 cart_model_press(&m,CART_UP);cart_model_press(&m,CART_RIGHT);
 for(int i=0;i<200&&m.phase==CART_PLAYING&&m.segment<2;i++)cart_model_step(&m);
 CHECK(m.lives==3);

 /* Both directions held: no repeat. */
 cart_model_init(&m,STRAIGHT,9);
 cart_repeat_direction(&m,true,true);CHECK(m.key==CART_NO_KEY);
 puts("game-cart-model: PASS");
 return 0;
}
