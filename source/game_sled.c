// Sled Racing controller: SledRacer.swf at a waddle, Waddle relaying uz/zm and paying
// the standing with zo, as in the web Sled (src/games/sled.ts).
#include "game_controller.h"
#include "game_sled_model.h"
#include "game_sled_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLED_TILE_SLOTS 7
// Dual screen: the lower screen shows our lane at this zoom, centred on the middle of the
// course, so the sled sits under the stylus; the top screen keeps the whole race.
#define LOWER_ZOOM 1.2f
#define LOWER_X 160.0f
#define LOWER_Y 150.0f
enum {MESSAGES_WAIT,MESSAGES_READY};
enum {BUTTON_EXIT=1,BUTTON_YES,BUTTON_NO,BUTTON_LEAVE,BUTTON_SCREENS};

typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 SledModel model;unsigned messages,readyTicks;bool asking,finished;
 int slotOf[SLED_RACERS];int tileShown[SLED_TILE_SLOTS];double drawnY[SLED_RACERS];
 SWLMatrix background;
 unsigned repeat;int touchDir,keyDir;
 bool dual,stylus;double stylusX;
} Sled;

/** Stage position of a racer at lateral x (game at 80,40 and the hill offset cancel our y). */
static void lane_point(double x,float *sx,float *sy){*sx=(float)(180+x);*sy=(float)(240-x*.6);}

static void target(Sled *s,const char *path,unsigned frame){swl_target(s->scene,path,frame,NULL,false);}
static void place(Sled *s,const char *path,double x,double y){swl_transform(s->scene,path,(SWLMatrix){1,0,0,1,(float)x,(float)y});}

static void sync(Sled *s){
 SledModel *m=&s->model;double myY=m->me>=0?m->racers[m->me].y:0;bool snap=false;
 // messages_mc: Wait, then Ready plays to frame 68, which calls handleStartGame, and on
 // past 79 back to its empty, stopped frame 1 (the web holds 79, leaving GO! up).
 target(s,"root.messages_mc",s->messages==MESSAGES_READY?(20+s->readyTicks>79?1:20+s->readyTicks):10);
 SWLMatrix background=s->background;background.x=(float)(-myY/25);swl_transform(s->scene,"root.game.background",background);
 place(s,"root.game.hill",100-myY,200-myY*.6);
 // addTile/removeTile: seven tiles around ours, each in slot n%7 so only one moves per tile.
 int tile=(int)floor(myY/200+.5);
 for(int n=tile-2;n<=tile+4;n++){
  int k=((n%SLED_TILE_SLOTS)+SLED_TILE_SLOTS)%SLED_TILE_SLOTS;char path[32];snprintf(path,sizeof path,"root.game.hill.tl%d",k);
  bool on=n>=0&&n<(int)m->hillLength;
  if(s->tileShown[k]==(on?n:-1))continue;
  if(on){unsigned type=0;for(unsigned i=0;i<SLED_TILE_TYPES;i++)if(SLED_TILE_IDS[i]==m->hill[n])type=i;target(s,path,type+1);place(s,path,n*200.0,n*120.0);}
  else place(s,path,0,-10000);
  if(s->tileShown[k]!=-2)snap=true;
  s->tileShown[k]=on?n:-1;
 }
 // The finish (t100) is drawn over the last tile (t99).
 int finish=-1;for(int n=tile-2;n<=tile+4;n++)if(n>=0&&n<(int)m->hillLength&&m->hill[n]==99)finish=n;
 if(finish>=0)place(s,"root.game.hill.fin",finish*200.0,finish*120.0);else place(s,"root.game.hill.fin",0,-10000);
 // Racers in row order (row*10+seat) fill player0-3.
 int order[SLED_RACERS],count=0;
 for(unsigned i=0;i<m->racerCount;i++)if(m->racers[i].named)order[count++]=(int)i;
 for(int i=1;i<count;i++){int v=order[i],j=i;while(j&&m->racers[order[j-1]].row*10+order[j-1]>m->racers[v].row*10+v){order[j]=order[j-1];j--;}order[j]=v;}
 for(int k=0;k<SLED_RACERS;k++){
  char path[32],sub[48];snprintf(path,sizeof path,"root.game.hill.player%d",k);
  if(k>=count){swl_visible(s->scene,path,false);s->slotOf[k]=-1;continue;}
  swl_visible(s->scene,path,true);
  const SledRacer *r=&m->racers[order[k]];
  if(s->slotOf[k]!=order[k]){
   if(s->slotOf[k]>=0)snap=true;
   s->slotOf[k]=order[k];
   uint32_t rgb=SLED_PLAYER_COLOURS[r->colour<SLED_COLOURS?r->colour:0];
   SWLColor colour={{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}};
   snprintf(sub,sizeof sub,"%s.art.body",path);swl_color(s->scene,sub,colour);
   snprintf(sub,sizeof sub,"%s.art.name_txt",path);game_bank_text(s->bank,sub,r->name);
  }
  // Sled.drawRace: a correction of more than 60 is drawn at once, not slid into.
  if(fabs(r->y-s->drawnY[order[k]])>60)snap=true;
  s->drawnY[order[k]]=r->y;
  target(s,path,r->sled);place(s,path,r->y+r->x,r->y*.6-r->x*.6);
  snprintf(sub,sizeof sub,"%s.art",path);target(s,sub,r->art);place(s,sub,0,-r->h);
  snprintf(sub,sizeof sub,"%s.shadow",path);place(s,sub,0,-r->ground);
 }
 // progress_mc: one marker per racer along the course (_y = 0), ours in frame 2.
 for(unsigned i=0;i<SLED_RACERS;i++){
  char path[32];snprintf(path,sizeof path,"root.progress_mc.player%u",i);const SledRacer *r=&m->racers[i];
  bool shown=i<m->racerCount&&r->named;swl_visible(s->scene,path,shown);
  if(shown){target(s,path,(int)i==m->me?2:1);place(s,path,r->y/180*(m->mapRows/300.0),0);}
 }
 if(snap)game_port_snap(s->port);
}

