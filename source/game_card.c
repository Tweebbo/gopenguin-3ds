#include "game_features.h"
#ifdef GP_NINJA_GAMES
#include "game_controller.h"
#include "game_card_model.h"
#include "game_card.h"
#include "game_card_art.h"
#include "game_card_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CARD_SHOT
#define CARD_SHOT 0
#endif

/* Card-Jitsu on two screens, drawn only from original art (tools/card-art.mjs):
 *  bank 2 (dojo bank, Sensei menu pack): frame 1 dojo background, frame 2 Sensei menu scene, frame 3 options scroll;
 *  bank 1 (fighters) over the dojo;
 *  bank 0 (board bank, card template): frame 1 board panel, names, hands, played cards and won-card thumbnails
 *          over the fighters, frame 2 the touch-screen hand with the original clock, frame 3 the help scroll.
 *  Upper = dojo + fighters + board (or the Sensei menu); lower = the hand, the options scroll or the help scroll. */
enum {BUTTON_SENSEI=1,BUTTON_MATCH,BUTTON_BACK,BUTTON_HELP,BUTTON_PICK,BUTTON_STARTER};
enum {FRAME_FRONT=1,FRAME_DISABLED,FRAME_OVER,FRAME_POW,FRAME_BACK,FRAME_THUMB};
enum {ROOT_BATTLE=1,ROOT_HAND,ROOT_HELP};
enum {DOJO_SCENE=1,DOJO_MENU,DOJO_OPTIONS};
enum {BANK_BOARD,BANK_FIGHTERS,BANK_DOJO};
static const uint32_t COLOURS[]={0x003366,0x003366,0x009900,0xff3399,0x333333,0xcc0000,0xff6600,0xffcc00,0x660099,0x996600,0xff6666,0x006600,0x0099cc,0x8ae302,0x93a0a4,0x02a797};
static const uint32_t CARD_COLOURS[]={1132705,6404422,16225579,10721738,14826534,16509741};
/* Chrome colours sampled from the original board: wood frame, paper and ink. */
#define WOOD 0x5a3a22u
#define WOOD_LIGHT 0x8a6440u
#define PAPER 0xefe7d2u
#define INK 0x2b1a0eu
/* Lower-screen hand: stage units (1 px = 760/320 stage units), matching the frame-2 layout in tools/card-art.mjs. */
#define HAND_X 12.0f
#define HAND_STEP 149.6f
#define HAND_Y 380.0f
#define HAND_SCALE .6f
#define LOWER_SCALE (320.0f/760)
static const char *const POWERS[]={"",
 "When played, lower values win ties next round.",
 "When scored, your card gets +2 next round.",
 "When scored, your opponent's card gets -2 next round.",
 "When scored, discard one opponent Snow card.",
 "When scored, discard one opponent Water card.",
 "When scored, discard one opponent Fire card.",
 "When scored, discard one opponent Red card.",
 "When scored, discard one opponent Blue card.",
 "When scored, discard one opponent Green card.",
 "When scored, discard one opponent Yellow card.",
 "When scored, discard one opponent Orange card.",
 "When scored, discard one opponent Purple card.",
 "When scored, Snow cannot be played next round.",
 "When scored, Fire cannot be played next round.",
 "When scored, Water cannot be played next round.",
 "When played, Water cards become Fire this round.",
 "When played, Snow cards become Water this round.",
 "When played, Fire cards become Snow this round.",
 "When scored, discard all opponent Red cards.",
 "When scored, discard all opponent Blue cards.",
 "When scored, discard all opponent Green cards.",
 "When scored, discard all opponent Yellow cards.",
 "When scored, discard all opponent Orange cards.",
 "When scored, discard all opponent Purple cards."};
