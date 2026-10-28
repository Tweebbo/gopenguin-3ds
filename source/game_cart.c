// Cart Surfer controller: the original game-cart movie (start screen, mine track,
// cart poses, HUD) driven by game_cart_model at 24 Hz (web: src/games/cart.ts).
#include "game_controller.h"
#include "game_cart_model.h"
#include "game_cart_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CART_ROOM 905
enum {SCREEN_TITLE,SCREEN_GAME};
enum {BUTTON_PLAY=1,BUTTON_HOW,BUTTON_EXIT,BUTTON_PAY,BUTTON_LEAVE,BUTTON_QUIT};
static const char *const PAD_LABELS[5]={"Left","Up","Down","Right","Jump"};
static const CartKey PAD_KEYS[5]={CART_LEFT,CART_UP,CART_DOWN,CART_RIGHT,CART_SPACE};

typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 CartModel model;bool started,instructions;unsigned screen;
 unsigned previousSegment,lastScore,points,pointTick;CartPhase previousPhase;bool segmentShown;
 bool held[5];int touchPad;
 uint32_t seed;
} Cart;

static uint32_t next(Cart *c){uint32_t x=c->seed;x^=x<<13;x^=x>>17;x^=x<<5;c->seed=x;return x;}
static void target(Cart *c,const char *path,unsigned frame,bool playing){swl_target(c->scene,path,frame,NULL,playing);}
static void visible(Cart *c,const char *path,bool shown){swl_visible(c->scene,path,shown);}
static void text(Cart *c,const char *path,const char *value){game_bank_text(c->bank,path,value);}

static void sync(Cart *c){
 CartModel *m=&c->model;char v[24];
 if(!c->segmentShown||c->previousSegment!=m->segment||(c->previousPhase==CART_RETRY&&m->phase==CART_PLAYING)){
  c->segmentShown=true;c->previousSegment=m->segment;target(c,"root.game_mc",cart_model_tile(m),false);
  // The twelve track segments restart their authored zoom on every tile.
  for(unsigned i=1;i<=12;i++){char path[32];snprintf(path,sizeof path,"root.game_mc#2.seg%u",i);target(c,path,13-i,true);}
 }
 target(c,"root.game_mc.cart_mc",m->pose,false);
 target(c,"root.lives_mc",m->lives>=4?1:5-m->lives,false);
 snprintf(v,sizeof v,"%u",m->score);text(c,"root.score_mc.score1",v);
 char padded[24];snprintf(padded,sizeof padded,"%06u",m->score);text(c,"root.score_mc.score2",padded);
 if(c->lastScore!=m->score){c->points=m->score-c->lastScore;c->lastScore=m->score;c->pointTick=1;}
 if(c->pointTick){
  ++c->pointTick;target(c,"root.point_mc",c->pointTick>17?17:c->pointTick,false);snprintf(v,sizeof v,"%u",c->points);text(c,"root.point_mc#1.message1",v);
  if(c->pointTick>=17){target(c,"root.point_mc",1,false);c->pointTick=0;}
 }
 target(c,"root.messages_mc",m->phase==CART_RETRY?(20+m->phaseTick>60?60:20+m->phaseTick):1,false);
 if(c->previousPhase!=m->phase&&(m->phase==CART_WON||m->phase==CART_LOST))game_port_stop_sounds(c->port);
 c->previousPhase=m->phase;
}

static void start(Cart *c){
 if(c->started)return;
 c->started=true;c->screen=SCREEN_GAME;
 unsigned track=next(c)%CART_TRACKS;cart_model_init(&c->model,CART_TRACK_DATA[track],CART_TRACK_LENGTH[track]);
 c->lastScore=c->points=c->pointTick=0;c->segmentShown=false;c->previousPhase=CART_PLAYING;
 visible(c,"root.window_mc",false);
 static const char *const shown[]={"root.game_mc","root.lives_mc","root.score_mc","root.point_mc","root.messages_mc"};
 for(unsigned i=0;i<5;i++)visible(c,shown[i],true);
 // The title bank (attached title movie) is no longer shown.
 game_port_release_bank(c->port,1);
 sync(c);game_port_snap(c->port);
}

