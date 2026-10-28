// Mancala controller: Mancala.swf at a room table (egyptian rules, drop mode) with
// Waddle's z-messages, as in the web Mancala (src/games/tables.ts).
#include "game_controller.h"
#include "game_mancala_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANCALA_STONES 48
#define MANCALA_MOVES 32
enum {STONE_REST=1,STONE_HAND=10,STONE_DROP=20,STONE_CAPTURE=30};
enum {ACTION_WAIT,ACTION_SEND,ACTION_MOVE,ACTION_END};
enum {MESSAGE_NONE,MESSAGE_FREE,MESSAGE_CAPTURE,MESSAGE_OVER};
enum {BUTTON_CLOSE=1,BUTTON_YES,BUTTON_NO,BUTTON_CUP=10};
static const unsigned MESSAGE_RANGE[4][2]={{0,0},{5,65},{70,130},{135,232}};

typedef struct {unsigned art,cup,frame,depth;double x,y;} Stone;
typedef struct {unsigned char player,cup;char command;} Move;
typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 char names[2][40];int seat,turn;bool started,over,leaving,asking;
 unsigned map[14];int board[14][MANCALA_STONES];unsigned counts[14];
 Stone stones[MANCALA_STONES];unsigned stoneCount,depth,scores[2];
 unsigned action;char turnCommand;int lastStone;bool endPending;
 // The authored step queues: one stone every `every` ticks.
 unsigned queueKind,queueEvery,queueWait;int hand[MANCALA_STONES];unsigned handCount;int nextCup;int captured[MANCALA_STONES];unsigned capturedCount;unsigned captureCup;
 unsigned message,messageFrame;
 int hintCup;unsigned hintStart,hintFrame;bool hintPlayer;
 int cursor;int slotOf[MANCALA_STONES];
 uint32_t seed;
 bool snapshot,awaiting,turnKnown,joined,coinsSeen,syncFailed;int serverTurn;float syncAt;
 Move moves[MANCALA_MOVES];unsigned moveFirst,moveCount;
} Mancala;

static bool number(const char *text,unsigned max,unsigned *out){if(!text||!*text)return false;unsigned n=0;for(const unsigned char *p=(const unsigned char *)text;*p;p++){if(*p<'0'||*p>'9'||n>max/10||(n==max/10&&(unsigned)(*p-'0')>max%10))return false;n=n*10+(unsigned)(*p-'0');}*out=n;return true;}
static bool board_values(const char *text,unsigned map[14]){if(!text)return false;memset(map,0,14*sizeof *map);if(!*text)return true;unsigned total=0;for(unsigned i=0;i<14;i++){if(*text<'0'||*text>'9')return false;unsigned n=0;while(*text>='0'&&*text<='9'){n=n*10+(unsigned)(*text++-'0');if(n>MANCALA_STONES)return false;}map[i]=n;total+=n;if(total>MANCALA_STONES)return false;if(i<13){if(*text++!=',')return false;}else if(*text)return false;}return true;}
static double random01(Mancala *m){uint32_t x=m->seed;x^=x<<13;x^=x>>17;x^=x<<5;m->seed=x;return (double)x/4294967296.0;}
static void target(Mancala *m,const char *path,unsigned frame,const char *label){if(m->scene)swl_target(m->scene,path,frame,label,false);}
static void text(Mancala *m,const char *path,const char *value){game_bank_text(m->bank,path,value);}
static bool my_turn(const Mancala *m){return m->snapshot&&!m->awaiting&&m->turnKnown&&!m->moveCount&&!m->endPending&&m->started&&!m->over&&m->action==ACTION_WAIT&&m->seat==m->turn;}
static void upper(char *out,size_t cap,const char *in){size_t i=0;for(;in[i]&&i+1<cap;i++)out[i]=(char)(in[i]>='a'&&in[i]<='z'?in[i]-32:in[i]);out[i]=0;}