typedef struct {
 GamePort *port;GameBank *bank;SWLScene *scene,*actors,*dojo;CardModel model;
 unsigned focus,deck,percentage;bool joined,queried,deckKnown,starterRequested,left,help,stylus,matched;
 float opening;int anim;unsigned root,clock;
} CardGame;
const CardModel *game_card_model(const void *game){return &((const CardGame *)game)->model;}
static void sync(CardGame *g);
static bool card_send(CardGame *g,char extension,const char *command,unsigned count,const char *const *args){return game_port_send(g->port,extension,command,count,args);}
static void close_game(void *game){
 CardGame *g=game;if(g->left)return;g->left=true;
 if(g->model.phase==CARD_QUEUE)card_send(g,'z',"lmm",0,NULL);
 else if(g->matched)card_send(g,'z',"lz",0,NULL);
 game_port_leave(g->port);
}
static unsigned icon(unsigned type){for(unsigned i=0;i<CARD_ICON_COUNT;i++)if(CARD_ICON_IDS[i]==type)return i+1;return 0;}
static const CardInfo *info(unsigned type){for(unsigned i=0;i<CARD_CATALOG_COUNT;i++)if(CARD_CATALOG[i].id==type)return &CARD_CATALOG[i];return NULL;}
static SWLColor rgb(uint32_t value){return (SWLColor){{0,0,0,1},{(float)(value>>16&255),(float)(value>>8&255),(float)(value&255),0}};}
static SWLMatrix place(float x,float y,float scale){return (SWLMatrix){scale,0,0,scale,x,y};}
static int shown_value(const CardGame *g,const CardItem *item){
 return (int)item->value+((item->held||g->model.picked[item->seat]==item->session)?g->model.modifier[item->seat]:0);
}
/** One instance of the original card template (frames front/disabled/over/pow/back/thumbnail). */
static void card_view(CardGame *g,const char *name,const CardItem *item,unsigned frame){
 char base[64],path[96];snprintf(base,sizeof base,"root.%s",name);unsigned art=item?icon(item->type):0;
 swl_visible(g->scene,base,item&&art);if(!item||!art)return;
 swl_target(g->scene,base,frame,NULL,false);
 snprintf(path,sizeof path,"%s.mc_art",base);swl_visible(g->scene,path,frame!=FRAME_BACK&&frame!=FRAME_THUMB);swl_target(g->scene,path,art,NULL,false);
 if(frame==FRAME_BACK)return;
 snprintf(path,sizeof path,"%s.mc_atr",base);swl_target(g->scene,path,item->element=='f'?1:item->element=='w'?2:3,NULL,false);
 const char *c=strchr("bgopry",item->colour);snprintf(path,sizeof path,"%s.mc_col",base);swl_color(g->scene,path,rgb(CARD_COLOURS[c&&*c?(unsigned)(c-"bgopry"):0]));
 if(frame==FRAME_THUMB)return;
 snprintf(path,sizeof path,"%s.card_glow",base);swl_visible(g->scene,path,item->power>0);
 char value[12];snprintf(value,sizeof value,"%d",shown_value(g,item));snprintf(path,sizeof path,"%s.mc_pt.tf_pt",base);game_bank_text(g->bank,path,value);
}
static void tint_players(CardGame *g){
 static const char *const parts[]={"body_mc","frontArm_mc","backArm_mc"};
 for(unsigned seat=0;seat<2;seat++){
  char player[32];snprintf(player,sizeof player,"root.fighter%u",seat);swl_visible(g->actors,player,seat==0||g->model.phase!=CARD_MENU||!game_port_descriptor(g->port)->fromSensei);
  char path[80];unsigned colour=g->model.colour[seat];if(colour>=sizeof COLOURS/sizeof *COLOURS)colour=1;
  for(unsigned i=0;i<3;i++){snprintf(path,sizeof path,"root.fighter%u.%s",seat,parts[i]);swl_color(g->actors,path,rgb(COLOURS[colour]));}
  bool sensei=g->model.mode==1&&seat==0;
  for(unsigned i=0;i<3;i++){static const char *const names[]={"sensay_mc","sensay2_mc","sensay_mc2"};snprintf(path,sizeof path,"root.fighter%u.%s",seat,names[i]);swl_visible(g->actors,path,sensei);}
  static const uint32_t belts[]={0xffffff,0xffffff,0xf6d82b,0xe58425,0x6e3d92,0x45a340,0x1d70c2,0xb72c2d,0x70422c,0x222222,0x222222};
  snprintf(path,sizeof path,"root.fighter%u.belt_mc",seat);swl_visible(g->actors,path,!sensei&&g->model.rank[seat]>0);swl_color(g->actors,path,rgb(belts[g->model.rank[seat]<=10?g->model.rank[seat]:0]));
  snprintf(path,sizeof path,"root.fighter%u.beltline_mc",seat);swl_visible(g->actors,path,!sensei&&g->model.rank[seat]>0);
 }
}
static bool in_menu(const CardGame *g){return g->model.phase==CARD_MENU;}
static bool hand_live(const CardModel *m){return m->phase==CARD_SELECT||m->phase==CARD_PICK_WAIT;}
static unsigned clock_seconds(const CardModel *m){return m->elapsed<20?(unsigned)(20-m->elapsed):0;}
static void sync_clock(CardGame *g){
 CardModel *m=&g->model;bool show=m->phase==CARD_SELECT&&!m->choicePending;unsigned seconds=clock_seconds(m);
 swl_visible(g->scene,"root.mc_clock",show);if(!show)return;
 if(g->clock==seconds+1)return;
 g->clock=seconds+1;
 unsigned frame=21-(seconds?seconds:1);swl_target(g->scene,"root.mc_clock",frame,NULL,false);
 char text[8];snprintf(text,sizeof text,"%u",seconds);game_bank_text(g->bank,"root.mc_clock.timer_txt",text);
}
static void sync(CardGame *g){
 CardModel *m=&g->model;int me=m->seat<0?(m->mode==1?1:0):m->seat;
 unsigned root=in_menu(g)?DOJO_MENU:DOJO_SCENE;
 if(root!=g->root){g->root=root;swl_target(g->dojo,"root",root,NULL,false);}
 if(g->focus>=m->hands[me])g->focus=0;
 /* Lower screen: the hand as large card faces; the focused card is raised in its 'over' frame, blocked cards use 'disabled'. */
 for(unsigned i=0;i<CARD_HAND;i++){
  char name[16];snprintf(name,sizeof name,"hand%u",i);const CardItem *item=i<m->hands[me]?card_model_find(m,m->hand[me][i]):NULL;
  bool blocked=item&&item->element==m->blocked[me],focused=item&&i==g->focus&&m->phase==CARD_SELECT&&!m->choicePending;
  card_view(g,name,item,blocked?FRAME_DISABLED:focused?FRAME_OVER:FRAME_FRONT);
  float scale=focused?HAND_SCALE*1.08f:HAND_SCALE,x=HAND_X+HAND_STEP*i-(focused?234.6f*(scale-HAND_SCALE)/2:0);
  char path[24];snprintf(path,sizeof path,"root.%s",name);swl_transform(g->scene,path,place(x,focused?HAND_Y-40:HAND_Y,scale));
 }
 /* Upper board: each seat's hand in its original slot (own cards face up, opponent's backs). */
 for(unsigned s=0;s<2;s++){
  bool front=(int)s==me;
  for(unsigned i=0;i<CARD_HAND;i++){
   char name[24],path[40];snprintf(name,sizeof name,"seat%u_%u",s,i);const CardItem *item=i<m->hands[s]&&!in_menu(g)?card_model_find(m,m->hand[s][i]):NULL;
   bool focused=front&&item&&i==g->focus&&m->phase==CARD_SELECT&&!m->choicePending;
   card_view(g,name,item,!front?FRAME_BACK:item&&item->element==m->blocked[s]?FRAME_DISABLED:focused?FRAME_OVER:FRAME_FRONT);
   float step=front?75:35,x=s==0?50+i*step:710-(i+1)*step;snprintf(path,sizeof path,"root.%s",name);
   swl_transform(g->scene,path,place(x,focused?375:385,front?.28f:.15f));
  }
  /* Played card: face down until the reveal, then into the won pile. */
  char name[24];snprintf(name,sizeof name,"pick%u",s);const CardItem *pick=in_menu(g)?NULL:card_model_find(m,m->picked[s]);
  if(pick&&pick->scored&&m->phase==CARD_REVEAL&&m->elapsed>=2)pick=NULL;
  bool hidden=pick&&(int)s!=me&&m->phase!=CARD_REVEAL&&m->phase!=CARD_OVER;
  card_view(g,name,pick,hidden?FRAME_BACK:FRAME_FRONT);
  /* Won cards: original thumbnails stacked per element, earlier ones on top so every colour stays visible. */
  for(unsigned e=0;e<3;e++){
   static const char elements[]="fws";unsigned n=0;
   for(unsigned i=0;i<m->count&&n<8&&!in_menu(g);i++){
    const CardItem *item=&m->cards[i];if(!item->scored||item->seat!=s||item->element!=elements[e])continue;
    if(item==pick)continue;
    snprintf(name,sizeof name,"score%u_%u_%u",s,e,n++);card_view(g,name,item,FRAME_THUMB);
   }
   while(n<8){snprintf(name,sizeof name,"score%u_%u_%u",s,e,n++);card_view(g,name,NULL,FRAME_THUMB);}
  }
 }
 swl_visible(g->scene,"root.tf_name0",false);swl_visible(g->scene,"root.tf_name1",false);tint_players(g);
 sync_clock(g);
 int anim=g->opening>0?2:1;if(anim!=g->anim){g->anim=anim;swl_target(g->actors,"root",(unsigned)anim,NULL,false);}
}
static void choose(CardGame *g,unsigned slot){
 unsigned id;if(!card_model_choose(&g->model,slot,&id))return;
 char value[16];snprintf(value,sizeof value,"%u",id);
 if(!card_send(g,'z',"zm",2,(const char *[]){"pick",value}))card_model_choice_failed(&g->model);
 sync(g);
}
static void begin(CardGame *g,bool sensei){
 if(!g->model.menuReady||g->deck<5||!g->deckKnown)return;
 if(card_send(g,'z',sensei?"jsen":"jmm",0,NULL)){card_model_begin(&g->model,sensei);g->help=false;sync(g);}
}
static void pump(CardGame *g){
 CardModel *m=&g->model;
 if(m->requestGet){if(card_send(g,'z',"gz",1,(const char *[]){m->mode?"1":"0"}))m->requestGet=false;}
 if(m->requestSeats){if(card_send(g,'z',"uz",0,NULL))m->requestSeats=false;}
 if(m->requestDeal&&g->opening<=0){char value[8];snprintf(value,sizeof value,"%u",m->dealAmount);if(card_send(g,'z',"zm",2,(const char *[]){"deal",value}))m->requestDeal=false;}
}
static void *create(GamePort *port){CardGame *g=calloc(1,sizeof *g);if(g){g->port=port;card_model_init(&g->model);if(game_port_descriptor(port)->fromSensei){g->model.mode=1;g->model.colour[0]=14;g->model.colour[1]=game_port_descriptor(port)->colour;snprintf(g->model.names[0],sizeof g->model.names[0],"Sensei");}}return g;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 CardGame *g=game;g->bank=game_port_bank(g->port,BANK_BOARD);g->scene=game_bank_scene(g->bank);g->actors=game_bank_scene(game_port_bank(g->port,BANK_FIGHTERS));g->dojo=game_bank_scene(game_port_bank(g->port,BANK_DOJO));
 if(!g->scene||!g->actors||!g->dojo)return false;
 g->root=0;g->clock=0;
#if CARD_SHOT==2
 g->help=true;
#endif
 sync(g);return true;
}
static void frame(void *game,float seconds,const GameInput *input){
 CardGame *g=game;CardModel *m=&g->model;if(!game_port_connected(g->port))return;
 if(!g->joined){g->joined=card_send(g,'s',"j#jr",3,(const char *[]){"951","0","0"});}
 if(m->menuReady&&!g->queried){bool level=card_send(g,'s',"ni#gnl",0,NULL),deck=card_send(g,'s',"ni#gcd",0,NULL);g->queried=level&&deck;}
 m->elapsed+=seconds;if(g->opening>0){g->opening-=seconds;if(g->opening<0)g->opening=0;sync(g);}
 unsigned click=game_port_clicked(g->port);
 if((input->down&(GAME_KEY_START|GAME_KEY_B))||click==BUTTON_BACK){close_game(g);return;}
 if(click==BUTTON_HELP||input->down&GAME_KEY_SELECT)g->help=!g->help;
 if(m->phase==CARD_MENU){
  if(click==BUTTON_SENSEI)begin(g,true);
  if(click==BUTTON_MATCH)begin(g,false);
  if(click==BUTTON_STARTER&&!g->starterRequested&&g->deckKnown&&g->deck==0){g->starterRequested=card_send(g,'s',"i#ai",1,(const char *[]){"821"});}
 }else if(m->phase==CARD_SELECT){
  if(input->down&GAME_KEY_LEFT)g->focus=g->focus?g->focus-1:m->hands[m->seat]?m->hands[m->seat]-1:0;
  if(input->down&GAME_KEY_RIGHT)g->focus=m->hands[m->seat]?(g->focus+1)%m->hands[m->seat]:0;
  if(!g->help&&(input->down&GAME_KEY_A||click==BUTTON_PICK))choose(g,g->focus);
  if(m->elapsed>=20&&!m->choicePending){bool picked=false;for(unsigned i=0;i<m->hands[m->seat];i++)if(card_model_playable(m,i)){choose(g,i);picked=true;break;}if(!picked&&card_send(g,'z',"zm",2,(const char *[]){"death","0"})){m->choicePending=true;m->phase=CARD_PICK_WAIT;}}
 }else if(m->phase==CARD_REVEAL&&m->elapsed>=m->revealTime){card_model_next_round(m);sync(g);}
 else if((m->phase==CARD_HANDSHAKE||m->phase==CARD_DEALING)&&m->elapsed>20){m->phase=CARD_ERROR;snprintf(m->message,sizeof m->message,"The game did not respond. Return to the room and try again.");}
 pump(g);if(input->down&(GAME_KEY_LEFT|GAME_KEY_RIGHT))sync(g);
 if(m->phase==CARD_REVEAL&&m->elapsed>=2&&m->elapsed-seconds<2)sync(g);
 sync_clock(g);
}
static void tick(void *game){CardGame *g=game;swl_tick(g->scene);swl_tick(g->actors);swl_tick(g->dojo);}
static int hand_slot(float x,float y){
 if(y<140||y>=240||x<5)return -1;
 int slot=(int)((x-5)/(HAND_STEP*LOWER_SCALE));return slot<5?slot:-1;
}
static void touch(void *game,GameTouch phase,float x,float y){
 CardGame *g=game;if(g->help||g->model.phase!=CARD_SELECT)return;
 int slot=hand_slot(x,y);
 if(phase==GAME_TOUCH_DOWN){g->stylus=slot>=0&&(unsigned)slot<g->model.hands[g->model.seat];if(g->stylus){g->focus=(unsigned)slot;sync(g);}}
 else if(phase==GAME_TOUCH_UP){bool chooseCard=g->stylus&&slot>=0&&(unsigned)slot==g->focus;g->stylus=false;if(chooseCard)choose(g,(unsigned)slot);}
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){
 CardGame *g=game;CardModel *m=&g->model;
#if CARD_SHOT==1
 (void)command;(void)count;(void)args;m->menuReady=true;g->deckKnown=true;g->deck=48;g->percentage=40;return true;
#endif
 if(!strcmp(command,"gcd")){
  unsigned total=0;for(unsigned i=0;i<count;i++){const char *p=args[i];while(*p){char *end;strtoul(p,&end,10);if(end==p||*end!=',')return false;p=end+1;unsigned long n=strtoul(p,&end,10);if(end==p||n>10000||(*end&&*end!='|'))return false;total+=(unsigned)n;p=*end?end+1:end;}}
  g->deck=total;g->deckKnown=true;return true;
 }
 if(!strcmp(command,"gnl")&&count>=2){g->percentage=(unsigned)strtoul(args[1],NULL,10);return true;}
 if(!strcmp(command,"ai")&&g->starterRequested){card_send(g,'s',"ni#gcd",0,NULL);return true;}
 if(!strcmp(command,"e")){if(g->starterRequested)card_send(g,'s',"ni#gcd",0,NULL);snprintf(m->message,sizeof m->message,"The server refused this request. Return and try again.");m->phase=CARD_ERROR;sync(g);return true;}
 if(!card_model_packet(m,command,count,args))return false;
 if(!strcmp(command,"scard"))g->matched=true;
 if(!strcmp(command,"sz"))g->opening=106.0f/24;
 sync(g);pump(g);return true;
}

