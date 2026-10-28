// Hydro-Hopper controller: the original HydroHopper movie screens, HUD and
// game clip driven by game_hydro_model at 24 Hz (web: src/games/hydro.ts).
#include "game_controller.h"
#include "game_hydro_model.h"
#include "game_hydro_data.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HYDRO_ROOM 903
// Slots per depth group (tools/export-game.mjs); measured peak is 18 in one group.
#define HYDRO_SLOTS 24
#define HYDRO_WAKE 30
enum {SCREEN_TITLE,SCREEN_INSTRUCTIONS,SCREEN_GAME,SCREEN_NEXT,SCREEN_RESULTS};
enum {BUTTON_PLAY=1,BUTTON_READY,BUTTON_NEXT,BUTTON_QUIT,BUTTON_PAY,BUTTON_LEAVE};
static const char *const SOUNDS[]={"jump_sound","splash_sound","score_sound","crash_sound","bonus_sound"};
static const char *const HELP[]={
 "WARNING: BUOYS CANNOT BE JUMPED!","GRAB THE LIFE RING FOR AN EXTRA TUBE!","REMEMBER: ALWAYS SWIM WITH A BUDDY!",
 "WHAT DID THE SARDINE CALL THE SUBMARINE? A CAN OF PEOPLE!","WHY DID THE WHALE CROSS THE ROAD? TO GET TO THE OTHER TIDE!",
 "WHAT'S THE BEST WAY TO CATCH A FISH? HAVE SOMEONE THROW IT AT YOU.","WHAT'S THE DIFFERENCE BETWEEN A FISH AND A PIANO? YOU CAN'T TUNA FISH!","GOOD JOB!"};

typedef struct {int owner;unsigned kind,frame;} Slot;
typedef struct {
 GamePort *port;SWLScene *scene,*title,*under;GameBank *bank;
 HydroModel model;bool started,crashShown;unsigned screen,lastCrashes;
 double mouseX;float stick;
 unsigned shownScore,shownLevel;double wake[HYDRO_WAKE][2];
 Slot slots[3][HYDRO_SLOTS];
 bool touching,tap,wantGame;float touchX,touchY,touchTime;
 uint32_t seed;
} Hydro;

static double random01(void *user){
 // xorshift32; the original uses Math.random(), so any uniform source is faithful.
 Hydro *h=user;uint32_t x=h->seed;x^=x<<13;x^=x>>17;x^=x<<5;h->seed=x;return (double)x/4294967296.0;
}
static void text(Hydro *h,const char *path,const char *value){game_bank_text(h->bank,path,value);}
// swl_target keeps the request for instances that appear at the next preparation.
static void goto_frame(Hydro *h,const char *path,unsigned frame,const char *label,bool playing){swl_target(h->scene,path,frame,label,playing);}
static SWLMatrix translate(double x,double y){return (SWLMatrix){1,0,0,1,(float)x,(float)y};}

static void sync_score(Hydro *h){
 char score[12],padded[24],level[12],levelPadded[24];
 snprintf(score,sizeof score,"%u",h->shownScore);snprintf(level,sizeof level,"%u",h->shownLevel);
 snprintf(padded,sizeof padded,"%.*s%s",(int)(strlen(score)<6?6-strlen(score):0),"000000",score);
 snprintf(levelPadded,sizeof levelPadded,"%.*s%s",(int)(strlen(level)<2?2-strlen(level):0),"00",level);
 text(h,"root.score_mc.score1",score);text(h,"root.score_mc.score2",padded);
 text(h,"root.score_mc.level1",level);text(h,"root.score_mc.level2",levelPadded);
 unsigned lives=h->model.lives<1?1:h->model.lives;goto_frame(h,"root.lives_mc",lives>=4?1:5-lives,NULL,false);
}

// Frame 1 of the obstacle clip is empty and stopped, so free slots need no visibility override.
static void hide_slot(Hydro *h,unsigned g,unsigned i){Slot *s=&h->slots[g][i];if(s->owner>=0||s->kind){char path[48];snprintf(path,sizeof path,"root.game.%c%u",'a'+g,i);goto_frame(h,path,1,NULL,false);}*s=(Slot){.owner=-1};}
static void hide_game(Hydro *h){
 for(unsigned g=0;g<3;g++)for(unsigned i=0;i<HYDRO_SLOTS;i++)hide_slot(h,g,i);
 if(h->crashShown||!h->started)swl_visible(h->scene,"root.game.crash_mc",false);
 h->crashShown=false;
}