static void update_players(Mancala *m){
 if(!m->scene)return;
 for(unsigned s=0;s<2;s++){
  char path[24],field[48],name[40],score[24];snprintf(path,sizeof path,"root.player%u",s);
  target(m,path,1,m->names[s][0]?(m->started&&m->turnKnown&&m->turn==(int)s?"Turn":"Wait"):NULL);
  upper(name,sizeof name,m->names[s]);
  snprintf(field,sizeof field,"%s.name1_txt",path);text(m,field,name);snprintf(field,sizeof field,"%s.name2_txt",path);text(m,field,name);
  snprintf(score,sizeof score,"Score: %u",m->scores[s]);snprintf(field,sizeof field,"%s.score_txt",path);text(m,field,score);
 }
}

static void place(Mancala *m,Stone *s,unsigned cup){
 const double *c=MANCALA_CUPS[cup],angle=random01(m)*3.141592653589793*2;
 s->cup=cup;s->x=c[0]+sin(angle)*floor(random01(m)*(c[2]/2));s->y=c[1]+cos(angle)*floor(random01(m)*(c[3]/2));
}

/** Stones in depth order fill the 48 slots each tick (the slot keeps a stone while its order holds). */
static void sync_stones(Mancala *m){
 unsigned order[MANCALA_STONES];for(unsigned i=0;i<m->stoneCount;i++)order[i]=i;
 for(unsigned i=1;i<m->stoneCount;i++){unsigned v=order[i],j=i;while(j&&m->stones[order[j-1]].depth>m->stones[v].depth){order[j]=order[j-1];j--;}order[j]=v;}
 bool snap=false;
 for(unsigned k=0;k<MANCALA_STONES;k++){
  char path[24],child[28];snprintf(path,sizeof path,"root.game.s%u",k);snprintf(child,sizeof child,"%s.p",path);
  if(k>=m->stoneCount){if(m->slotOf[k]!=-1){target(m,path,6,NULL);m->slotOf[k]=-1;}continue;}
  const Stone *s=&m->stones[order[k]];
  if(m->slotOf[k]!=(int)order[k]){if(m->slotOf[k]>=0)snap=true;m->slotOf[k]=(int)order[k];target(m,path,s->art+1,NULL);}
  target(m,child,s->frame,NULL);
  swl_transform(m->scene,path,(SWLMatrix){1,0,0,1,(float)s->x,(float)s->y});
 }
 if(snap)game_port_snap(m->port);
}

static void update_score(Mancala *m);
static void game_over(Mancala *m);
static void leave(Mancala *m,bool sendLeave){if(m->leaving)return;m->leaving=true;if(sendLeave)game_port_send(m->port,'z',"lz",0,NULL);game_port_leave(m->port);}
static void play_message(Mancala *m,unsigned message){m->message=message;m->messageFrame=MESSAGE_RANGE[message][0];target(m,"root.message",m->messageFrame,NULL);}

static void update_turn(Mancala *m){if(m->action==ACTION_END)return;m->turn=m->turn>0?0:1;m->action=ACTION_WAIT;update_players(m);}
static void update_score(Mancala *m){
 for(unsigned p=0;p<2;p++){m->scores[p]=0;for(unsigned cup=p*7;cup<=p*7+6;cup++)m->scores[p]+=m->counts[cup];}
 update_players(m);
}
static void game_over(Mancala *m){
 if(m->over)return;
 m->over=true;m->action=ACTION_END;
 for(unsigned s=0;s<2;s++){char name[40],line[64],path[40];upper(name,sizeof name,m->names[s]);snprintf(line,sizeof line,"%s: %u",name,m->scores[s]);
  snprintf(path,sizeof path,"root.message.score%u.message1",s);text(m,path,line);snprintf(path,sizeof path,"root.message.score%u.message2",s);text(m,path,line);}
 play_message(m,MESSAGE_OVER);
}