/* ---- Lower-screen chrome (drawn primitives only; all art comes from the bank). ---- */
static void plate(GamePort *p,float x,float y,float w,float h){
 game_port_rect(p,x,y,w,h,WOOD);game_port_rect(p,x+2,y+2,w-4,h-4,PAPER);game_port_rect(p,x+2,y+h-4,w-4,2,0xd9ccab);
}
/** Word-wrapped text; returns the y below the last line. */
static float wrap(GamePort *p,float x,float y,float width,float size,uint32_t colour,GameAlign align,const char *text,unsigned maxLines){
 float charWidth=size*.52f;unsigned perLine=(unsigned)(width/charWidth);if(perLine<4)perLine=4;
 char line[96];unsigned lines=0;const char *cursor=text;
 while(*cursor&&lines<maxLines){
  while(*cursor==' ')cursor++;
  size_t n=strlen(cursor);const char *newline=strchr(cursor,'\n');size_t limit=newline?(size_t)(newline-cursor):n;
  size_t take=limit;
  if(take>perLine){take=perLine;while(take>0&&cursor[take]!=' ')take--;if(!take)take=perLine;}
  if(take>=sizeof line)take=sizeof line-1;
  memcpy(line,cursor,take);line[take]=0;
  game_port_text(p,align==GAME_ALIGN_CENTER?x+width/2:x,y,size,colour,align,line);
  y+=size+3;lines++;cursor+=take;if(*cursor=='\n')cursor++;
 }
 return y;
}
static void header(CardGame *g,const char *title,const char *middle){
 GamePort *p=g->port;
 game_port_rect(p,0,0,320,24,WOOD);game_port_rect(p,0,22,320,2,WOOD_LIGHT);
 game_port_text(p,8,5,12,0xffffff,GAME_ALIGN_LEFT,title);
 if(middle&&*middle)game_port_text(p,180,6,10,0xf3e3bd,GAME_ALIGN_CENTER,middle);
 game_port_button(p,BUTTON_HELP,222,2,30,20,g->help?"X":"?",true);
 game_port_button(p,BUTTON_BACK,256,2,60,20,"Exit",true);
}
static const char *element_name(char e){return e=='f'?"Fire":e=='w'?"Water":"Snow";}
static const char *colour_name(char c){const char *p=strchr("bgopry",c);static const char *const names[]={"Blue","Green","Orange","Purple","Red","Yellow"};return names[p&&*p?(unsigned)(p-"bgopry"):0];}
static void draw_menu(CardGame *g){
 GamePort *p=g->port;CardModel *m=&g->model;
 header(g,game_port_descriptor(p)->fromSensei?"Sensei":"Card-Jitsu","");
 bool enabled=m->menuReady&&g->deckKnown&&g->deck>=5;
 /* Rows follow the original options scroll: belt, Sensei and instructions icons are drawn by the bank (frame 4). */
 static const char *const labels[]={"Earn your belts","Challenge Sensei","Instructions"};
 static const unsigned ids[]={BUTTON_MATCH,BUTTON_SENSEI,BUTTON_HELP};
 for(unsigned i=0;i<3;i++){
  float y=47+i*42.0f;bool on=i==2||enabled;
  game_port_hotspot(p,ids[i],38,y-4,248,34,on);
  game_port_text(p,80,y+5,13,on?INK:0x9a8b70,GAME_ALIGN_LEFT,labels[i]);
 }
 float y=176;
 if(g->deckKnown&&!g->deck)game_port_button(p,BUTTON_STARTER,70,y,180,26,g->starterRequested?"Getting starter deck...":"Get starter deck",!g->starterRequested);
 else{
  char line[96];
  if(!m->menuReady)snprintf(line,sizeof line,"Connecting to the Dojo...");
  else if(!g->deckKnown)snprintf(line,sizeof line,"Checking your deck...");
  else snprintf(line,sizeof line,"%u cards  |  Belt progress %u%%",g->deck,g->percentage);
  game_port_text(p,160,y+6,11,INK,GAME_ALIGN_CENTER,line);
 }
}
static void draw_help(CardGame *g){
 GamePort *p=g->port;
 header(g,"How to play","");
 static const char *const lines[]={"Fire beats Snow, Snow beats Water, Water beats Fire.","Same element: the higher number wins.",
  "Win with 3 cards of one element in different","colours, or one of each element in different colours."};
 plate(p,6,186,308,52);
 for(unsigned i=0;i<4;i++)game_port_text(p,12,190+i*11.5f,9.5f,INK,GAME_ALIGN_LEFT,lines[i]);
}
static void draw_bottom(void *game){
 CardGame *g=game;CardModel *m=&g->model;GamePort *p=g->port;
 if(g->help){draw_help(g);return;}
 if(m->phase==CARD_MENU){draw_menu(g);return;}
 char middle[48]="";if(m->round)snprintf(middle,sizeof middle,"Round %u",m->round);
 header(g,"Card-Jitsu",middle);
 int me=m->seat<0?0:m->seat;
 if(hand_live(m)&&m->hands[me]){
  const CardItem *card=g->focus<m->hands[me]?card_model_find(m,m->hand[me][g->focus]):NULL;
  /* Left scroll: the inspected card (original name, element, colour and power text). */
  plate(p,5,30,122,88);
  if(card){
   const CardInfo *about=info(card->type);char line[64];
   wrap(p,11,35,110,10,INK,GAME_ALIGN_LEFT,about&&about->name?about->name:"",2);
   snprintf(line,sizeof line,"%s %d  |  %s",element_name(card->element),shown_value(g,card),colour_name(card->colour));
   game_port_text(p,11,62,9.5f,0x6b4a2b,GAME_ALIGN_LEFT,line);
   if(card->element==m->blocked[me])wrap(p,11,77,110,9.5f,0xa01922,GAME_ALIGN_LEFT,"Blocked this round.",3);
   else wrap(p,11,77,110,9.5f,INK,GAME_ALIGN_LEFT,card->power&&card->power<sizeof POWERS/sizeof *POWERS?POWERS[card->power]:"No power.",3);
  }
  /* Right scroll: what to do now. */
  plate(p,193,30,122,88);
  const char *status=m->choicePending||m->phase==CARD_PICK_WAIT?"Card played. Waiting for your opponent...":"Choose your card!";
  wrap(p,199,36,110,11,INK,GAME_ALIGN_LEFT,status,3);
  if(m->swap)wrap(p,199,76,110,9.5f,0xa01922,GAME_ALIGN_LEFT,"Lower values win ties this round.",2);
  else if(m->phase==CARD_SELECT&&!m->choicePending)wrap(p,199,80,110,9.5f,0x6b4a2b,GAME_ALIGN_LEFT,"Tap a card, or Left / Right and A.",2);
 }else{
  plate(p,24,34,272,m->phase==CARD_OVER?96:64);
  const char *title=m->phase==CARD_QUEUE?"Waiting for another ninja...":m->phase==CARD_REVEAL?"Round result":m->phase==CARD_OVER?"Match result":m->phase==CARD_ERROR?"Card-Jitsu":"Entering the match...";
  game_port_text(p,160,41,13,INK,GAME_ALIGN_CENTER,title);
  float y=wrap(p,34,62,252,10.5f,0x6b4a2b,GAME_ALIGN_CENTER,m->message,2);
  if(m->phase==CARD_OVER&&m->rankAwarded){char line[64];snprintf(line,sizeof line,"New belt rank: %u",m->newRank);game_port_text(p,160,y+2,11,INK,GAME_ALIGN_CENTER,line);}
  if(m->phase==CARD_OVER||m->phase==CARD_ERROR)game_port_button(p,BUTTON_BACK,90,m->phase==CARD_OVER?100:102,140,26,"Return to room",true);
 }
}
static void draw_top(void *game){
 CardGame *g=game;
 if(!in_menu(g)){
  /* Player names on the board, at a readable size (the original fields are 7 px tall on this screen). */
  game_port_text(g->port,28,184,10,INK,GAME_ALIGN_LEFT,g->model.names[0]);
  game_port_text(g->port,372,184,10,INK,GAME_ALIGN_RIGHT,g->model.names[1]);
  return;
 }
 /* The Sensei's speech bubble (original text from the Sensei menu). */
 static const char *const speech[]={"Do you wish to play","And compete with another","Student, grasshopper?"};
 for(unsigned i=0;i<3;i++)game_port_text(g->port,262,9+i*14,11,0x000000,GAME_ALIGN_CENTER,speech[i]);
}
/* Fit the 760x480 dojo by width and crop the ceiling, not the Sensei shelf at the bottom. */
static bool top_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){400.0f/760,0,0,400.0f/760,0,-12.6f};return true;}
static bool lower_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){LOWER_SCALE,0,0,LOWER_SCALE,0,0};return true;}
/* Touch screen: the board bank (hand or help) except on the menu, where the dojo bank shows the options scroll. */
static bool lower_bank_view(void *game,unsigned bank,SWLMatrix *view){
 CardGame *g=game;*view=(SWLMatrix){LOWER_SCALE,0,0,LOWER_SCALE,0,0};
 return bank==BANK_BOARD?g->help||!in_menu(g):bank==BANK_DOJO&&in_menu(g)&&!g->help;
}
static unsigned lower_frame(void *game,unsigned bank){CardGame *g=game;return bank==BANK_BOARD?(g->help?ROOT_HELP:ROOT_HAND):bank==BANK_DOJO?DOJO_OPTIONS:0;}
static bool bank_view(void *game,unsigned bank,SWLMatrix *view){CardGame *g=game;*view=(SWLMatrix){1,0,0,1,0,0};return bank==BANK_DOJO||!in_menu(g);}
const GameController GAME_CARD_CONTROLLER={.key="card",.kind=GAME_CARD,.room=998,.rate=24,.banks={GP_CARD_BANK,GP_CARD_ACTORS_BANK,GP_CARD_PREVIEW_PREFIX "dojo.gpk"},.order={BANK_DOJO,BANK_FIGHTERS,BANK_BOARD},.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.top_view=top_view,.bank_view=bank_view,.lower_view=lower_view,.lower_bank_view=lower_bank_view,.lower_frame=lower_frame,.draw_top=draw_top};
#endif