static bool packet(void *game,const char *command,unsigned count,char *const *args){
 Sled *s=game;
 // handleUpdateGame: [count, players...]; the first list starts the countdown. The model
 // needs no scene, so lists and moves that arrive while the bank loads are kept.
 if(!strcmp(command,"uz")){
  if(count>1)sled_model_players(&s->model,count-1>SLED_RACERS?SLED_RACERS:count-1,(const char *const *)args+1,game_port_descriptor(s->port)->name);
  if(s->messages==MESSAGES_WAIT){s->messages=MESSAGES_READY;s->readyTicks=0;}
  return true;
 }
 if(!strcmp(command,"zm")){
  if(count<4)return false;
  char *end;long id=strtol(args[0],&end,10);if(*end||id<0||id>=SLED_RACERS)return true;
  sled_model_remote(&s->model,(int)id,strtod(args[1],NULL),strtod(args[2],NULL),strtod(args[3],NULL));return true;
 }
 return false;
}

static void *create(GamePort *port){
 Sled *s=calloc(1,sizeof *s);if(!s)return NULL;s->port=port;
 sled_model_init(&s->model,game_port_descriptor(port)->waddle);
 for(int i=0;i<SLED_RACERS;i++)s->slotOf[i]=-1;
 for(int i=0;i<SLED_TILE_SLOTS;i++)s->tileShown[i]=-2;
 s->dual=true;
 return s;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Sled *s=game;s->bank=game_port_bank(s->port,0);s->scene=game_bank_scene(s->bank);if(!s->scene)return false;
 // The background keeps its authored matrix except _x: swl_instance gives world matrices, so undo game's.
 SWLInstance parent,background;if(!swl_instance(s->scene,"root.game",&parent)||!swl_instance(s->scene,"root.game.background",&background))return false;
 SWLMatrix g=parent.matrix;float det=g.a*g.d-g.b*g.c;if(det==0)return false;
 SWLMatrix inverse={g.d/det,-g.b/det,-g.c/det,g.a/det,(g.c*g.y-g.d*g.x)/det,(g.b*g.x-g.a*g.y)/det};
 s->background=swl_matrix_multiply(inverse,background.matrix);
 sync(s);game_port_send(s->port,'z',"jz",0,NULL);return true;
}