static void check_turn(Mancala *m,unsigned cup);
static void drop_stone(Mancala *m,int id,unsigned cup,bool capture){
 Stone *s=&m->stones[id];s->frame=capture?STONE_CAPTURE:STONE_DROP;
 game_port_sound(m->port,"sound_mancala_drop",cup==6||cup==13?50:20,false);
 place(m,s,cup);m->board[cup][m->counts[cup]++]=id;
 if(id==m->lastStone&&!capture)check_turn(m,cup);
}
static int check_next_cup(const Mancala *m,int cup){
 int skip=m->turn>0?6:13;
 if(cup<0)cup=13;
 if(cup==skip)cup++;
 return cup>13?0:cup<0?13:cup;
}
/** captureStones: the last stone and the opposite cup go to the mover's mancala. */
static void capture_stones(Mancala *m,unsigned cup){
 m->capturedCount=0;m->captureCup=m->turn>0?13:6;
 if(m->counts[cup])m->captured[m->capturedCount++]=m->board[cup][--m->counts[cup]];
 while(m->counts[12-cup])m->captured[m->capturedCount++]=m->board[12-cup][--m->counts[12-cup]];
 m->queueKind=2;m->queueEvery=2;m->queueWait=0;
}
static void check_turn(Mancala *m,unsigned cup){
 char command=m->turnCommand;m->turnCommand=0;
 if(command=='f'){m->action=ACTION_WAIT;update_score(m);play_message(m,MESSAGE_FREE);}
 else if(command=='c'||command=='q')capture_stones(m,cup);
 else{update_score(m);update_turn(m);}
}
/** moveStones: lift the cup, then drop one stone every third frame. */
static void move_stones(Mancala *m,unsigned cup){
 m->action=ACTION_MOVE;m->hintCup=-1;m->handCount=0;
 while(m->counts[cup]){int id=m->board[cup][--m->counts[cup]];m->hand[m->handCount++]=id;m->stones[id].frame=STONE_HAND;m->stones[id].depth=m->depth++;}
 m->lastStone=m->handCount?m->hand[0]:-1;m->nextCup=(int)cup+1;
 m->queueKind=1;m->queueEvery=3;m->queueWait=0;
}

static void replace_board(Mancala *m){
 memset(m->counts,0,sizeof m->counts);memset(m->board,0,sizeof m->board);memset(m->stones,0,sizeof m->stones);
 m->handCount=m->capturedCount=m->queueKind=m->queueWait=m->moveFirst=m->moveCount=0;m->lastStone=-1;m->action=ACTION_WAIT;m->message=0;m->hintCup=-1;
 target(m,"root.message",1,NULL);for(unsigned i=0;i<14;i++){char path[24];snprintf(path,sizeof path,"root.h%u",i);target(m,path,1,NULL);}
 for(unsigned i=0;i<MANCALA_STONES;i++)m->slotOf[i]=-2;
 unsigned id=0,art=0;
 for(unsigned cup=0;cup<14;cup++)for(unsigned i=0;i<m->map[cup]&&id<MANCALA_STONES;i++){
  Stone *s=&m->stones[id];*s=(Stone){.art=art,.frame=STONE_REST,.depth=id};place(m,s,cup);m->board[cup][m->counts[cup]++]=(int)id;id++;art=(art+1)%5;
 }
 m->stoneCount=id;m->depth=id;update_score(m);sync_stones(m);
}
static void begin(Mancala *m,int turn){if(!m->started){m->turn=m->serverTurn=turn;m->turnKnown=true;}m->started=true;update_players(m);}
static void fail(Mancala *m,const char *notice);
static void start_move(Mancala *m){
 if(!m->moveCount||m->queueKind||m->action==ACTION_MOVE||m->action==ACTION_END||m->over)return;
 Move next=m->moves[m->moveFirst];m->moveFirst=(m->moveFirst+1)%MANCALA_MOVES;m->moveCount--;
 if(!m->counts[next.cup]){fail(m,"The table board is out of sync. Please join again.");return;}
 m->turn=next.player;m->turnCommand=next.command;move_stones(m,next.cup);
}
static void fail(Mancala *m,const char *notice){const GameHost *h=game_port_host(m->port);if(h->notice)h->notice(h->user,notice);leave(m,true);}

