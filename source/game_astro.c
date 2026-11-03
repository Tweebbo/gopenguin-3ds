// Astro Barrier on the original cabinet. Model: game_astro_model.c at 24 Hz.
#include "game_controller.h"
#include "game_astro_model.h"
#include "game_astro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASTRO_ROOM 900
enum {SCREEN_TITLE,SCREEN_GAME,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_FIRE,BUTTON_NEXT,BUTTON_RETRY,BUTTON_EXIT,BUTTON_PAY,BUTTON_LEAVE};

typedef struct {unsigned frame;bool hit;} Shown;

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 AstroModel model;
 bool started;
 unsigned screen;
 Shown shown[ASTRO_MAX_OBJECTS];
 unsigned shownShots;
 bool shownBullet;
 int pad;
 int lastDirection;
} Astro;

AstroModel *game_astro_model(void *game){return &((Astro *)game)->model;}

static void target(Astro *a,const char *path,unsigned frame,bool playing){swl_target(a->scene,path,frame,NULL,playing);}
static void text(Astro *a,const char *path,const char *value){game_bank_text(a->bank,path,value);}
static void translate(Astro *a,const char *path,double x,double y){swl_transform(a->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});}

static void sync_hud(Astro *a){
 char value[16];
 snprintf(value,sizeof value,"%u",a->model.score);text(a,"root.astroBarrier.score_txt",value);
 snprintf(value,sizeof value,"%u",a->model.level+1);text(a,"root.astroBarrier.level_txt",value);
 snprintf(value,sizeof value,"%u",a->model.lives>0?(unsigned)a->model.lives:0u);text(a,"root.astroBarrier.lives_txt",value);
 unsigned shots=a->model.shots+2;if(shots>12)shots=12;if(shots<1)shots=1;
 if(shots!=a->shownShots){target(a,"root.astroBarrier.shots",shots,false);a->shownShots=shots;}
}

static void sync_ship(Astro *a){
 AstroModel *m=&a->model;
 translate(a,"root.astroBarrier.gun",m->gunX,ASTRO_GUN_Y);
 if(m->bullet!=a->shownBullet){
  a->shownBullet=m->bullet;
  swl_visible(a->scene,"root.astroBarrier.bullet",m->bullet);
  target(a,"root.astroBarrier.bullet",m->bullet?2:1,false);
 }
 if(m->bullet)translate(a,"root.astroBarrier.bullet",m->gunX,m->bulletY);
 swl_visible(a->scene,"root.astroBarrier.turretbullet",false);
}

static void sync_object(Astro *a,unsigned index){
 const AstroObject *object=&a->model.objects[index];
 Shown *shown=&a->shown[index];
 if(shown->frame==object->frame&&shown->hit==object->hit)return;
 shown->frame=object->frame;shown->hit=object->hit;
 char path[64],wall[80];
 snprintf(path,sizeof path,"root.astroBarrier.%s",object->data->name);
 snprintf(wall,sizeof wall,"%s.wall",path);
 switch(object->data->kind){
  case ASTRO_BARRIER:
  case ASTRO_YELLOW:target(a,wall,object->frame,false);break;
  case ASTRO_SLIDE:
  case ASTRO_ONEUP:
   target(a,wall,object->frame,false);
   target(a,path,1,object->data->kind==ASTRO_SLIDE&&object->hit);
   break;
  case ASTRO_SWITCH:
  case ASTRO_TURRET:
  case ASTRO_ORANGE:target(a,path,object->frame,false);break;
  case ASTRO_BONUS:
   swl_visible(a->scene,path,!object->hit);
   target(a,path,object->hit?2:1,false);
   break;
  case ASTRO_BLOCK:
  case ASTRO_TRAIN:break;
 }
}

static void sync_objects(Astro *a){for(unsigned i=0;i<a->model.objectCount;i++)sync_object(a,i);}
static void sync(Astro *a){sync_ship(a);sync_objects(a);sync_hud(a);}

static void load_level(Astro *a){
 memset(a->shown,0,sizeof a->shown);
 a->shownShots=0;a->shownBullet=false;
 target(a,"root.astroBarrier",astro_model_level(&a->model)->frame,false);
 sync(a);game_port_snap(a->port);
}

static void start(Astro *a){
 a->started=true;a->screen=SCREEN_GAME;
 astro_model_start(&a->model);load_level(a);
}

static void next_level(Astro *a){
 astro_model_next(&a->model);
 if(a->model.phase==ASTRO_WON)return;
 load_level(a);
}

static void restart_level(Astro *a){
 astro_model_restart(&a->model);
 if(a->model.phase==ASTRO_PLAYING)load_level(a);
}

static void *create(GamePort *port){
 Astro *a=calloc(1,sizeof *a);if(!a)return NULL;
 a->port=port;astro_model_init(&a->model);return a;
}
static void destroy(void *game){free(game);}

static bool ready(void *game){
 Astro *a=game;
 a->bank=game_port_bank(a->port,0);a->scene=game_bank_scene(a->bank);
 if(!a->scene)return false;
 target(a,"root.astroBarrier",1,false);
 swl_visible(a->scene,"root.astroBarrier.leveldone",false);
 swl_visible(a->scene,"root.astroBarrier.levelrestart",false);
 swl_visible(a->scene,"root.astroBarrier.levelMember",false);
 swl_visible(a->scene,"root.astroBarrier.turretbullet",false);
 text(a,"root.astroBarrier.titleLabel","Astro Barrier");
 text(a,"root.astroBarrier.levelLabel","Level");
 text(a,"root.astroBarrier.livesLabel","Lives");
 text(a,"root.astroBarrier.score_txt","0");
 text(a,"root.astroBarrier.level_txt","1");
 text(a,"root.astroBarrier.lives_txt","2");
 target(a,"root.astroBarrier.shots",10,false);
 a->shownShots=10;a->screen=SCREEN_TITLE;
 return true;
}

