#include "game_cart_model.h"
#include <string.h>

static const unsigned SCORES[]={0,20,80,100,80,50,30,80}; // by CartTrick
static unsigned end(unsigned pose){switch(pose){case 6:return 20;case 11:return 25;case 15:case 16:case 18:return 17;case 17:return 27;case 19:return 7;case 20:return 22;default:return 0;}}

void cart_model_init(CartModel *m,const unsigned *track,unsigned length){
 memset(m,0,sizeof *m);m->lives=4;m->segmentTick=1;m->pose=24;m->poseTick=1;m->key=CART_NO_KEY;
 m->length=length>CART_MAX_TRACK?CART_MAX_TRACK:length;memcpy(m->track,track,m->length*sizeof *track);
}
unsigned cart_model_tile(const CartModel *m){return m->segment<m->length?m->track[m->segment]:0;}
unsigned cart_model_coins(const CartModel *m){return m->score/10;}
static bool turn(const CartModel *m){unsigned t=cart_model_tile(m);return t==2||t==3;}
static bool corner(const CartModel *m){return turn(m)&&m->segmentTick>=11&&m->segmentTick<23;}
static void pose(CartModel *m,unsigned p){m->pose=p;m->poseTick=1;}
static void neutral(CartModel *m){m->stance=CART_NONE;m->trick=CART_NO_TRICK;m->move=CART_NO_MOVE;m->moveTick=0;pose(m,1);}
static void crash(CartModel *m,unsigned p){m->lives--;m->phase=m->lives?CART_RETRY:CART_LOST;m->phaseTick=0;pose(m,p);}

void cart_model_press(CartModel *m,CartKey key){
 if(m->phase==CART_RETRY){
  if(key==CART_SPACE&&m->phaseTick>=40){m->phase=CART_PLAYING;m->phaseTick=0;m->segment=m->safe;m->segmentTick=1;m->stance=CART_NONE;m->trick=m->lastTrick=CART_NO_TRICK;m->move=CART_NO_MOVE;m->moveTick=m->bonus=0;m->key=CART_NO_KEY;pose(m,24);}
  return;
 }
 if(m->phase!=CART_PLAYING||m->trick)return;
 m->key=key;
 if(key==CART_LEFT||key==CART_RIGHT){
  if(m->stance==CART_JUMPING){m->trick=CART_SPIN;pose(m,key==CART_LEFT?15:16);}
  else{m->move=m->stance==CART_SURFING?CART_SLIDE:m->stance==CART_WHEELIE?CART_GRIND:CART_LEAN;pose(m,(m->move==CART_SLIDE?7:m->move==CART_GRIND?12:2)+(key==CART_RIGHT));}
  return;
 }
 CartTrick trick=CART_NO_TRICK;unsigned p=0;
 if(m->stance==CART_SURFING){if(key==CART_SPACE){trick=CART_OLIE;p=6;}else if(key==CART_UP){trick=CART_HANDSTAND;p=18;}}
 else if(m->stance==CART_WHEELIE){if(key==CART_SPACE){trick=CART_BACKFLIP;p=11;}else if(key==CART_DOWN){trick=CART_RUNNING;p=20;}}
 else if(m->stance==CART_JUMPING){if(key==CART_UP){trick=CART_FLAP;p=17;}else if(key==CART_DOWN){trick=CART_SLAM;p=19;}}
 if(trick){m->trick=trick;pose(m,p);return;}
 if(m->stance==CART_NONE){m->stance=key==CART_UP?CART_SURFING:key==CART_DOWN?CART_WHEELIE:CART_JUMPING;pose(m,key==CART_UP?4:key==CART_DOWN?9:14);}
 else if((m->stance==CART_SURFING&&key==CART_DOWN)||(m->stance==CART_WHEELIE&&key==CART_UP))neutral(m);
}

static unsigned js_round_ratio(unsigned tick,unsigned divisor,unsigned points){
 // Math.round(tick/divisor*points) with the same double operations.
 double v=(double)tick/divisor*points;unsigned r=(unsigned)v;return v-r>=.5?r+1:r;
}
void cart_model_release(CartModel *m,CartKey key){
 if(key!=m->key||m->phase!=CART_PLAYING)return;
 if(m->move){
  CartMove move=m->move;
  if(move!=CART_LEAN&&(m->moveTick>10||m->bonus))m->score+=js_round_ratio(m->moveTick,move==CART_GRIND?28:32,move==CART_GRIND?80:40)+m->bonus;
  m->move=CART_NO_MOVE;m->moveTick=m->bonus=0;
  if(move!=CART_SLIDE)neutral(m);else pose(m,5);
 }
 m->key=CART_NO_KEY;
}

void cart_model_step(CartModel *m){
 if(m->phase!=CART_PLAYING){
  m->phaseTick++;m->poseTick++;
  if(m->phase==CART_RETRY&&m->segment!=m->safe&&(int)m->phaseTick>=(turn(m)?26:48)-(int)m->segmentTick){m->segment=m->safe;m->segmentTick=1;}
  return;
 }
 m->poseTick++;
 unsigned e=end(m->pose);
 if(m->trick&&e&&m->poseTick>=e){
  m->score+=SCORES[m->trick]/(m->lastTrick==m->trick?2:1);
  m->lastTrick=m->trick;m->stance=m->trick==CART_OLIE||m->trick==CART_HANDSTAND?CART_SURFING:CART_NONE;m->trick=CART_NO_TRICK;pose(m,m->stance==CART_SURFING?5:1);
 }else if((m->pose==9&&m->poseTick>=25)||(m->pose==14&&m->poseTick>=21)||(m->pose==24&&m->poseTick>=28))neutral(m);
 else if(m->pose==4&&m->poseTick>=17)pose(m,5);
 if(corner(m)&&(m->trick||m->key!=(cart_model_tile(m)==2?CART_RIGHT:CART_LEFT))){crash(m,22);return;}
 if(m->move&&!corner(m)){
  if(++m->moveTick>(m->move==CART_GRIND?28u:m->move==CART_SLIDE?32u:38u)){crash(m,m->move==CART_GRIND?22:m->key==CART_LEFT?21:23);return;}
 }
 m->segmentTick++;
 if(turn(m)&&m->segmentTick==23){if(m->move==CART_GRIND||m->move==CART_SLIDE)m->bonus=10;else m->score+=10;}
 if(cart_model_tile(m)==6&&m->segmentTick>=28){m->phase=CART_WON;return;}
 if(m->segmentTick>=(turn(m)?26u:48u)){
  if(cart_model_tile(m)==1)m->safe=m->segment;
  m->segment++;m->segmentTick=1;
 }
}