static void *create(GamePort *port){Cart *c=calloc(1,sizeof *c);if(!c)return NULL;c->port=port;c->touchPad=-1;c->seed=0x1B873593u^(uint32_t)(uintptr_t)c;if(!c->seed)c->seed=5;return c;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Cart *c=game;c->bank=game_port_bank(c->port,0);c->scene=game_bank_scene(c->bank);if(!c->scene||!game_port_bank(c->port,1))return false;
 static const char *const hidden[]={"root.game_mc","root.lives_mc","root.score_mc","root.point_mc","root.messages_mc","root.window_mc.start_screen.instructions_mc",
  "root.window_mc.start_screen.penguin_cart_screen.puffle_mc","root.window_mc.start_screen.instructions_mc.puffle","root.window_mc.start_screen.instructions_mc.puffle_fall",
  "root.game_mc.cart_mc.art.puffle","root.game_mc.cart_mc.art.enters.puffle"};
 for(unsigned i=0;i<sizeof hidden/sizeof *hidden;i++)visible(c,hidden[i],false);
 target(c,"root.window_mc.start_screen",1,false);
 uint32_t rgb=game_port_descriptor(c->port)->colourRGB;
 SWLColor colour={{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}};
 swl_color(c->scene,"root.window_mc.start_screen.penguin_cart_screen.penguin_mc",colour);
 for(unsigned i=0;i<CART_TINTS;i++)swl_color(c->scene,CART_TINT_PATHS[i],colour);
 return true;
}

static void press(Cart *c,unsigned pad,bool down){
 if(c->held[pad]==down)return;
 c->held[pad]=down;if(down)cart_model_press(&c->model,PAD_KEYS[pad]);else cart_model_release(&c->model,PAD_KEYS[pad]);
}

static void tick(void *game){
 Cart *c=game;if(!c->scene)return;
 if(c->screen!=SCREEN_GAME||game_port_session(c->port)!=GAME_SESSION_PLAYING){swl_tick(c->scene);return;}
 if(c->model.phase==CART_WON||c->model.phase==CART_LOST)return;
 swl_tick(c->scene);cart_model_step(&c->model);sync(c);
}

static void frame(void *game,float seconds,const GameInput *input){
 Cart *c=game;(void)seconds;if(!c->scene)return;
 unsigned clicked=game_port_clicked(c->port);bool online=game_port_session(c->port)==GAME_SESSION_PLAYING;
 if(c->screen==SCREEN_TITLE){
  if(online&&(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A)))start(c);
  else if(clicked==BUTTON_HOW){c->instructions=true;visible(c,"root.window_mc.start_screen.instructions_mc",true);}
  else if(clicked==BUTTON_EXIT)game_port_finish(c->port,0);
  return;
 }
 CartModel *m=&c->model;
 if(m->phase==CART_WON||m->phase==CART_LOST){
  if(game_port_session(c->port)==GAME_SESSION_PAYING&&clicked==BUTTON_LEAVE)game_port_leave(c->port);
  else if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(c->port,cart_model_coins(m));
  return;
 }
 if(clicked==BUTTON_QUIT){game_port_finish(c->port,cart_model_coins(m));return;}
 // Arrow keys and the space bar: D-Pad or Circle Pad, and A (or B) for space.
 bool keys[5]={(input->held&GAME_KEY_LEFT)||input->stickX<-.5f,(input->held&GAME_KEY_UP)||input->stickY>.5f,(input->held&GAME_KEY_DOWN)||input->stickY<-.5f,(input->held&GAME_KEY_RIGHT)||input->stickX>.5f,(input->held&(GAME_KEY_A|GAME_KEY_B))!=0};
 for(unsigned i=0;i<5;i++)press(c,i,keys[i]||c->touchPad==(int)i);
}