/** startGame(): a fresh round keeps score, level and lives. */
static void start_game(Hydro *h){
 const GameDescriptor *d=game_port_descriptor(h->port);
 unsigned score=h->model.score,level=h->model.level,lives=h->model.lives;bool keep=h->started;
 hydro_model_init(&h->model,random01,h);
 if(keep){h->model.score=score;h->model.level=level;h->model.lives=lives;}
 hydro_model_start(&h->model,d->hand);hydro_model_mouse(&h->model,h->mouseX);h->started=true;h->lastCrashes=0;
 for(unsigned i=0;i<HYDRO_WAKE;i++){h->wake[i][0]=0;h->wake[i][1]=400;}
 hide_game(h);
 goto_frame(h,"root.game.player.art",h->model.variant+1,NULL,false);
 goto_frame(h,"root.game.crash_mc",h->model.variant+1,NULL,false);
}

static void screen(Hydro *h,unsigned which){
 static const char *const labels[]={NULL,"Instructions","Game","Next Level","Game Over"};
 h->screen=which;h->touching=h->tap=false;
 if(which==SCREEN_TITLE)goto_frame(h,"root",1,NULL,false);else goto_frame(h,"root",0,labels[which],false);
 text(h,"root.play_label","PLAY");text(h,"root.ready_label","READY");text(h,"root.nextlevel_label","NEXT\nLEVEL?");
 text(h,"root.instruction","SLIDE THE STYLUS OR USE THE CIRCLE PAD TO MOVE. TAP OR PRESS A TO JUMP!");
 text(h,"root.score_mc.level","LEVEL:");text(h,"root.score_mc.score","SCORE:");
 if(which==SCREEN_NEXT){
  unsigned level=h->model.level,help=level<10?level-1:8;if(help<1)help=1;
  goto_frame(h,"root.help",help,NULL,false);text(h,"root.help.help",HELP[help-1]);
 }
 if(which==SCREEN_GAME)start_game(h);
 if(which==SCREEN_RESULTS)game_port_stop_sounds(h->port);
}

/** Ready: the title bank is no longer shown; the water/background bank takes its texture budget. */
static void start_play(Hydro *h){
 h->wantGame=true;
 if(h->title){game_port_release_bank(h->port,1);h->title=NULL;}
 GameResource state=game_port_require_bank(h->port,2);
 if(state==GAME_RESOURCE_PENDING)return;
 h->wantGame=false;if(state!=GAME_RESOURCE_READY)return; // the port shows a retry
 h->under=game_bank_scene(game_port_bank(h->port,2));if(h->under)screen(h,SCREEN_GAME);
}

static void *create(GamePort *port){Hydro *h=calloc(1,sizeof *h);if(!h)return NULL;h->port=port;h->mouseX=200;h->seed=0x9E3779B9u^(uint32_t)(uintptr_t)h;if(!h->seed)h->seed=1;hydro_model_init(&h->model,random01,h);return h;}
static void destroy(void *game){free(game);}

static bool ready(void *game){
 Hydro *h=game;h->bank=game_port_bank(h->port,0);h->scene=game_bank_scene(h->bank);
 h->title=game_bank_scene(game_port_bank(h->port,1));
 if(!h->scene||!h->title)return false;
 // Every body holder's script: new Color(body).setRGB(colours[myPlayer.Colour]).
 uint32_t rgb=game_port_descriptor(h->port)->colourRGB;
 SWLColor colour={{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}};
 for(unsigned i=0;i<HYDRO_BODIES;i++)if(!swl_color(h->scene,HYDRO_BODY_PATHS[i],colour))return false;
 hide_game(h);screen(h,SCREEN_TITLE);
 game_port_sound(h->port,"backgroundLoop",45,true);
 return true;
}

