#include "game_pizzatron_model.h"
#include "game_pizzatron_data.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned random_int(PizzModel *m,unsigned n){double value=m->random?m->random(m->user):(double)rand()/((double)RAND_MAX+1);return n?(unsigned)floor(value*n):0;}
static bool mask_hit(unsigned id,double x,double y){
 const PizzMask *mask=&PIZZ_MASKS[id];int px=(int)floor(x-mask->x),py=(int)floor(y-mask->y);
 if(px<0||py<0||px>=mask->w||py>=mask->h)return false;
 unsigned bit=(unsigned)py*mask->w+(unsigned)px;return (PIZZ_MASK_BITS[mask->offset+(bit>>3)]&(1<<(bit&7)))!=0;
}
static bool covered(const PizzModel *m,unsigned kind){
 static const int points[6][2]={{0,-50},{0,50},{-90,0},{90,0},{20,0},{-20,0}};
 for(unsigned p=0;p<6;p++){
  bool hit=false;
  for(unsigned i=0;i<PIZZ_SAUCE_SLOTS;i++){
   const PizzSplat *s=&m->splats[i];if(!s->active||s->kind!=kind)continue;
   unsigned mask=PIZZ_SPLAT_MASKS[kind-1][s->variant-1][s->age>10?9:s->age-1];
   if(mask_hit(mask,points[p][0]-s->x,points[p][1]-s->y)){hit=true;break;}
  }
  if(!hit)return false;
 }
 return true;
}
void pizz_model_init(PizzModel *m,bool candy,PizzRandom random,void *user){
 memset(m,0,sizeof *m);m->x=-240;m->y=375;m->order=1;m->serial=1;m->variant=1;m->popup=1;m->mouseX=m->oldX[0]=m->oldX[1]=380;m->mouseY=m->oldY[0]=m->oldY[1]=275;m->strengthY=20;m->candy=candy;m->random=random;m->user=user;
 for(unsigned i=0;i<PIZZ_DROP_SLOTS;i++)m->drops[i].vy=20;
}
bool pizz_model_hit(const PizzModel *m,double x,double y){return m->phase!=PIZZ_PLACE&&mask_hit(PIZZ_PIZZA_MASK,x-m->x,y-m->y);}
const PizzOrder *pizz_model_order(const PizzModel *m){return &PIZZ_ORDERS[m->order-1+(m->candy?24:0)];}
const char *pizz_model_label(const PizzModel *m,unsigned field){return field<7?PIZZ_ORDER_LABELS[m->order-1+(m->candy?24:0)][field]:"";}
unsigned pizz_model_coins(const PizzModel *m){return m->score+m->tips;}
void pizz_model_pickup(PizzModel *m,unsigned ingredient){if(m->result||ingredient<1||ingredient>7)return;m->ingredient=m->ingredient==ingredient?0:ingredient;if(m->ingredient>=4)m->variant=1+random_int(m,3);}
static void sound(PizzModel *m,const char *name){if(m->soundCount<4)m->sounds[m->soundCount++]=name;}
static void throw_ingredient(PizzModel *m,unsigned kind){
 PizzDrop *drop=&m->drops[m->dropCount++%PIZZ_DROP_SLOTS];drop->active=true;drop->kind=kind;drop->variant=m->variant;drop->x=m->mouseX;drop->y=m->mouseY;drop->vx-=m->strengthX;drop->vy-=m->strengthY;drop->rotation=0;
 if(fabs(m->strengthX)>=30)sound(m,"zip");
}
static void use_ingredient(PizzModel *m){
 unsigned ingredient=m->ingredient;if(!ingredient)return;
 if(ingredient<=2){
  if(m->held&&m->splatCount<290&&pizz_model_hit(m,m->mouseX,m->mouseY+100)){
   if(!(ingredient==1?m->normal:m->hot)){
    unsigned kind=ingredient+(m->candy?2:0);
    for(unsigned i=0;i<PIZZ_SAUCE_SLOTS;i++)if(m->splats[i].kind!=kind)m->splats[i].active=false;
    m->splats[m->splatCount]=(PizzSplat){.slot=m->splatCount,.kind=kind,.variant=1+random_int(m,4),.age=1,.x=m->mouseX-m->x,.y=m->mouseY-m->y+100,.active=true};
   }
   m->splatCount++;
  }else if(!m->held){throw_ingredient(m,ingredient);m->ingredient=0;}
  return;
 }
 if(m->held)return;
 if(pizz_model_hit(m,m->mouseX,m->mouseY)&&(ingredient==3||!m->perfect)){
  if(ingredient==3)m->cheese=true;
  else{
   unsigned slot=m->toppingCount++%PIZZ_TOPPING_SLOTS;
   m->toppings[slot]=(PizzTopping){.slot=slot,.kind=ingredient-3+(m->candy?4:0),.variant=m->variant,.x=m->mouseX-m->x,.y=m->mouseY-m->y,.active=true};m->counts[ingredient-4]++;
  }
  sound(m,"3");
 }else throw_ingredient(m,ingredient);
 m->ingredient=0;
}
void pizz_model_step(PizzModel *m){
 if(m->result)return;
 m->soundCount=0;
 for(unsigned i=0;i<PIZZ_SAUCE_SLOTS;i++)if(m->splats[i].active&&m->splats[i].age<10)m->splats[i].age++;
 if(m->phase==PIZZ_ORDER){
  if(m->perfect){unsigned possible=m->candy?24:m->made;m->order=1+random_int(m,possible>24?24:possible);m->perfect=false;}m->phase=PIZZ_PLAY;
 }else if(m->phase==PIZZ_PLAY){
  const PizzOrder *order=pizz_model_order(m);bool toppings=true;
  for(unsigned i=0;i<4;i++){if(order->topping[i]){if(m->counts[i]<order->topping[i])toppings=false;}else if(m->counts[i]){m->boost++;toppings=false;}}
  m->perfect=m->normal==(order->sauce==1)&&m->hot==(order->sauce==2)&&m->cheese==(bool)order->cheese&&toppings;
  if(m->perfect)m->boost++;
  m->normal=covered(m,m->candy?3:1);m->hot=covered(m,m->candy?4:2);
  if(m->x<900)m->x+=m->speed+m->boost;else m->phase=PIZZ_REMOVE;
 }else if(m->phase==PIZZ_REMOVE){
  if(m->perfect){m->consecutive++;m->handicap++;m->made++;m->score+=m->candy?10:5;m->popup=2;m->popupSerial++;}else{m->consecutive=0;m->handicap--;m->failures++;}
  unsigned tier=m->consecutive/5;if(tier>6)tier=6;if(tier){m->tips+=5+tier*5;m->popup=2+tier;m->popupSerial++;}
  m->ordersMade++;m->normal=m->hot=m->cheese=false;memset(m->counts,0,sizeof m->counts);m->splatCount=0;m->boost=0;m->overflow=0;memset(m->splats,0,sizeof m->splats);memset(m->toppings,0,sizeof m->toppings);m->phase=PIZZ_PLACE;
 }else{m->x=-240;m->y=375;m->serial++;m->phase=PIZZ_ORDER;}
 use_ingredient(m);
 if(m->splatCount>189){m->speed++;m->overflow=m->normal?1:m->hot?2:0;}
 else{static const int thresholds[8]={-2,-1,4,8,13,19,26,34};for(unsigned i=0;i<8;i++)if(m->handicap<=thresholds[i]){m->speed=i+1+(m->candy?1:0);break;}}
 m->strengthX=fmax(-30,fmin(30,(m->oldX[0]-m->mouseX)/3));m->strengthY=fmax(-20,fmin(40,(m->oldY[0]-m->mouseY+60)/3));
 m->oldX[0]=m->oldX[1];m->oldX[1]=m->mouseX;m->oldY[0]=m->oldY[1];m->oldY[1]=m->mouseY;
 for(unsigned i=0;i<PIZZ_DROP_SLOTS;i++){
  PizzDrop *drop=&m->drops[i];if(!drop->active)continue;
  double vx=floor(drop->vx+.5),vy=floor(drop->vy+.5);drop->vy+=vy>20?-1:vy<20?1:20-drop->vy;drop->vx+=vx>0?-.5:vx<0?.5:-drop->vx;drop->x+=drop->vx;drop->y+=drop->vy;drop->rotation+=drop->vx;
 }
 if(m->failures>=5)m->result=m->made<=1?4:5;
 if(m->ordersMade>=40)m->result=m->made>=40?7:6;
}
