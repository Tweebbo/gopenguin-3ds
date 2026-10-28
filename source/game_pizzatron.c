#include "game_controller.h"
#include "game_pizzatron_model.h"
#include "common.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {BUTTON_PLAY=1,BUTTON_HOW,BUTTON_CANDY,BUTTON_EXIT,BUTTON_PAY,BUTTON_LEAVE};
static const float ZOOM=.95f;
static const float WORK_Y=164;
static const float CENTRES[7]={18,62,112,157,202,247,292};
static const char *const LABELS[2][7]={{"Sauce","Hot","Cheese","Seaweed","Shrimp","Squid","Fish"},{"Choc.","Icing","Sprinkles","Liquorice","Chips","Marsh.","Jellies"}};
typedef struct {
 GamePort *port;GameBank *bank;SWLScene *scene;
 PizzModel model;uint32_t seed;
 unsigned screen,focus,lastOrder,lastResult,lastPopup;
 unsigned shownSplats[PIZZ_SAUCE_SLOTS],shownToppings[PIZZ_TOPPING_SLOTS];
 bool started,candy,finished,stylus,keyHeld;
 float pointerX,pointerY;
#ifdef GP_LAN_ONLY
 unsigned profileTicks;u64 sceneMS,modelMS,syncMS;
#endif
} Pizzatron;