static bool packet(void *game,const char *command,unsigned count,char *const *args){
 Mancala *m=game;
 if(!strcmp(command,"gz")){
  if(m->over||m->endPending)return true;
  unsigned map[14];if(count!=3||!board_values(args[2],map))return false;
  for(unsigned s=0;s<2;s++)snprintf(m->names[s],sizeof m->names[s],"%s",count>s?args[s]:"");
  if(!m->scene)return true;
  /* GZ includes earlier moves. Its board replaces animation and FIFO state at this barrier. */
  memcpy(m->map,map,sizeof map);m->snapshot=true;m->awaiting=false;if(m->turnKnown)m->turn=m->serverTurn;replace_board(m);
  if(m->seat>=0&&!m->joined){m->joined=true;if(!game_port_send(m->port,'z',"jz",0,NULL))m->syncFailed=true;}return true;
 }
 if(!strcmp(command,"jz")){unsigned seat;if(count!=1||!number(args[0],1,&seat))return false;if(m->seat>=0)m->seat=(int)seat;return true;}
 if(!strcmp(command,"uz")){unsigned seat;if(count<1||count>2||!number(args[0],1,&seat))return false;snprintf(m->names[seat],sizeof m->names[seat],"%s",count>1?args[1]:"");update_players(m);return true;}
 if(!strcmp(command,"sz")){unsigned turn=0;if(count>1||(count&&!number(args[0],1,&turn)))return false;begin(m,(int)turn);return true;}
 if(!strcmp(command,"zm")){
  unsigned player,cup;if(count<2||count>3||!number(args[0],1,&player)||!number(args[1],12,&cup)||cup==6||(player==0?cup>5:cup<7))return false;
  const char *value=count>2?args[2]:"";if(*value&&(value[1]||(*value!='f'&&*value!='c'&&*value!='q')))return false;
  if(m->over||m->endPending)return true;
  m->started=m->turnKnown=true;m->serverTurn=*value=='f'?(int)player:1-(int)player;
  if(!m->snapshot||m->awaiting)return true;
  if(m->moveCount==MANCALA_MOVES){fail(m,"Too many table moves arrived at once. Please join again.");return true;}
  m->moves[(m->moveFirst+m->moveCount)%MANCALA_MOVES]=(Move){(unsigned char)player,(unsigned char)cup,*value};m->moveCount++;start_move(m);return true;
 }
 if(!strcmp(command,"zo")){
  unsigned total=0;if(count>1||(count&&!number(args[0],0x7fffffffu,&total)))return false;
  if(m->endPending||m->over)return true;
  m->endPending=true;
  if(count&&!m->coinsSeen){m->coinsSeen=true;const GameHost *h=game_port_host(m->port);if(h->coins)h->coins(h->user,total);}
  if(!m->snapshot){m->over=true;leave(m,false);return true;}
  if(!m->moveCount&&!m->queueKind&&m->action==ACTION_WAIT)game_over(m);
  return true;
 }
 if(!strcmp(command,"cz")){m->over=true;leave(m,false);return true;}
 return false;
}

static void *create(GamePort *port){
 Mancala *m=calloc(1,sizeof *m);if(!m)return NULL;m->port=port;int seat=game_port_descriptor(port)->seat;m->seat=seat==99||seat<1?-1:seat-1;
 m->hintCup=-1;m->lastStone=-1;m->cursor=0;for(unsigned i=0;i<MANCALA_STONES;i++)m->slotOf[i]=-2;
 m->seed=0x27D4EB2Fu^(uint32_t)(uintptr_t)m;if(!m->seed)m->seed=9;return m;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Mancala *m=game;m->bank=game_port_bank(m->port,0);m->scene=game_bank_scene(m->bank);if(!m->scene)return false;
 target(m,"root.player0",1,NULL);target(m,"root.player1",1,NULL);sync_stones(m);
 m->awaiting=true;m->syncAt=game_port_clock(m->port);m->syncFailed=!game_port_send(m->port,'z',"gz",1,(const char *[]){""});
 return true;
}

