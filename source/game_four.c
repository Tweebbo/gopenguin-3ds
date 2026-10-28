// Find Four controller: FindFour.swf at a room table, with Waddle's z-messages
// (gz, jz, uz, sz, zm, zo, cz, lz) as in the web FindFour (src/games/tables.ts).
#include "game_controller.h"
#include "game_four.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOUR_SPACE 24.3f
enum {BUTTON_CLOSE=1,BUTTON_YES,BUTTON_NO,BUTTON_COLUMN=10};

typedef struct {unsigned player,frame;bool drop;} Coin;
typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 char names[2][40];int seat,turn;bool started,over,leaving,asking;
 unsigned map[FOUR_W][FOUR_H];Coin coins[FOUR_W][FOUR_H];
 bool win;unsigned winX,winY,winD,winFrame,winHold;
 unsigned column;char notice[96];unsigned noticeTicks;int lastX,lastY;
 bool snapshot,awaiting,turnKnown,joined,coinsSeen,syncFailed;float syncAt;
} Four;

static bool number(const char *text,unsigned max,unsigned *out){if(!text||!*text)return false;unsigned n=0;for(const unsigned char *p=(const unsigned char *)text;*p;p++){if(*p<'0'||*p>'9'||n>max/10||(n==max/10&&(unsigned)(*p-'0')>max%10))return false;n=n*10+(unsigned)(*p-'0');}*out=n;return true;}
static bool board_values(const char *text,unsigned map[FOUR_W][FOUR_H]){if(!text)return false;memset(map,0,sizeof(unsigned)*FOUR_W*FOUR_H);if(!*text)return true;for(unsigned i=0;i<FOUR_W*FOUR_H;i++){if(*text<'0'||*text>'2')return false;map[i/FOUR_H][i%FOUR_H]=(unsigned)(*text++-'0');if(i+1<FOUR_W*FOUR_H){if(*text++!=',')return false;}else if(*text)return false;}return true;}
static void target(Four *f,const char *path,unsigned frame,const char *label){if(f->scene)swl_target(f->scene,path,frame,label,false);}
static bool dropping(const Four *f){for(unsigned x=0;x<FOUR_W;x++)for(unsigned y=0;y<FOUR_H;y++)if(f->map[x][y]&&f->coins[x][y].frame<y+2)return true;return false;}
static bool my_turn(const Four *f){return f->snapshot&&!f->awaiting&&f->turnKnown&&f->started&&!f->over&&f->seat==f->turn&&!dropping(f);}

static void update_players(Four *f){
 if(!f->scene)return;
 for(unsigned s=0;s<2;s++){
  char path[32],field[48],upper[40];snprintf(path,sizeof path,"root.player%u_mc",s);
  target(f,path,1,f->names[s][0]?(f->started&&f->turnKnown&&f->turn==(int)s?"Turn":"Wait"):NULL);
  size_t i=0;for(;f->names[s][i]&&i<sizeof upper-1;i++)upper[i]=(char)(f->names[s][i]>='a'&&f->names[s][i]<='z'?f->names[s][i]-32:f->names[s][i]);upper[i]=0;
  snprintf(field,sizeof field,"%s.name1_txt",path);game_bank_text(f->bank,field,upper);
  snprintf(field,sizeof field,"%s.name2_txt",path);game_bank_text(f->bank,field,upper);
 }
}

static void show_coin(Four *f,unsigned x,unsigned y){
 char path[24],child[28];unsigned k=x*FOUR_H+y;snprintf(path,sizeof path,"root.game_mc.k%u",k);snprintf(child,sizeof child,"%s.c",path);
 target(f,path,f->map[x][y]+1,NULL);target(f,child,f->coins[x][y].frame,NULL);
 swl_transform(f->scene,path,(SWLMatrix){1,0,0,1,x*FOUR_SPACE,0});
}

static void add_coin(Four *f,unsigned player,unsigned x,unsigned y,bool drop){
 if(x>=FOUR_W||y>=FOUR_H||player<1||player>2)return;
 f->map[x][y]=player;f->coins[x][y]=(Coin){player,drop?1:y+2,drop};show_coin(f,x,y);
 if(!drop)return;
 f->turn=2-(int)player;
 target(f,"root.game_mc.turn_mc",f->seat==f->turn?(unsigned)f->turn+2:1,NULL);
 update_players(f);
}