static double random01(void *user){Pizzatron *p=user;p->seed^=p->seed<<13;p->seed^=p->seed>>17;p->seed^=p->seed<<5;return (double)p->seed/4294967296.0;}
static void target(Pizzatron *p,const char *path,unsigned frame){swl_target(p->scene,path,frame,NULL,false);}
static void place(Pizzatron *p,const char *path,double x,double y){swl_transform(p->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});}
static void number(Pizzatron *p,const char *path,unsigned n){char value[16];snprintf(value,sizeof value,"%u",n);game_bank_text(p->bank,path,value);}
static void sync(Pizzatron *p){
 PizzModel *m=&p->model;
 if(m->result){
  if(p->lastResult!=m->result){p->lastResult=m->result;target(p,"root",m->result);}
  number(p,"root.pizzasmadeText",m->made);number(p,"root.scoreText",m->score);number(p,"root.tipsText",m->tips);number(p,"root.earningsText",pizz_model_coins(m));return;
 }
 unsigned order=m->order+(p->candy?24:0);
 if(p->lastOrder!=order){
  p->lastOrder=order;target(p,"root.orderscreen",order);
  static const char *const fields[7]={"pizza","topping1","topping2","topping3","topping4","topping5","topping6"};
  for(unsigned i=0;i<7;i++){char path[48];snprintf(path,sizeof path,"root.orderscreen.%s",fields[i]);game_bank_text(p->bank,path,pizz_model_label(m,i));}
 }
 swl_visible(p->scene,"root.pizza",m->phase!=PIZZ_PLACE);place(p,"root.pizza",m->x,m->y);
 target(p,"root.pizza.base",m->normal?(p->candy?4:2):m->hot?(p->candy?5:3):1);target(p,"root.pizza.cheese",m->cheese?(p->candy?3:2):1);
 for(unsigned i=0;i<PIZZ_SAUCE_SLOTS;i++){
  const PizzSplat *s=&m->splats[i];unsigned frame=s->active?2+(s->kind-1)*40+(s->variant-1)*10+(s->age>10?9:s->age-1):0;
  if(!frame&&!p->shownSplats[i])continue;
  char path[48];snprintf(path,sizeof path,"root.pizza.sauce.s%u",i);
  if(frame!=p->shownSplats[i]){swl_visible(p->scene,path,frame!=0);if(frame)target(p,path,frame);p->shownSplats[i]=frame;}
  if(frame)place(p,path,s->x,s->y);
 }
 for(unsigned i=0;i<PIZZ_TOPPING_SLOTS;i++){
  const PizzTopping *t=&m->toppings[i];unsigned frame=t->active?2+(t->kind-1)*3+t->variant-1:0;
  if(!frame&&!p->shownToppings[i])continue;
  char path[48];snprintf(path,sizeof path,"root.pizza.topping.t%u",i);
  if(frame!=p->shownToppings[i]){swl_visible(p->scene,path,frame!=0);if(frame)target(p,path,frame);p->shownToppings[i]=frame;}
  if(frame)place(p,path,t->x,t->y);
 }
 const char *const ingredients[]={"ingredients_sauces","ingredients_cheese","ingredients_topping1","ingredients_topping2","ingredients_topping3","ingredients_topping4"};
 for(unsigned i=0;i<6;i++){
  unsigned frame=i==0?(p->candy?5:1)+(m->ingredient>=1&&m->ingredient<=2?m->ingredient:0):(p->candy?2:1);
  char upper[48],lower[48];snprintf(upper,sizeof upper,"root.%s",ingredients[i]);snprintf(lower,sizeof lower,"root.lower_%s",ingredients[i]);target(p,upper,frame);target(p,lower,frame);
  float x=i?CENTRES[i+1]:40,scale=(i?.32f:.43f)/ZOOM;
  swl_transform(p->scene,lower,(SWLMatrix){scale,0,0,scale,(float)m->x+(x-160)/ZOOM,(float)m->y+(60-WORK_Y)/ZOOM});
 }
 swl_transform(p->scene,"root.lower_bg",(SWLMatrix){320.0f/760/ZOOM,0,0,240.0f/480/ZOOM,(float)m->x,(float)m->y+(120-WORK_Y)/ZOOM});
 const PizzOrder *recipe=pizz_model_order(m);const char *const checks[]={"sauce","hotsauce","cheese","top1","top2","top3","top4"};
 for(unsigned i=0;i<7;i++){
  bool checked=i==0?m->normal:i==1?m->hot:i==2?m->cheese:m->counts[i-3]>=recipe->topping[i-3];char path[40];snprintf(path,sizeof path,"root.orderscreen.%s",checks[i]);target(p,path,checked?2:1);
 }
 number(p,"root.made",m->made);number(p,"root.left",40-m->ordersMade);number(p,"root.failures",m->failures);number(p,"root.coins",pizz_model_coins(m));
 unsigned hand=1;if(m->ingredient==1)hand=(p->candy?12:2)+(m->held?1:0);else if(m->ingredient==2)hand=(p->candy?14:4)+(m->held?1:0);else if(m->ingredient>=3)hand=m->ingredient+3+(p->candy?10:0);
 target(p,"root.hand",hand);target(p,"root.hand.topping",m->variant);place(p,"root.hand",m->mouseX,m->mouseY);
 if(p->lastPopup!=m->popupSerial){p->lastPopup=m->popupSerial;target(p,"root.txt_popup",m->popup);if(p->candy)swl_target(p->scene,"root.txt_popup.earned",0,"tip10",false);}
 if(m->overflow)swl_target(p->scene,"root.pizza.overflow",0,m->overflow==1?(p->candy?"explodecandy":"explode"):(p->candy?"explodehotcandy":"explodehot"),false);else target(p,"root.pizza.overflow",1);
 for(unsigned i=0;i<PIZZ_DROP_SLOTS;i++){
  const PizzDrop *drop=&m->drops[i];if(!drop->active)continue;char path[40],art[48];snprintf(path,sizeof path,"root.drop%u",i);snprintf(art,sizeof art,"%s.art",path);target(p,path,drop->kind+(p->candy?7:0)+1);target(p,art,drop->variant);
  double angle=drop->rotation*3.141592653589793/180;swl_transform(p->scene,path,(SWLMatrix){(float)cos(angle),(float)sin(angle),(float)-sin(angle),(float)cos(angle),(float)drop->x,(float)drop->y});
 }
}
static void start(Pizzatron *p){if(p->started)return;p->started=true;pizz_model_init(&p->model,p->candy,random01,p);target(p,"root",3);sync(p);game_port_snap(p->port);}
static void *create(GamePort *port){Pizzatron *p=calloc(1,sizeof *p);if(p){p->port=port;p->seed=0x91b7a6d3u^game_port_descriptor(port)->player;p->focus=1;p->pointerX=160;p->pointerY=WORK_Y;}return p;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Pizzatron *p=game;p->bank=game_port_bank(p->port,0);p->scene=game_bank_scene(p->bank);if(!p->scene)return false;target(p,"root",2);
 for(unsigned i=0;i<PIZZ_SAUCE_SLOTS;i++){char path[48];snprintf(path,sizeof path,"root.pizza.sauce.s%u",i);swl_visible(p->scene,path,false);}
 for(unsigned i=0;i<PIZZ_TOPPING_SLOTS;i++){char path[48];snprintf(path,sizeof path,"root.pizza.topping.t%u",i);swl_visible(p->scene,path,false);}
 uint32_t rgb=game_port_descriptor(p->port)->colourRGB;SWLColor colour={{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}};
 swl_color(p->scene,"root.title_mc.titlescreen_mc.penguin_mc",colour);swl_color(p->scene,"root.lower_title.penguin_mc",colour);return true;
}
static void finish(Pizzatron *p){if(p->finished)return;p->finished=true;p->model.held=false;game_port_finish(p->port,p->started?pizz_model_coins(&p->model)*10:0);}
static void tick(void *game){
 Pizzatron *p=game;if(p->finished)return;
#ifdef GP_LAN_ONLY
 u64 before=now_ms();
#endif
 if(p->scene)swl_tick(p->scene);
 if(!p->started)return;
#ifdef GP_LAN_ONLY
 u64 sceneAt=now_ms();
#endif
 pizz_model_step(&p->model);
#ifdef GP_LAN_ONLY
 u64 modelAt=now_ms();
#endif
 for(unsigned i=0;i<p->model.soundCount;i++)game_port_sound(p->port,p->model.sounds[i],100,false);
 sync(p);
#ifdef GP_LAN_ONLY
 p->sceneMS+=sceneAt-before;p->modelMS+=modelAt-sceneAt;p->syncMS+=now_ms()-modelAt;
 if(++p->profileTicks==24){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"pizzatron ticks=24 scene_ms=%llu model_ms=%llu sync_ms=%llu heads=%u preparations=%llu\n",(unsigned long long)p->sceneMS,(unsigned long long)p->modelMS,(unsigned long long)p->syncMS,swl_stats(p->scene).heads,(unsigned long long)swl_stats(p->scene).preparations);fclose(trace);}p->profileTicks=0;p->sceneMS=p->modelMS=p->syncMS=0;}