static void tick(void *game){
 Mancala *m=game;if(!m->scene)return;swl_tick(m->scene);
 if(m->syncFailed||(m->awaiting&&game_port_clock(m->port)-m->syncAt>10)){fail(m,"Could not synchronize the table. Please join again.");return;}
 if(m->queueKind&&!m->queueWait--){
  m->queueWait=m->queueEvery-1;
  if(m->queueKind==1){
   if(!m->handCount)m->queueKind=0;
   else{int id=m->hand[--m->handCount];m->nextCup=check_next_cup(m,m->nextCup);drop_stone(m,id,(unsigned)m->nextCup,false);m->nextCup++;if(!m->handCount&&m->queueKind==1)m->queueKind=0;}
  }else if(m->capturedCount){
   int id=m->captured[--m->capturedCount];drop_stone(m,id,m->captureCup,true);
   if(!m->capturedCount){m->queueKind=0;play_message(m,MESSAGE_CAPTURE);}
  }else m->queueKind=0;
 }
 if(m->hintCup>=0){unsigned stop=m->hintPlayer?11:26;if(m->hintFrame<stop){m->hintFrame++;char path[24];snprintf(path,sizeof path,"root.h%d",m->hintCup);target(m,path,m->hintFrame,NULL);}}
 if(m->message){
  if(m->messageFrame<MESSAGE_RANGE[m->message][1])target(m,"root.message",++m->messageFrame,NULL);
  else{unsigned done=m->message;m->message=0;target(m,"root.message",1,NULL);
   if(done==MESSAGE_CAPTURE){update_score(m);update_turn(m);}else if(done==MESSAGE_OVER)leave(m,false);}
 }
 start_move(m);
 if(m->endPending&&!m->moveCount&&!m->queueKind&&m->action==ACTION_WAIT&&!m->over)game_over(m);
 sync_stones(m);
}

static void show_hint(Mancala *m,int cup){
 char path[24];
 if(m->hintCup>=0&&m->hintCup!=cup){snprintf(path,sizeof path,"root.h%d",m->hintCup);target(m,path,1,NULL);}
 if(cup<0||m->action!=ACTION_WAIT||!m->started||!m->counts[cup]){m->hintCup=-1;return;}
 if(cup==m->hintCup)return;
 bool other=m->seat!=m->turn||(m->turn==0&&cup>6)||(m->turn==1&&cup<7);
 m->hintCup=cup;m->hintPlayer=!other;m->hintFrame=other?20:5;
 snprintf(path,sizeof path,"root.h%d",cup);target(m,path,m->hintFrame,NULL);
 char box[48],count[8];snprintf(box,sizeof box,"%s.messageBox",path);snprintf(count,sizeof count,"%u",m->counts[cup]);text(m,box,count);
}
static void click_cup(Mancala *m,unsigned cup){
 if(!my_turn(m)||!m->counts[cup])return;
 unsigned mancala=m->turn>0?13:6;
 if(cup<mancala&&cup>=mancala-6){char c[12];snprintf(c,sizeof c,"%u",cup);m->action=ACTION_SEND;show_hint(m,-1);game_port_send(m->port,'z',"zm",1,(const char *[]){c});}
}
static void close_game(void *game){Mancala *m=game;if(m->leaving)return;if(!m->started||m->over||m->seat<0){leave(m,!m->over);return;}m->asking=true;}