static void finish(Sled *s){
 if(s->finished)return;
 s->finished=true;
 // sendGameOver(place) pays [20,10,5,5][place-1]; there is no standing to pay without a finish.
 if(s->model.over&&s->model.place>=1&&s->model.place<=4)game_port_finish(s->port,s->model.place);else game_port_leave(s->port);
}

static void tick(void *game){
 Sled *s=game;if(!s->scene)return;SledModel *m=&s->model;
 if(s->messages==MESSAGES_READY&&s->readyTicks<60){s->readyTicks++;if(20+s->readyTicks>=68)m->started=true;}
 if(!s->finished){
  sled_model_step(m);
  // A held direction repeats like a held key, about eight steps a second. In dual screen the
  // stylus is a target lane: the sled steps toward it at the same rate and stops under it.
  if(s->stylus&&!s->keyDir&&m->me>=0){double dx=s->stylusX-m->racers[m->me].x;s->touchDir=dx>10?1:dx<-10?-1:0;}
  int held=s->keyDir?s->keyDir:s->touchDir;
  if(held&&++s->repeat%3==0)sled_model_turn(m,held);
  for(unsigned i=0;i<m->sendCount;i++){
   // Codes, y (rounded by movePlayer) and Math.round(time) are integers, as the web sends them.
   char me[12],code[24],y[24],time[24];snprintf(me,sizeof me,"%d",m->me);snprintf(code,sizeof code,"%.0f",m->sends[i].code);snprintf(y,sizeof y,"%.0f",m->sends[i].y);snprintf(time,sizeof time,"%.0f",floor(m->sends[i].time+.5));
   game_port_send(s->port,'z',"zm",4,(const char *[]){me,code,y,time});
  }
 }
 m->sendCount=0;swl_tick(s->scene);sync(s);
 // gameOver -> SHELL.sendGameOver(place), then back to the room.
 if(m->over)finish(s);
}

/** exitGame: confirm while racing, then sendJoinLastRoom without a payout. */
static void exit_game(Sled *s){if(s->finished)return;if(!s->model.started||s->model.over)finish(s);else s->asking=true;}
static void close_game(void *game){exit_game(game);}

static void frame(void *game,float seconds,const GameInput *input){
 Sled *s=game;(void)seconds;unsigned clicked=game_port_clicked(s->port);
 if(clicked==BUTTON_LEAVE){game_port_leave(s->port);return;}
 if(s->finished)return;
 if(s->asking){
  if(clicked==BUTTON_YES||(input->down&GAME_KEY_A)){s->asking=false;s->finished=true;game_port_leave(s->port);}
  else if(clicked==BUTTON_NO||(input->down&GAME_KEY_B)||s->model.over)s->asking=false;
  return;
 }
 if(clicked==BUTTON_EXIT){exit_game(s);return;}
 if(clicked==BUTTON_SCREENS||(input->down&GAME_KEY_SELECT)){s->dual=!s->dual;s->stylus=false;s->touchDir=0;return;}
 int dir=(input->held&(GAME_KEY_RIGHT|GAME_KEY_UP))||input->stickX>.5f?1:(input->held&(GAME_KEY_LEFT|GAME_KEY_DOWN))||input->stickX<-.5f?-1:0;
 // The key listener: one step per press, then the held key repeats.
 if(dir&&dir!=s->keyDir){sled_model_turn(&s->model,dir);s->repeat=0;}
 s->keyDir=dir;
}