static int compare_y(const void *a,const void *b){
 const HydroHazard *x=*(const HydroHazard *const *)a,*y=*(const HydroHazard *const *)b;
 return x->y<y->y?-1:x->y>y->y?1:x->n<y->n?-1:x->n>y->n;
}

/** Draw order: obstacles sorted by y around the boat (y 140) and the player, like the web's sorted items. */
static void sync_hazards(Hydro *h){
 const HydroModel *m=&h->model;const HydroHazard *sorted[HYDRO_MAX_HAZARDS];
 for(unsigned i=0;i<m->count;i++)sorted[i]=&m->hazards[i];
 qsort(sorted,m->count,sizeof *sorted,compare_y);
 unsigned used[3]={0};bool snap=false;
 for(unsigned i=0;i<m->count;i++){
  const HydroHazard *z=sorted[i];unsigned g=z->y<=140?0:z->y<=m->y?1:2;
  if(used[g]>=HYDRO_SLOTS)continue;
  Slot *s=&h->slots[g][used[g]];char path[48],item[56];snprintf(path,sizeof path,"root.game.%c%u",'a'+g,used[g]);snprintf(item,sizeof item,"%s.item",path);used[g]++;
  unsigned kind=z->kind+2>25?25:z->kind+2,frame=z->frame<1?1:z->frame>HYDRO_CLIP_DATA[z->clip].frames?HYDRO_CLIP_DATA[z->clip].frames:z->frame;
  // A slot that changes owner keeps the same draw token; skip interpolation when its art would match.
  if(s->owner!=(int)z->n&&s->owner>=0&&s->kind==kind)snap=true;
  if(s->kind!=kind){goto_frame(h,path,kind,NULL,false);s->kind=kind;}
  if(s->frame!=frame||s->owner!=(int)z->n){goto_frame(h,item,frame,NULL,false);s->frame=frame;}
  s->owner=(int)z->n;
  swl_transform(h->scene,path,translate(z->x,HYDRO_HAZARD_Y));
 }
 for(unsigned g=0;g<3;g++)for(unsigned i=used[g];i<HYDRO_SLOTS;i++)if(h->slots[g][i].owner>=0)hide_slot(h,g,i);
 if(snap)game_port_snap(h->port);
}

static void tick(void *game){
 Hydro *h=game;if(!h->scene)return;
 if(h->screen!=SCREEN_GAME||game_port_session(h->port)!=GAME_SESSION_PLAYING){swl_tick(h->scene);return;}
 swl_tick(h->under);
 HydroModel *m=&h->model;
 hydro_model_mouse(m,h->mouseX);
 if(h->tap){h->tap=false;hydro_model_jump(m);}
 hydro_model_step(m);
 swl_tick(h->scene);
 for(unsigned i=0;i<m->soundCount;i++)game_port_sound(h->port,SOUNDS[m->sounds[i]],100,false);
 for(unsigned i=0;i<m->boatCount;i++)goto_frame(h,"root.game.boat",0,m->boat[i]==HYDRO_BOAT_SCORE?"Score":"Bump",true);
 m->soundCount=m->boatCount=0;
 if(m->crashes!=h->lastCrashes){
  h->lastCrashes=m->crashes;
  swl_visible(h->scene,"root.game.crash_mc",true);h->crashShown=true;
  swl_transform(h->scene,"root.game.crash_mc",translate(m->crashX,m->crashY));
  goto_frame(h,"root.game.crash_mc.art",2,NULL,true);
 }
 // updateWake: the head follows the tube (or rests at 400 mid-jump); the rest trail 8 px.
 for(unsigned i=HYDRO_WAKE-1;i>0;i--){h->wake[i][0]=h->wake[i-1][0];h->wake[i][1]=h->wake[i-1][1]+8;}
 double ax,ay;hydro_model_player_art(m->playerFrame,&ax,&ay);h->wake[0][0]=ax;h->wake[0][1]=m->jumping?400:ay;
 for(unsigned i=0;i<HYDRO_WAKE;i++){char path[40];snprintf(path,sizeof path,"root.water_mc.wake.wake%u",i+1);swl_transform(h->under,path,translate(h->wake[i][0],h->wake[i][1]));}
 // updateScore/updateLevel count up one per frame.
 if(h->shownScore<m->score)h->shownScore++;
 if(h->shownLevel<m->level)h->shownLevel++;
 goto_frame(h,"root.game.player",m->playerFrame>HYDRO_PLAYER_FRAMES?HYDRO_PLAYER_FRAMES:m->playerFrame,NULL,false);
 goto_frame(h,"root.game.player.art.penguin",m->penguinFrame,NULL,false);
 sync_hazards(h);sync_score(h);
 if(m->over)screen(h,SCREEN_RESULTS);
 else if(m->levelDone){hydro_model_next_level(m);screen(h,m->over?SCREEN_RESULTS:SCREEN_NEXT);}
}

