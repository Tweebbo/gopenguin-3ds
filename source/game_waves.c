#include "game_controller.h"
#include "game_waves_model.h"
#include "game_waves.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAVES_ROOM 912
enum {SCREEN_MENU,SCREEN_GAME,SCREEN_RESULT};
enum {BUTTON_PAY=1,BUTTON_AGAIN,BUTTON_EXIT,BUTTON_JUMP=10,BUTTON_LEFT=11,BUTTON_RIGHT=12,BUTTON_UP=13,BUTTON_DOWN=14,BUTTON_MODE_BASE=20};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 WavesModel model;
 unsigned screen;
 bool started,left,right,up,down,jumpTouch;
 unsigned shown;
} Waves;

WavesModel *game_waves_model(void *game){return &((Waves *)game)->model;}
static const char *MODE_NAMES[]={"Freestyle","Survival","Competition","Lesson"};
static const char *SOUND_NAMES[]={"splash4","achord1","splash4","drip","dchord2","splash4"};
static const char *SOUND_KEYS[]={"jump","trick","wipeout","wave","win","lose"};

static const char *sound_name(const char *key){for(unsigned i=0;i<6;i++)if(!strcmp(key,SOUND_KEYS[i]))return SOUND_NAMES[i];return NULL;}
static void sync_art(Waves *w){
 WavesModel *m=&w->model;SWLScene *s=w->scene;
 swl_target(s,"root.water",1,NULL,false);swl_visible(s,"root.water",true);swl_visible(s,"root.bg",true);
 swl_goto(s,"root.player",1,m->air?"wavejump":"normalforward",false);
 double angle=m->spin*3.141592653589793/180.0;float c=(float)cos(angle),sn=(float)sin(angle);
 swl_transform(s,"root.player",(SWLMatrix){c,sn,-sn,c,380,(float)m->y});swl_visible(s,"root.player",true);
 double sharkX=380.0+(m->sharkX-m->distance);
 if(m->mode==WAVES_SURVIVAL&&sharkX>-120&&sharkX<820){
  swl_goto(s,"root.shark",1,"normal",false);swl_transform(s,"root.shark",(SWLMatrix){.35f,0,0,.35f,(float)sharkX,300});swl_visible(s,"root.shark",true);
 }else swl_visible(s,"root.shark",false);
 game_port_snap(w->port);
}
static void stop_audio(Waves *w){game_port_stop_sounds(w->port);}
static void start(Waves *w,unsigned mode){
 stop_audio(w);waves_model_init(&w->model,(WavesMode)mode);w->screen=SCREEN_GAME;w->started=true;
 w->left=w->right=w->up=w->down=w->jumpTouch=false;
 game_port_sound(w->port,"backgroundLoop",55,true);sync_art(w);
}
static void *create(GamePort *port){Waves *w=calloc(1,sizeof *w);if(!w)return NULL;w->port=port;waves_model_init(&w->model,WAVES_FREESTYLE);return w;}
static void destroy(void *game){free(game);}
static bool ready(void *game){Waves *w=game;w->bank=game_port_bank(w->port,0);w->scene=game_bank_scene(w->bank);if(!w->scene)return false;swl_visible(w->scene,"root.shark",false);w->screen=SCREEN_MENU;return true;}
static void tick(void *game){
 Waves *w=game;if(!w->scene)return;swl_tick_deferred(w->scene);
 if(w->screen!=SCREEN_GAME||game_port_session(w->port)!=GAME_SESSION_PLAYING)return;
 WavesModel *m=&w->model;waves_model_update(m,1000.0/24.0,w->up,w->down,w->left,w->right);
 for(unsigned i=0;i<m->soundCount;i++){const char *name=sound_name(m->sounds[i]);if(name)game_port_sound(w->port,name,70,false);}
 m->soundCount=0;
 sync_art(w);
 if(m->phase==WAVES_RESULT){stop_audio(w);w->screen=SCREEN_RESULT;}
}
static void frame(void *game,float seconds,const GameInput *input){
 Waves *w=game;(void)seconds;unsigned clicked=game_port_clicked(w->port);
 if(w->screen==SCREEN_MENU){
  if(clicked==BUTTON_EXIT){game_port_leave(w->port);return;}
  if(clicked>=BUTTON_MODE_BASE&&clicked<BUTTON_MODE_BASE+4){start(w,clicked-BUTTON_MODE_BASE);return;}
  return;
 }
 if(w->screen==SCREEN_RESULT){
  if(clicked==BUTTON_AGAIN||(input->down&GAME_KEY_A)){stop_audio(w);w->screen=SCREEN_MENU;return;}
  if(clicked==BUTTON_PAY||(input->down&GAME_KEY_B)){stop_audio(w);game_port_finish(w->port,waves_model_coins(&w->model));}
  return;
 }
 if(clicked==BUTTON_EXIT){stop_audio(w);game_port_finish(w->port,waves_model_coins(&w->model));return;}
 bool jump=(input->down&(GAME_KEY_A|GAME_KEY_B))||w->jumpTouch;w->jumpTouch=false;
 if(jump)waves_model_jump(&w->model);
}
static void touch(void *game,GameTouch phase,float x,float y){
 Waves *w=game;if(w->screen!=SCREEN_GAME)return;
 if(phase==GAME_TOUCH_UP){w->left=w->right=w->up=w->down=false;return;}
 if(y>=150){if(phase==GAME_TOUCH_DOWN)w->jumpTouch=true;return;}
 w->left=x<80;w->right=x>=240;w->up=x>=80&&x<160;w->down=x>=160&&x<240;
}
static void close_game(void *game){Waves *w=game;stop_audio(w);if(w->screen==SCREEN_MENU)game_port_leave(w->port);else game_port_finish(w->port,waves_model_coins(&w->model));}
static void draw_bottom(void *game){
 Waves *w=game;GamePort *p=w->port;WavesModel *m=&w->model;char line[96];
 if(w->screen==SCREEN_MENU){
  game_port_title_band(p);game_port_text(p,160,8,12,0xffffff,GAME_ALIGN_CENTER,"Catchin' Waves");
  for(unsigned i=0;i<4;i++)game_port_button(p,BUTTON_MODE_BASE+i,50,34+i*34,220,30,MODE_NAMES[i],true);
  game_port_button(p,BUTTON_EXIT,260,2,54,20,"Exit",true);
  return;
 }
 if(w->screen==SCREEN_RESULT){
  snprintf(line,sizeof line,"%s  %s",MODE_NAMES[m->mode],m->success?"Success":"Wipeout");
  game_port_text(p,160,44,14,0xffffff,GAME_ALIGN_CENTER,line);
  snprintf(line,sizeof line,"Score %u  Tricks %u  Coins %u",(unsigned)m->score,(unsigned)m->trickScore,waves_model_coins(m));
  game_port_text(p,160,72,11,0xffffff,GAME_ALIGN_CENTER,line);
  game_port_button(p,BUTTON_AGAIN,54,112,100,38,"Play again",true);
  game_port_button(p,BUTTON_PAY,178,112,100,38,"Get coins",true);
  return;
 }
 game_port_rect(p,0,0,320,34,0x003366);
 snprintf(line,sizeof line,"%s  Wave %u/3  Score %u",MODE_NAMES[m->mode],m->wave,(unsigned)m->score);
 game_port_text(p,5,6,9,0xffffff,GAME_ALIGN_LEFT,line);
 snprintf(line,sizeof line,"Lives %u  Combo %u  Distance %um",m->lives,m->combo,(unsigned)(m->distance/10));
 game_port_text(p,5,20,9,0xffffff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,266,4,50,22,"Exit",true);
 game_port_button(p,BUTTON_LEFT,4,140,70,42,"<",w->left);
 game_port_button(p,BUTTON_UP,80,140,70,42,"^",w->up);
 game_port_button(p,BUTTON_DOWN,156,140,70,42,"v",w->down);
 game_port_button(p,BUTTON_RIGHT,232,140,70,42,">",w->right);
 game_port_button(p,BUTTON_JUMP,60,190,200,44,"Jump",false);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}
const GameController GAME_WAVES_CONTROLLER={
 .key="waves",.kind=GAME_SOLO,.room=WAVES_ROOM,.rate=24,
 .banks={"games/waves.gpk"},.order={0,1,2},.backgroundRGB=0x0d74bd,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,
};