// Lower screen: two steering pads under the status line, or in dual screen the stylus over our lane.
static void touch(void *game,GameTouch phase,float x,float y){
 Sled *s=game;
 if(s->dual){
  if(phase==GAME_TOUCH_UP||s->asking||s->finished){s->stylus=false;s->touchDir=0;return;}
  double lane=90+(x-LOWER_X)/LOWER_ZOOM;s->stylusX=lane<0?0:lane>180?180:lane;
  if(!s->stylus){s->stylus=true;s->repeat=2;}// the first step goes out on the next tick
  return;
 }
 if(phase==GAME_TOUCH_UP||y<60||s->asking||s->finished){s->touchDir=0;return;}
 int dir=x<160?-1:1;if(dir!=s->touchDir){sled_model_turn(&s->model,dir);s->repeat=0;}
 s->touchDir=dir;
}

static bool lower_view(void *game,SWLMatrix *view){
 Sled *s=game;if(!s->dual||!s->scene)return false;
 float cx,cy;lane_point(90,&cx,&cy);
 *view=(SWLMatrix){LOWER_ZOOM,0,0,LOWER_ZOOM,LOWER_X-LOWER_ZOOM*cx,LOWER_Y-LOWER_ZOOM*cy};
 return true;
}

static void draw_bottom(void *game){
 Sled *s=game;GamePort *p=s->port;const SledModel *m=&s->model;
 if(s->dual&&!s->asking&&!m->over&&!s->finished){
  // The scene is already on the lower screen: a slim HUD over it.
  game_port_rect(p,4,4,150,26,0x0b3d6e);
  game_port_text(p,12,9,11,0xffffff,GAME_ALIGN_LEFT,!m->started?(s->messages==MESSAGES_WAIT?"Waiting for players...":"Get ready!"):"Drag to steer");
  game_port_button(p,BUTTON_EXIT,244,4,72,26,"Exit",true);
  game_port_button(p,BUTTON_SCREENS,160,4,78,26,"Pads",true);
  // Target lane marker under the stylus.
  if(s->stylus)game_port_rect(p,LOWER_X+(float)(s->stylusX-90)*LOWER_ZOOM-2,200,4,10,0xffdd33);
  return;
 }
 game_port_rect(p,0,0,320,240,0x5d8fbf);
 if(s->asking){
  game_port_text(p,160,70,12,0xffffff,GAME_ALIGN_CENTER,"Are you sure you want to quit this game?");
  game_port_button(p,BUTTON_YES,50,120,100,40,"Yes",true);game_port_button(p,BUTTON_NO,170,120,100,40,"No",true);return;
 }
 if(m->over||s->finished){
  static const char *const places[]={"You did not finish.","1st place!","2nd place!","3rd place!","4th place!"};
  game_port_text(p,160,60,15,0xffffff,GAME_ALIGN_CENTER,places[m->place<=4?m->place:0]);
  if(!game_port_descriptor(p)->online)game_port_text(p,160,90,11,0xffffff,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
  bool waiting=game_port_session(p)==GAME_SESSION_PAYING&&!game_port_can_retry(p);
  if(waiting)game_port_text(p,160,90,11,0xffffff,GAME_ALIGN_CENTER,"Waiting for the server...");
  if(waiting&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,90,150,140,30,"Leave without waiting",true);
  return;
 }
 game_port_text(p,8,10,13,0xffffff,GAME_ALIGN_LEFT,!m->started?(s->messages==MESSAGES_WAIT?"Waiting for players...":"Get ready!"):"Race!");
 game_port_button(p,BUTTON_EXIT,244,6,72,26,"Exit",true);
 game_port_button(p,BUTTON_SCREENS,8,36,120,22,"Dual screen",true);
 game_port_rect(p,4,60,152,132,s->touchDir<0?0x3a6f9f:0x2a5f8f);game_port_text(p,80,120,14,0xffffff,GAME_ALIGN_CENTER,"Left");
 game_port_rect(p,164,60,152,132,s->touchDir>0?0x3a6f9f:0x2a5f8f);game_port_text(p,240,120,14,0xffffff,GAME_ALIGN_CENTER,"Right");
}

const GameController GAME_SLED_CONTROLLER={
 .key="sled",.kind=GAME_WADDLE,.banks={"games/sled.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.lower_view=lower_view,
};