static void frame(void *game,float seconds,const GameInput *input){
 Hydro *h=game;if(!h->scene)return;
 unsigned clicked=game_port_clicked(h->port);bool online=game_port_descriptor(h->port)->online,playing=game_port_session(h->port)==GAME_SESSION_PLAYING;
 bool confirm=input->down&GAME_KEY_A;
 switch(h->screen){
  case SCREEN_TITLE:if(playing&&(clicked==BUTTON_PLAY||confirm))screen(h,SCREEN_INSTRUCTIONS);break;
  case SCREEN_INSTRUCTIONS:if(playing&&(clicked==BUTTON_READY||confirm||h->wantGame))start_play(h);break;
  case SCREEN_NEXT:if(clicked==BUTTON_NEXT||confirm)screen(h,SCREEN_GAME);else if(clicked==BUTTON_QUIT)screen(h,SCREEN_RESULTS);break;
  case SCREEN_RESULTS:
   if(clicked==BUTTON_PAY||confirm)game_port_finish(h->port,online?h->model.score:0);
   else if(clicked==BUTTON_LEAVE)game_port_leave(h->port);
   break;
  case SCREEN_GAME:{
   if(clicked==BUTTON_QUIT){screen(h,SCREEN_RESULTS);break;}
   float x=input->stickX;if(input->held&GAME_KEY_LEFT)x=-1;if(input->held&GAME_KEY_RIGHT)x=1;
   if(fabsf(x)>.15f){h->mouseX+=x*360*seconds;if(h->mouseX<0)h->mouseX=0;if(h->mouseX>400)h->mouseX=400;}
   if(input->down&(GAME_KEY_A|GAME_KEY_B|GAME_KEY_X|GAME_KEY_Y|GAME_KEY_L|GAME_KEY_R|GAME_KEY_UP))h->tap=true;
   if(h->touching)h->touchTime+=seconds;
   break;}
 }
}

static void touch(void *game,GameTouch phase,float x,float y){
 Hydro *h=game;if(h->screen!=SCREEN_GAME)return;
 if(y<34)return; // the Quit row
 if(phase==GAME_TOUCH_DOWN){h->touching=true;h->touchX=x;h->touchY=y;h->touchTime=0;}
 if(!h->touching)return;
 h->mouseX=(double)x*400.0/320.0;
 // A short tap jumps (web: click); dragging only steers.
 if(phase==GAME_TOUCH_UP){if(h->touchTime<.3f&&fabsf(x-h->touchX)<10&&fabsf(y-h->touchY)<10)h->tap=true;h->touching=false;}
}

static void close_game(void *game){
 Hydro *h=game;
 if(h->screen==SCREEN_RESULTS)game_port_finish(h->port,game_port_descriptor(h->port)->online?h->model.score:0);
 else if(h->started)screen(h,SCREEN_RESULTS);
 else game_port_leave(h->port);
}