bool four_find_win(const unsigned map[FOUR_W][FOUR_H],int lastX,int lastY,unsigned *ax,unsigned *ay,unsigned *ad){
 if(lastX<0||lastY<0||lastX>=FOUR_W||lastY>=FOUR_H)return false;
 unsigned value=map[lastX][lastY];if(!value)return false;
 static const int dirs[4][3]={{1,0,2},{0,1,1},{1,1,3},{1,-1,4}};
 bool found=false;double best=0;
 for(unsigned k=0;k<4;k++){
  int dx=dirs[k][0],dy=dirs[k][1],d=dirs[k][2];
  int back=0,fwd=0,x=lastX-dx,y=lastY-dy;
  while(x>=0&&x<FOUR_W&&y>=0&&y<FOUR_H&&map[x][y]==value){back++;x-=dx;y-=dy;}
  x=lastX+dx;y=lastY+dy;while(x>=0&&x<FOUR_W&&y>=0&&y<FOUR_H&&map[x][y]==value){fwd++;x+=dx;y+=dy;}
  int total=back+1+fwd;if(total<4)continue;
  int sx0=lastX-dx*back,sy0=lastY-dy*back;
  for(int i=back-3>0?back-3:0;i<=(back<total-4?back:total-4);i++){
   int sx=sx0+dx*i,sy=sy0+dy*i,minY=sy,minX=sx,maxX=sx;
   for(int n=1;n<4;n++){int px=sx+dx*n,py=sy+dy*n;if(py<minY)minY=py;if(px<minX)minX=px;if(px>maxX)maxX=px;}
   double cx=sx+dx*1.5-lastX,cy=sy+dy*1.5-lastY,dist=cx*cx+cy*cy;
   if(!found||dist<best){found=true;best=dist;*ax=(unsigned)(d==4?maxX:minX);*ay=(unsigned)minY;*ad=(unsigned)d;}
  }
 }
 return found;
}

static void leave(Four *f,bool sendLeave){
 if(f->leaving)return;
 f->leaving=true;if(sendLeave)game_port_send(f->port,'z',"lz",0,NULL);
 game_port_leave(f->port);
}