// Lower-screen pads, like the web touch layout.
static int pad_at(float x,float y){if(y<178||y>234)return -1;int i=(int)((x-4)/63.2f);return i>=0&&i<5?i:-1;}
static void touch(void *game,GameTouch phase,float x,float y){
 Cart *c=game;if(c->screen!=SCREEN_GAME)return;
 c->touchPad=phase==GAME_TOUCH_UP?-1:pad_at(x,y);
}

static void close_game(void *game){Cart *c=game;game_port_finish(c->port,c->started?cart_model_coins(&c->model):0);}

static void draw_bottom(void *game){
 Cart *c=game;GamePort *p=c->port;char line[96];bool online=game_port_session(p)==GAME_SESSION_PLAYING;
 bool active=c->screen==SCREEN_GAME&&c->model.phase!=CART_WON&&c->model.phase!=CART_LOST;
 if(c->screen!=SCREEN_TITLE)game_port_rect(p,0,0,320,active?34:240,0x2b1d12);
 if(c->screen==SCREEN_TITLE){
  game_port_title_band(p);
  game_port_button(p,BUTTON_PLAY,6,197,112,36,"Play",online);
  game_port_button(p,BUTTON_HOW,124,197,112,36,"Instructions",!c->instructions);
  game_port_button(p,BUTTON_EXIT,242,197,72,36,"Exit",true);
  return;
 }
 const CartModel *m=&c->model;
 snprintf(line,sizeof line,"Score %u   Carts %u",m->score,m->lives);game_port_text(p,8,10,11,0xffffff,GAME_ALIGN_LEFT,line);
 if(m->phase==CART_WON||m->phase==CART_LOST){
  snprintf(line,sizeof line,"%s You earned %u coins.",m->phase==CART_WON?"Mine complete!":"Game over.",cart_model_coins(m));
  game_port_text(p,160,60,13,0xffffff,GAME_ALIGN_CENTER,line);
  if(!game_port_descriptor(p)->online)game_port_text(p,160,82,11,0xf0e0c0,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
  bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
  game_port_button(p,BUTTON_PAY,90,104,140,42,waiting?"Paying...":game_port_descriptor(p)->online?"Get paid":"Exit",!waiting);
  if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,156,140,30,"Leave without waiting",true);
  return;
 }
 game_port_button(p,BUTTON_QUIT,244,4,72,26,"Quit",true);
 if(m->phase==CART_RETRY){game_port_rect(p,20,40,280,23,0x2b1d12);game_port_text(p,160,46,11,0xffffff,GAME_ALIGN_CENTER,"Press A or Jump to continue");}
 for(unsigned i=0;i<5;i++){float x=4+i*63.2f;game_port_pad(p,x,178,59,56,PAD_LABELS[i],c->held[i]);}
}

static bool bank_view(void *game,unsigned bank,SWLMatrix *local){
 Cart *c=game;*local=(SWLMatrix){1,0,0,1,0,0};if(!bank)return true;
 if(c->screen!=SCREEN_TITLE)return false;
 SWLInstance title;if(!swl_instance(c->scene,"root.window_mc.start_screen.title_mc",&title))return false;
 *local=title.matrix;return true;
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
static bool lower_view(void *game,SWLMatrix *view){Cart *c=game;if(c->screen==SCREEN_TITLE){*view=game_port_art_view(30,0,420,315);return true;}if(c->screen!=SCREEN_GAME||c->model.phase==CART_WON||c->model.phase==CART_LOST)return false;*view=(SWLMatrix){.6f,0,0,.6f,160-380*.6f,100-360*.6f};return true;}

const GameController GAME_CART_CONTROLLER={
 .key="cart",.room=CART_ROOM,.banks={"games/cart.gpk","games/cart-title.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.lower_view=lower_view,
};