// The player's own row on the lower screen, left to right as seen on the board.
static unsigned own_cup(const Mancala *m,int i){return m->seat==1?(unsigned)(12-i):(unsigned)i;}
static void frame(void *game,float seconds,const GameInput *input){
 Mancala *m=game;(void)seconds;unsigned clicked=game_port_clicked(m->port);
 if(m->asking){if(clicked==BUTTON_YES||(input->down&GAME_KEY_A))leave(m,true);else if(clicked==BUTTON_NO||(input->down&GAME_KEY_B))m->asking=false;return;}
 if(clicked==BUTTON_CLOSE){close_game(m);return;}
 if(clicked>=BUTTON_CUP&&clicked<BUTTON_CUP+14){click_cup(m,clicked-BUTTON_CUP);return;}
 if((input->down&GAME_KEY_LEFT)&&m->cursor>0)m->cursor--;
 if((input->down&GAME_KEY_RIGHT)&&m->cursor<5)m->cursor++;
 if(m->seat>=0&&m->action==ACTION_WAIT)show_hint(m,(int)own_cup(m,m->cursor));
 if(input->down&GAME_KEY_A)click_cup(m,own_cup(m,m->cursor));
}

static void draw_bottom(void *game){
 Mancala *m=game;GamePort *p=m->port;char line[96];
 game_port_rect(p,0,0,320,m->asking?240:52,0x4a2e12);
 if(m->asking){game_port_text(p,160,70,13,0xffffff,GAME_ALIGN_CENTER,"Are you sure you want to quit this game?");game_port_button(p,BUTTON_YES,50,120,100,40,"Yes",true);game_port_button(p,BUTTON_NO,170,120,100,40,"No",true);return;}
 const char *state=!m->started?(m->seat<0?"Watching":"Waiting for an opponent..."):m->over?"Game over":my_turn(m)?"Your turn":m->seat<0?"Watching":"Opponent's turn";
 snprintf(line,sizeof line,"%s %u - %u %s",m->names[0][0]?m->names[0]:"-",m->scores[0],m->scores[1],m->names[1][0]?m->names[1]:"-");
 game_port_text(p,8,8,11,0xffffff,GAME_ALIGN_LEFT,line);game_port_text(p,8,26,13,0xffdd33,GAME_ALIGN_LEFT,state);
 game_port_button(p,BUTTON_CLOSE,244,6,72,26,"Close",true);
 for(unsigned cup=0;cup<14;cup++){
  const double *c=MANCALA_CUPS[cup];float x=(float)(c[0]+10)*1.3f-35,y=(float)(c[1]+10)*1.3f-10;
  if(cup==own_cup(m,m->cursor)&&my_turn(m))game_port_rect(p,x-12,y+16,24,3,0x086aa6);
  char count[8];snprintf(count,sizeof count,"%u",m->counts[cup]);float labelY=cup==6||cup==13?190:cup<6?y+23:y-34;
  game_port_rect(p,x-12,labelY-2,24,16,0xffffff);game_port_text(p,x,labelY,11,0x4a2e12,GAME_ALIGN_CENTER,count);
 }
 game_port_rect(p,0,216,320,24,0x4a2e12);game_port_text(p,160,222,11,0xffffff,GAME_ALIGN_CENTER,"Tap your cups. Left/Right + A: sow stones");
}

static void touch(void *game,GameTouch phase,float x,float y){
 Mancala *m=game;if(phase!=GAME_TOUCH_DOWN||m->asking||!my_turn(m))return;
 for(int i=0;i<6;i++){unsigned cup=own_cup(m,i);const double *c=MANCALA_CUPS[cup];float cx=(float)(c[0]+10)*1.3f-35,cy=(float)(c[1]+10)*1.3f-10;if(fabsf(x-cx)<=18&&fabsf(y-cy)<=21){m->cursor=i;click_cup(m,cup);return;}}
}
static bool lower_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){1.3f,0,0,1.3f,-35,-10};return true;}
static bool top_view(void *game,SWLMatrix *view){(void)game;float scale=240.0f/224;*view=(SWLMatrix){scale,0,0,scale,(400-300*scale)/2,0};return true;}

const GameController GAME_MANCALA_CONTROLLER={
 .key="mancala",.kind=GAME_TABLE,.stageWidth=300,.stageHeight=224,.backgroundRGB=0xffffff,.banks={"games/mancala.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.touch=touch,.lower_view=lower_view,.top_view=top_view,
};