static bool packet(void *game,const char *command,unsigned count,char *const *args){
 Four *f=game;
 if(!strcmp(command,"gz")){
  if(f->over)return true;
  unsigned map[FOUR_W][FOUR_H];if(count!=3||!board_values(args[2],map))return false;
  for(unsigned s=0;s<2;s++)snprintf(f->names[s],sizeof f->names[s],"%s",count>s?args[s]:"");
  if(!f->scene)return true;
  /* The ordered GZ response includes earlier ZMs. Replace the board; never replay those moves. */
  memcpy(f->map,map,sizeof map);memset(f->coins,0,sizeof f->coins);f->win=false;f->winFrame=f->winHold=0;target(f,"root.game_mc.win",1,NULL);
  unsigned players[2]={0};for(unsigned x=0;x<FOUR_W;x++)for(unsigned y=0;y<FOUR_H;y++){
   unsigned v=map[x][y];if(v){players[v-1]++;f->coins[x][y]=(Coin){v,y+2,false};show_coin(f,x,y);}else{char path[24];snprintf(path,sizeof path,"root.game_mc.k%u",x*FOUR_H+y);target(f,path,1,NULL);}
  }
  if(players[0]+players[1]){f->started=true;f->turnKnown=players[0]==players[1]||players[0]==players[1]+1;f->turn=players[0]==players[1]?0:1;}
  f->snapshot=true;f->awaiting=false;
  update_players(f);
  target(f,"root.game_mc.turn_mc",f->started&&f->turnKnown&&f->seat==f->turn?(unsigned)f->turn+2:1,NULL);
  if(f->seat>=0&&!f->joined){f->joined=true;if(!game_port_send(f->port,'z',"jz",0,NULL))f->syncFailed=true;}
  return true;
 }
 if(!strcmp(command,"jz")){unsigned seat;if(count!=1||!number(args[0],1,&seat))return false;if(f->seat>=0)f->seat=(int)seat;return true;}
 if(!strcmp(command,"uz")){unsigned seat;if(count<1||count>2||!number(args[0],1,&seat))return false;snprintf(f->names[seat],sizeof f->names[seat],"%s",count>1?args[1]:"");update_players(f);return true;}
 if(!strcmp(command,"sz")){unsigned turn=0;if(count>1||(count&&!number(args[0],1,&turn)))return false;if(!f->started){f->turn=(int)turn;f->turnKnown=true;}f->started=true;update_players(f);if(f->snapshot)target(f,"root.game_mc.turn_mc",f->turnKnown&&f->seat==f->turn?(unsigned)f->turn+2:1,NULL);return true;}
 if(!strcmp(command,"zm")){unsigned player,x,y;if(count!=3||!number(args[0],1,&player)||!number(args[1],FOUR_W-1,&x)||!number(args[2],FOUR_H-1,&y))return false;if(f->over)return true;f->started=f->turnKnown=true;f->lastX=(int)x;f->lastY=(int)y;f->turn=1-(int)player;if(f->snapshot&&!f->awaiting)add_coin(f,player+1,x,y,true);return true;}
 if(!strcmp(command,"zo")){
  unsigned total=0;if(count==1&&!number(args[0],0x7fffffffu,&total))return false;
  if(f->over)return true;
  f->over=true;
  if(count==1&&!f->coinsSeen){f->coinsSeen=true;const GameHost *h=game_port_host(f->port);if(h->coins)h->coins(h->user,total);}
  if(!f->snapshot){leave(f,false);return true;}
  target(f,"root.game_mc.turn_mc",1,NULL);
  // Pre-CPIP servers send the winning line; later ones send coins and the line comes from the board.
  if(count>=3){unsigned x,y,d;if(number(args[0],FOUR_W-1,&x)&&number(args[1],FOUR_H-1,&y)&&number(args[2],4,&d)&&d){f->win=true;f->winX=x;f->winY=y;f->winD=d;}}
  else{
   if(four_find_win((const unsigned (*)[FOUR_H])f->map,f->lastX,f->lastY,&f->winX,&f->winY,&f->winD))f->win=true;
  }
  if(!f->win)f->winHold=36; // web: close 1.5 s later
  return true;
 }
 if(!strcmp(command,"cz")){f->over=true;snprintf(f->notice,sizeof f->notice,"%.63s has quit the game.",count?args[0]:"Your opponent");const GameHost *h=game_port_host(f->port);if(h->notice)h->notice(h->user,f->notice);leave(f,false);return true;}
 return false;
}

static void *create(GamePort *port){
 Four *f=calloc(1,sizeof *f);if(!f)return NULL;f->port=port;
 int seat=game_port_descriptor(port)->seat;f->seat=seat==99||seat<1?-1:seat-1;f->column=3;f->lastX=f->lastY=-1;return f;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Four *f=game;f->bank=game_port_bank(f->port,0);f->scene=game_bank_scene(f->bank);if(!f->scene)return false;
 target(f,"root.player0_mc",1,NULL);target(f,"root.player1_mc",1,NULL);target(f,"root.game_mc.turn_mc",1,NULL);
 f->awaiting=true;f->syncAt=game_port_clock(f->port);f->syncFailed=!game_port_send(f->port,'z',"gz",1,(const char *[]){""});
 return true;
}

static void tick(void *game){
 Four *f=game;if(!f->scene)return;swl_tick(f->scene);
 if(f->syncFailed||(f->awaiting&&game_port_clock(f->port)-f->syncAt>10)){const GameHost *h=game_port_host(f->port);if(h->notice)h->notice(h->user,"Could not synchronize the table. Please join again.");leave(f,true);return;}
 for(unsigned x=0;x<FOUR_W;x++)for(unsigned y=0;y<FOUR_H;y++){
  Coin *c=&f->coins[x][y];if(!f->map[x][y]||c->frame>=y+2)continue;
  if(++c->frame==y+2)game_port_sound(f->port,"sound_drop",50,false);
  show_coin(f,x,y);
 }
 if(f->win&&!dropping(f)&&f->winFrame<40){
  if(!f->winFrame++)game_port_sound(f->port,"sound_win",50,false);
  target(f,"root.game_mc.win",f->winD+1,NULL);target(f,"root.game_mc.win.w",f->winFrame,NULL);
  swl_transform(f->scene,"root.game_mc.win",(SWLMatrix){1,0,0,1,f->winX*FOUR_SPACE,f->winY*FOUR_SPACE});
  // coinwin's frame 40 calls gameOverDone(): the widget closes.
  if(f->winFrame==40)f->winHold=15;
 }
 if(f->winHold&&!--f->winHold)leave(f,false);
 if(f->noticeTicks&&!--f->noticeTicks)leave(f,false);
}