#endif
}
static void pointer(Pizzatron *p){
 p->model.mouseX=p->model.x+(p->pointerX-160)/ZOOM;p->model.mouseY=p->model.y+(p->pointerY-WORK_Y)/ZOOM-(p->model.ingredient>=1&&p->model.ingredient<=2?100:0);
}
static void frame(void *game,float seconds,const GameInput *input){
 Pizzatron *p=game;unsigned clicked=game_port_clicked(p->port);
 if(clicked==BUTTON_EXIT||clicked==BUTTON_PAY){finish(p);return;}if(clicked==BUTTON_LEAVE){game_port_leave(p->port);return;}
 if(!p->started){
  if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))start(p);
  if(clicked==BUTTON_HOW){p->screen=1;target(p,"root.title_mc",2);}
  if(clicked==BUTTON_CANDY||(input->down&GAME_KEY_X)){p->candy=!p->candy;target(p,"root.title_mc.titlescreen_mc.switch_mc",p->candy?2:1);}
  return;
 }
 if(p->finished||p->model.result)return;
 if(input->down&(GAME_KEY_L|GAME_KEY_LEFT))p->focus=p->focus>1?p->focus-1:7;
 if(input->down&(GAME_KEY_R|GAME_KEY_RIGHT))p->focus=p->focus<7?p->focus+1:1;
 if(!p->stylus){
  p->pointerX=fmaxf(8,fminf(312,p->pointerX+input->stickX*150*seconds));p->pointerY=fmaxf(100,fminf(232,p->pointerY-input->stickY*100*seconds));
  if(input->down&GAME_KEY_A)pizz_model_pickup(&p->model,p->focus);
  p->keyHeld=(input->held&GAME_KEY_A)!=0;p->model.held=p->keyHeld;pointer(p);
 }
}
static void touch(void *game,GameTouch phase,float x,float y){
 Pizzatron *p=game;if(!p->started||p->finished||p->model.result||y<30)return;
 p->pointerX=x;p->pointerY=y;
 if(phase==GAME_TOUCH_DOWN&&y<92){unsigned ingredient=x<42?1:x<84?2:x<134?3:x<179?4:x<224?5:x<269?6:7;p->focus=ingredient;pizz_model_pickup(&p->model,ingredient);}
 p->stylus=phase!=GAME_TOUCH_UP;p->model.held=p->stylus||p->keyHeld;pointer(p);
}
static void close_game(void *game){finish(game);}
static bool bank_view(void *game,unsigned bank,SWLMatrix *view){
 Pizzatron *p=game;if(!bank)return true;if(bank!=1||p->started||p->screen)return false;SWLInstance title;if(!swl_instance(p->scene,"root.title_mc.titlescreen_mc.titleImage",&title))return false;*view=title.matrix;return true;
}
static bool lower_view(void *game,SWLMatrix *view){
 Pizzatron *p=game;if(p->finished||(p->started&&p->model.result))return false;
 *view=p->started?(SWLMatrix){ZOOM,0,0,ZOOM,160-ZOOM*(float)p->model.x,WORK_Y-ZOOM*(float)p->model.y}:(SWLMatrix){1,0,0,1,0,0};return true;
}
static unsigned lower_frame(void *game,unsigned bank){Pizzatron *p=game;return bank?0:p->started?8:9;}
static void draw_bottom(void *game){
 Pizzatron *p=game;GamePort *port=p->port;PizzModel *m=&p->model;
 if(!p->started){
  game_port_rect(port,0,0,320,28,0x003366);game_port_text(port,10,6,14,0xffffff,GAME_ALIGN_LEFT,"Pizzatron 3000");
  game_port_button(port,BUTTON_EXIT,264,3,52,22,"Exit",true);
  game_port_button(port,BUTTON_PLAY,10,125,140,36,p->screen?"Start game":"Play",true);
  if(!p->screen)game_port_button(port,BUTTON_HOW,10,168,140,32,"Instructions",true);
  game_port_button(port,BUTTON_CANDY,10,207,190,28,p->candy?"Candy mode: ON":"Candy mode: OFF",true);
  if(p->screen){game_port_rect(port,155,127,160,75,0xffffff);game_port_text(port,161,132,10,0x003366,GAME_ALIGN_LEFT,"Drag sauce onto the pizza.");game_port_text(port,161,150,10,0x003366,GAME_ALIGN_LEFT,"Drop the ordered toppings.");game_port_text(port,161,168,10,0x003366,GAME_ALIGN_LEFT,"L/R choose, A hold, Circle Pad.");}
  return;
 }
 if(p->finished||m->result){
  game_port_rect(port,0,0,320,240,0xf4e5c9);game_port_text(port,160,18,16,0x543201,GAME_ALIGN_CENTER,m->made==40?"40 perfect pizzas!":"Pizzatron 3000");
  char summary[72];snprintf(summary,sizeof summary,"%u pizzas  |  %u mistakes",m->made,m->failures);game_port_text(port,160,65,12,0x543201,GAME_ALIGN_CENTER,summary);snprintf(summary,sizeof summary,"Sales %u  +  tips %u",m->score,m->tips);game_port_text(port,160,91,12,0x543201,GAME_ALIGN_CENTER,summary);snprintf(summary,sizeof summary,"%u coins",pizz_model_coins(m));game_port_text(port,160,121,16,0x543201,GAME_ALIGN_CENTER,summary);
  if(!game_port_descriptor(port)->online)game_port_text(port,160,153,10,0x543201,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
  if(!p->finished)game_port_button(port,BUTTON_PAY,85,185,150,36,"Get paid",true);
  else if(!game_port_paid(port)&&*game_port_status(port))game_port_button(port,BUTTON_LEAVE,45,160,230,30,"Return without waiting",true);
  return;
 }
 game_port_rect(port,0,0,320,28,0x003366);game_port_text(port,7,7,10,0xffffff,GAME_ALIGN_LEFT,pizz_model_label(m,0));game_port_button(port,BUTTON_EXIT,264,3,52,22,"Exit",true);
 game_port_rect(port,0,76,320,16,0xffefd5);
 const PizzOrder *order=pizz_model_order(m);
 for(unsigned i=0;i<7;i++){
  game_port_text(port,CENTRES[i],78,10,0x003366,GAME_ALIGN_CENTER,LABELS[p->candy][i]);
  bool selected=p->focus==i+1;unsigned rgb=selected?0xffcc00:0x07539a;
  if(selected){float x=i<2?(float)(i*42):i==2?84:134+(float)(i-3)*45,w=i<2?42:i==2?50:i==6?51:45;game_port_rect(port,x,31,w,2,rgb);}
  if(i>=3&&order->topping[i-3]){char count[16];snprintf(count,sizeof count,"%u/%u",m->counts[i-3],order->topping[i-3]);game_port_text(port,CENTRES[i],94,10,0x003366,GAME_ALIGN_CENTER,count);}
 }
 char status[64];snprintf(status,sizeof status,"%u/40  |  %u coins  |  %u mistakes",m->ordersMade,pizz_model_coins(m),m->failures);game_port_rect(port,0,222,320,18,0x003366);game_port_text(port,160,225,10,0xffffff,GAME_ALIGN_CENTER,status);
}
const GameController GAME_PIZZATRON_CONTROLLER={.key="pizzatron",.kind=GAME_SOLO,.room=910,.rate=24,.banks={"games/pizzatron.gpk","games/pizzatron-title.gpk"},.order={0,1,2},.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.lower_view=lower_view,.lower_frame=lower_frame};