static void drain_sounds(Astro *a){
 for(unsigned i=0;i<a->model.soundCount;i++)game_port_sound(a->port,a->model.sounds[i],100,false);
 a->model.soundCount=0;
 for(unsigned i=0;i<a->model.stampCount;i++){
  char value[8];snprintf(value,sizeof value,"%u",a->model.stamps[i]);
  const char *args[]={value};game_port_send(a->port,'s',"st#sse",1,args);
 }
 a->model.stampCount=0;
}

static void tick(void *game){
 Astro *a=game;if(!a->scene)return;
 swl_tick_deferred(a->scene);
 if(a->screen!=SCREEN_GAME||game_port_session(a->port)!=GAME_SESSION_PLAYING)return;
 astro_model_step(&a->model);
 drain_sounds(a);
 sync(a);
 if(a->model.phase==ASTRO_DONE||a->model.phase==ASTRO_LOST||a->model.phase==ASTRO_OVER||a->model.phase==ASTRO_WON)a->screen=SCREEN_RESULTS;
}

static void frame(void *game,float seconds,const GameInput *input){
 Astro *a=game;(void)seconds;if(!a->scene)return;
 unsigned clicked=game_port_clicked(a->port);
 AstroModel *m=&a->model;
 if(a->screen==SCREEN_TITLE){
  if(clicked==BUTTON_EXIT){game_port_leave(a->port);return;}
  if((clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))&&game_port_session(a->port)==GAME_SESSION_PLAYING)start(a);
  return;
 }
 if(a->screen==SCREEN_RESULTS){
  if(m->phase==ASTRO_DONE){
   if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_A)){next_level(a);drain_sounds(a);}
  }else if(m->phase==ASTRO_LOST){
   if(clicked==BUTTON_RETRY||(input->down&GAME_KEY_A))restart_level(a);
  }else{
   if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(a->port,astro_model_score(m));
   else if(clicked==BUTTON_LEAVE)game_port_leave(a->port);
  }
  return;
 }
 if(clicked==BUTTON_EXIT){game_port_finish(a->port,astro_model_score(m));return;}
 if(clicked==BUTTON_FIRE||(input->down&(GAME_KEY_A|GAME_KEY_B)))astro_model_fire(m);
 int direction=0;
 if((input->held&GAME_KEY_LEFT)||input->stickX<-.5f||a->pad<0)direction=-1;
 else if((input->held&GAME_KEY_RIGHT)||input->stickX>.5f||a->pad>0)direction=1;
 if(direction!=a->lastDirection){
  if(direction<0)astro_model_press(m,-1);
  else if(direction>0)astro_model_press(m,1);
  else astro_model_release(m);
  a->lastDirection=direction;
 }
}

static void touch(void *game,GameTouch phase,float x,float y){
 Astro *a=game;if(a->screen!=SCREEN_GAME||y<190)return;
 if(phase==GAME_TOUCH_UP){a->pad=0;return;}
 a->pad=x<70?-1:x>250?1:0;
}

static void close_game(void *game){
 Astro *a=game;
 if(!a->started){game_port_leave(a->port);return;}
 game_port_finish(a->port,astro_model_score(&a->model));
}

static void draw_bottom(void *game){
 Astro *a=game;GamePort *p=a->port;char line[96];
 if(a->screen==SCREEN_TITLE){
  game_port_title_band(p);
  game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",game_port_session(p)==GAME_SESSION_PLAYING);
  game_port_button(p,BUTTON_EXIT,196,197,84,36,"Exit",true);
  return;
 }
 AstroModel *m=&a->model;
 if(a->screen==SCREEN_RESULTS){
  snprintf(line,sizeof line,"Score %u",astro_model_score(m));
  game_port_text(p,160,40,16,0xffffff,GAME_ALIGN_CENTER,line);
  if(m->phase==ASTRO_DONE){
   game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Level complete!");
   game_port_button(p,BUTTON_NEXT,90,110,140,42,"Next level",true);
  }else if(m->phase==ASTRO_LOST){
   game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"No shots left.");
   game_port_button(p,BUTTON_RETRY,90,110,140,42,"Restart level",true);
  }else{
   bool online=game_port_descriptor(p)->online;
   if(online)snprintf(line,sizeof line,"You earned %u coins.",m->score);
   else snprintf(line,sizeof line,"Offline preview: no coins are paid.");
   game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,line);
   bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
   game_port_button(p,BUTTON_PAY,90,110,140,44,waiting?"Paying...":online?"Get coins":"Exit",!waiting);
   if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,162,140,30,"Leave without waiting",true);
  }
  return;
 }
 game_port_rect(p,0,0,320,30,0x003366);
 snprintf(line,sizeof line,"Score %u  Shots %u  Lives %u  Level %u",m->score,m->shots,m->lives>0?(unsigned)m->lives:0u,m->level+1);
 game_port_text(p,7,9,10,0xffffff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,264,3,52,22,"Exit",true);
 game_port_rect(p,0,190,320,50,0x003366);
 game_port_pad(p,4,196,58,40,"<",a->pad<0);
 game_port_button(p,BUTTON_FIRE,131,196,58,40,"Fire",true);
 game_port_pad(p,258,196,58,40,">",a->pad>0);
}

static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}

const GameController GAME_ASTRO_CONTROLLER={
 .key="astro",.kind=GAME_SOLO,.room=ASTRO_ROOM,.rate=24,
 .banks={"games/astro.gpk"},.order={0,1,2},.backgroundRGB=0x006600,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,
};