static void drop(Four *f,unsigned x){
 if(!my_turn(f))return;
 for(int y=FOUR_H-1;y>=0;y--)if(!f->map[x][y]){char cx[12],cy[12];snprintf(cx,sizeof cx,"%u",x);snprintf(cy,sizeof cy,"%d",y);game_port_send(f->port,'z',"zm",2,(const char *[]){cx,cy});return;}
 game_port_sound(f->port,"sound_gromb",50,false);
}

/** closeGame: leaving a started game asks first. */
static void close_game(void *game){
 Four *f=game;if(f->leaving)return;
 if(!f->started||f->over||f->seat<0){leave(f,!f->over);return;}
 f->asking=true;
}

static void frame(void *game,float seconds,const GameInput *input){
 Four *f=game;(void)seconds;unsigned clicked=game_port_clicked(f->port);
 if(f->asking){if(clicked==BUTTON_YES||(input->down&GAME_KEY_A))leave(f,true);else if(clicked==BUTTON_NO||(input->down&GAME_KEY_B))f->asking=false;return;}
 if(clicked==BUTTON_CLOSE){close_game(f);return;}
 if(clicked>=BUTTON_COLUMN&&clicked<BUTTON_COLUMN+FOUR_W){f->column=clicked-BUTTON_COLUMN;drop(f,f->column);return;}
 if((input->down&GAME_KEY_LEFT)&&f->column>0)f->column--;
 if((input->down&GAME_KEY_RIGHT)&&f->column<FOUR_W-1)f->column++;
 if(input->down&GAME_KEY_A)drop(f,f->column);
}

static void draw_bottom(void *game){
 Four *f=game;GamePort *p=f->port;char line[96];
 if(f->asking){
  game_port_rect(p,0,0,320,240,0x003366);
  game_port_text(p,160,70,13,0xffffff,GAME_ALIGN_CENTER,"Are you sure you want to quit this game?");
  game_port_button(p,BUTTON_YES,50,120,100,40,"Yes",true);game_port_button(p,BUTTON_NO,170,120,100,40,"No",true);return;
 }
 if(f->noticeTicks){game_port_rect(p,0,0,320,240,0x003366);game_port_text(p,160,100,13,0xffffff,GAME_ALIGN_CENTER,f->notice);return;}
 const char *state=!f->started?(f->seat<0?"Watching":"Waiting for an opponent..."):f->over?"Game over":my_turn(f)?"Your turn":f->seat<0?"Watching":"Opponent's turn";
 snprintf(line,sizeof line,"%s vs %s",f->names[0][0]?f->names[0]:"-",f->names[1][0]?f->names[1]:"-");
 game_port_rect(p,0,0,320,52,0x003366);game_port_text(p,8,8,11,0xffffff,GAME_ALIGN_LEFT,line);game_port_text(p,8,29,12,0xffdd33,GAME_ALIGN_LEFT,state);
 game_port_button(p,BUTTON_CLOSE,244,6,72,26,"Close",true);
 if(my_turn(f))game_port_rect(p,67.2f+f->column*FOUR_SPACE*1.1f,53,25,2,0xffdd33);
 game_port_rect(p,0,229,320,11,0x003366);game_port_text(p,160,230,10,0xffffff,GAME_ALIGN_CENTER,"Tap the board. Left/Right + A: drop a coin");
}

static void touch(void *game,GameTouch phase,float x,float y){Four *f=game;if(phase!=GAME_TOUCH_DOWN||f->asking||f->noticeTicks||y<56||y>=229)return;int column=(int)((x-66.335f)/(FOUR_SPACE*1.1f));if(x>=66.335f&&column>=0&&column<FOUR_W){f->column=(unsigned)column;drop(f,f->column);}}
static bool top_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){1.4f,0,0,1.4f,57.2f,-40};return true;}
static bool lower_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){1.1f,0,0,1.1f,47.8f,17};return true;}

const GameController GAME_FOUR_CONTROLLER={
 .key="four",.kind=GAME_TABLE,.stageWidth=204,.stageHeight=280,.backgroundRGB=0x00529b,.banks={"games/four.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.touch=touch,.top_view=top_view,.lower_view=lower_view,
};