static void draw_bottom(void *game){
 Hydro *h=game;GamePort *p=h->port;char line[96];bool playing=game_port_session(p)==GAME_SESSION_PLAYING;
 if(h->screen!=SCREEN_TITLE)game_port_rect(p,0,0,320,h->screen==SCREEN_GAME?34:240,0x003366);
 switch(h->screen){
  case SCREEN_TITLE:
   game_port_title_band(p);
   game_port_button(p,BUTTON_PLAY,90,197,140,36,"Play",playing);
   break;
  case SCREEN_INSTRUCTIONS:
   game_port_text(p,160,34,12,0xffffff,GAME_ALIGN_CENTER,"Slide the stylus or use the Circle Pad");
   game_port_text(p,160,52,12,0xffffff,GAME_ALIGN_CENTER,"to steer. Tap, or press A or B, to jump.");
   game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Buoys cannot be jumped.");
   game_port_button(p,BUTTON_READY,90,110,140,44,"Ready",playing);
   break;
  case SCREEN_GAME:{
   game_port_button(p,BUTTON_QUIT,244,4,72,26,"Quit",true);
   snprintf(line,sizeof line,"Score %u   Level %u   Tubes %u",h->model.score,h->model.level,h->model.lives);
   game_port_text(p,8,10,11,0xffffff,GAME_ALIGN_LEFT,line);
   game_port_rect(p,0,224,320,16,0x003366);game_port_text(p,160,227,10,0xffffff,GAME_ALIGN_CENTER,"Drag to steer. Tap / A: jump");
   break;}
  case SCREEN_NEXT:
   snprintf(line,sizeof line,"Level %u   Score %u",h->model.level,h->model.score);
   game_port_text(p,160,40,14,0xffffff,GAME_ALIGN_CENTER,line);
   game_port_button(p,BUTTON_NEXT,90,90,140,44,"Next level",true);
   game_port_button(p,BUTTON_QUIT,110,150,100,30,"Quit",true);
   break;
  case SCREEN_RESULTS:{
   bool online=game_port_descriptor(p)->online;GameSessionState session=game_port_session(p);
   snprintf(line,sizeof line,"Score: %u",h->model.score);game_port_text(p,160,40,16,0xffffff,GAME_ALIGN_CENTER,line);
   if(online)snprintf(line,sizeof line,"You earned %u coins.",h->model.score/10);else snprintf(line,sizeof line,"Offline preview: no coins are paid.");
   game_port_text(p,160,66,13,0xffffff,GAME_ALIGN_CENTER,line);
   bool waiting=session==GAME_SESSION_PAYING&&!game_port_can_retry(p);
   game_port_button(p,BUTTON_PAY,90,100,140,44,waiting?"Paying...":online?"Get paid":"Exit",!waiting);
   if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,152,140,32,"Leave without waiting",true);
   break;}
 }
}

static void overlay(void *game,unsigned bank,float depth,const GameCanvas *canvas){
 Hydro *h=game;if(bank!=2||h->screen!=SCREEN_GAME||depth<FLT_MAX||!canvas->line)return;
 // updateRope: a 2 px line from the boat's tow point to the tube, above the water and wake, below everything else (as in the web).
 double ax,ay;hydro_model_player_art(h->model.playerFrame,&ax,&ay);
 canvas->line(canvas->user,HYDRO_GAME_X+HYDRO_PLAYER_X,HYDRO_GAME_Y+HYDRO_PLAYER_Y,(float)(HYDRO_GAME_X+ax+HYDRO_PLAYER_X),(float)(HYDRO_GAME_Y+ay+HYDRO_PLAYER_Y-10),2,0x000000);
}

static bool bank_view(void *game,unsigned bank,SWLMatrix *local){
 Hydro *h=game;*local=(SWLMatrix){1,0,0,1,0,0};
 if(bank==0)return true;
 if(bank==2)return h->screen==SCREEN_GAME;
 if(h->screen!=SCREEN_TITLE)return false;
 SWLInstance holder;if(!swl_instance(h->scene,"root.splash.title",&holder))return false;
 *local=holder.matrix;return true;
}
static bool lower_view(void *game,SWLMatrix *view){Hydro *h=game;if(h->screen==SCREEN_TITLE){*view=game_port_art_view(180,60,400,300);return true;}if(h->screen!=SCREEN_GAME)return false;*view=(SWLMatrix){.8f,0,0,.8f,-HYDRO_GAME_X*.8f,34-HYDRO_PLAYER_Y*.8f};return true;}

static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}

const GameController GAME_HYDRO_CONTROLLER={
 .key="hydro",.room=HYDRO_ROOM,.banks={"games/hydro.gpk","games/hydro-title.gpk","games/hydro-under.gpk"},.lazy=1u<<2,.order={2,0,1},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.overlay=overlay,.bank_view=bank_view,.lower_view=lower_view,
};
