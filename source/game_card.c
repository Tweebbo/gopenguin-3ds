#include "game_features.h"
#ifdef GP_NINJA_GAMES
#include "game_controller.h"
#include "game_card_model.h"
#include "game_card.h"
#include "game_card_art.h"
#include "game_card_preview.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {BUTTON_SENSEI=1,BUTTON_MATCH,BUTTON_BACK,BUTTON_HELP,BUTTON_PICK,BUTTON_STARTER};
static const uint32_t COLOURS[]={0x003366,0x003366,0x009900,0xff3399,0x333333,0xcc0000,0xff6600,0xffcc00,0x660099,0x996600,0xff6666,0x006600,0x0099cc,0x8ae302,0x93a0a4,0x02a797};
static const uint32_t CARD_COLOURS[]={1132705,6404422,16225579,10721738,14826534,16509741};
typedef struct {
 GamePort *port;GameBank *bank;SWLScene *scene,*actors;CardModel model;
 unsigned focus,deck,percentage;bool joined,queried,deckKnown,starterRequested,left,help,stylus,matched;
 float opening;int anim;CardPreview preview;
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
static SWLColor rgb(uint32_t value){return (SWLColor){{0,0,0,1},{(float)(value>>16&255),(float)(value>>8&255),(float)(value&255),0}};}
static void card_view(CardGame *g,const char *name,const CardItem *item,bool focused){
 char base[64],path[96];snprintf(base,sizeof base,"root.%s",name);unsigned frame=item?icon(item->type):0;
 swl_visible(g->scene,base,item&&frame);if(!item||!frame)return;
 swl_target(g->scene,base,1,NULL,false);
 snprintf(path,sizeof path,"%s.mc_art",base);swl_target(g->scene,path,frame,NULL,false);
 snprintf(path,sizeof path,"%s.mc_atr",base);swl_target(g->scene,path,item->element=='f'?1:item->element=='w'?2:3,NULL,false);
 const char *c=strchr("bgopry",item->colour);snprintf(path,sizeof path,"%s.mc_col",base);swl_color(g->scene,path,rgb(CARD_COLOURS[c?(unsigned)(c-"bgopry"):0]));
 snprintf(path,sizeof path,"%s.card_glow",base);swl_visible(g->scene,path,focused||item->power>0);
 int points=(int)item->value+((item->held||g->model.picked[item->seat]==item->session)?g->model.modifier[item->seat]:0);
 char value[12];snprintf(value,sizeof value,"%d",points);snprintf(path,sizeof path,"%s.mc_pt.tf_pt",base);game_bank_text(g->bank,path,value);
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
static void sync(CardGame *g){
 CardModel *m=&g->model;int seat=m->seat<0?0:m->seat;
 if(g->focus>=m->hands[seat])g->focus=0;
 for(unsigned i=0;i<CARD_HAND;i++){
  char name[16];snprintf(name,sizeof name,"hand%u",i);const CardItem *item=i<m->hands[seat]?card_model_find(m,m->hand[seat][i]):NULL;card_view(g,name,item,i==g->focus&&m->phase==CARD_SELECT);
  if(item&&item->element==m->blocked[seat]){char path[40];snprintf(path,sizeof path,"root.hand%u",i);swl_target(g->scene,path,2,NULL,false);}
 }
 for(unsigned s=0;s<2;s++){
  char name[24];snprintf(name,sizeof name,"pick%u",s);card_view(g,name,card_model_find(m,m->picked[s]),false);
  if((int)s!=m->seat&&m->picked[s]&&m->seat>=0&&!m->picked[m->seat]){char path[40];snprintf(path,sizeof path,"root.pick%u",s);swl_target(g->scene,path,5,NULL,false);}
  unsigned visible=0;bool seen[3][6]={{false}};
  for(unsigned i=0;i<m->count&&visible<9;i++){
   const CardItem *item=&m->cards[i];if(!item->scored||item->seat!=s)continue;
   unsigned e=item->element=='f'?0:item->element=='w'?1:2;const char *p=strchr("bgopry",item->colour);unsigned c=p?(unsigned)(p-"bgopry"):0;if(seen[e][c])continue;seen[e][c]=true;
   snprintf(name,sizeof name,"score%u_%u",s,visible++);card_view(g,name,item,false);
  }
  while(visible<9){snprintf(name,sizeof name,"score%u_%u",s,visible++);card_view(g,name,NULL,false);}
 }
 game_bank_text(g->bank,"root.tf_name0",m->names[0]);game_bank_text(g->bank,"root.tf_name1",m->names[1]);game_bank_text(g->bank,"root.tf_msg","");tint_players(g);
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
 if(card_send(g,'z',sensei?"jsen":"jmm",0,NULL)){card_model_begin(&g->model,sensei);g->help=false;}
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
 CardGame *g=game;g->bank=game_port_bank(g->port,0);g->scene=game_bank_scene(g->bank);g->actors=game_bank_scene(game_port_bank(g->port,1));if(!g->scene||!g->actors)return false;sync(g);return true;
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
 const CardItem *selected=m->seat>=0&&g->focus<m->hands[m->seat]?card_model_find(m,m->hand[m->seat][g->focus]):NULL;
 card_preview_update(g->port,&g->preview,(m->phase==CARD_SELECT||m->phase==CARD_PICK_WAIT)&&selected?selected->type:0);
 if(selected)card_preview_stats(g->port,&g->preview,(int)selected->value+m->modifier[m->seat],selected->colour,selected->element==m->blocked[m->seat]);
}
static void tick(void *game){CardGame *g=game;swl_tick(g->scene);swl_tick(g->actors);}
static void touch(void *game,GameTouch phase,float x,float y){
 CardGame *g=game;if(g->help||g->model.phase!=CARD_SELECT)return;
 int slot=x>=5&&x<320&&y>=55&&y<139?(int)((x-5)/63):-1;
 if(phase==GAME_TOUCH_DOWN){g->stylus=slot>=0&&slot<5;if(g->stylus){g->focus=(unsigned)slot;sync(g);}}
 else if(phase==GAME_TOUCH_UP){bool chooseCard=g->stylus&&slot>=0&&(unsigned)slot==g->focus;g->stylus=false;if(chooseCard)choose(g,(unsigned)slot);}
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){
 CardGame *g=game;CardModel *m=&g->model;
 if(!strcmp(command,"gcd")){
  unsigned total=0;for(unsigned i=0;i<count;i++){const char *p=args[i];while(*p){char *end;strtoul(p,&end,10);if(end==p||*end!=',')return false;p=end+1;unsigned long n=strtoul(p,&end,10);if(end==p||n>10000||(*end&&*end!='|'))return false;total+=(unsigned)n;p=*end?end+1:end;}}
  g->deck=total;g->deckKnown=true;return true;
 }
 if(!strcmp(command,"gnl")&&count>=2){g->percentage=(unsigned)strtoul(args[1],NULL,10);return true;}
 if(!strcmp(command,"ai")&&g->starterRequested){card_send(g,'s',"ni#gcd",0,NULL);return true;}
 if(!strcmp(command,"e")){if(g->starterRequested)card_send(g,'s',"ni#gcd",0,NULL);snprintf(m->message,sizeof m->message,"The server refused this request. Return and try again.");m->phase=CARD_ERROR;return true;}
 if(!card_model_packet(m,command,count,args))return false;
 if(!strcmp(command,"scard"))g->matched=true;
 if(!strcmp(command,"sz"))g->opening=106.0f/24;
 sync(g);pump(g);return true;
}
static void draw_bottom(void *game){
 CardGame *g=game;CardModel *m=&g->model;game_port_rect(g->port,0,0,320,48,0x003366);game_port_text(g->port,10,7,13,0xffffff,GAME_ALIGN_LEFT,game_port_descriptor(g->port)->fromSensei?"Sensei  |  Card-Jitsu":"Card-Jitsu");
 if(g->help){
  game_port_rect(g->port,0,28,320,179,0xdaf2ff);
  static const char *const lines[]={"Fire beats snow. Snow beats water.","Water beats fire. Same element:","the higher value wins.","Win three different colours of one", "element, or all three elements in", "different colours. Power cards can", "change the next round or remove wins.","Tap a card, or use Left / Right + A.","The 20-second timer keeps running."};
  for(unsigned i=0;i<9;i++)game_port_text(g->port,8,35+i*16,10,0x003366,GAME_ALIGN_LEFT,lines[i]);
  game_port_button(g->port,BUTTON_HELP,8,208,148,27,"Back to match",true);game_port_button(g->port,BUTTON_BACK,164,208,148,27,"Leave",true);return;
 }
 if(m->phase==CARD_MENU){
  game_port_rect(g->port,0,28,320,212,0xdaf2ff);
  game_port_text(g->port,10,39,11,0x003366,GAME_ALIGN_LEFT,m->menuReady?"Play in the Dojo with the same penguin.":"Connecting to the local Dojo...");
  bool enabled=m->menuReady&&g->deckKnown&&g->deck>=5;
  game_port_button(g->port,BUTTON_SENSEI,8,67,304,31,"Challenge Sensei",enabled);
  game_port_button(g->port,BUTTON_MATCH,8,105,304,31,"Find an opponent",enabled);
  if(g->deckKnown&&!g->deck)game_port_button(g->port,BUTTON_STARTER,8,143,304,31,g->starterRequested?"Getting starter deck...":"Get starter deck",!g->starterRequested);
  else{char line[96];snprintf(line,sizeof line,"%u cards  |  Belt progress: %u%%",g->deck,g->percentage);game_port_text(g->port,10,149,11,0x003366,GAME_ALIGN_LEFT,line);}
  game_port_button(g->port,BUTTON_HELP,8,202,148,30,"How to play",true);game_port_button(g->port,BUTTON_BACK,164,202,148,30,"Back",true);return;
 }
 game_port_rect(g->port,0,139,320,101,0xdaf2ff);
 if(m->phase==CARD_SELECT||m->phase==CARD_PICK_WAIT){
  char line[96];unsigned seconds=m->elapsed<20?(unsigned)(20-m->elapsed):0;snprintf(line,sizeof line,"Round %u  |  %u seconds",m->round,seconds);game_port_text(g->port,8,31,10,0xffffff,GAME_ALIGN_LEFT,line);
  for(unsigned i=0;m->seat>=0&&i<m->hands[m->seat];i++){const CardItem *card=card_model_find(m,m->hand[m->seat][i]);if(card)card_preview_hand(g->port,i,(int)card->value+m->modifier[m->seat],card->element,card->colour,i==g->focus,card->element==m->blocked[m->seat]);}
  game_port_text(g->port,8,146,10,0x003366,GAME_ALIGN_LEFT,m->choicePending||m->phase==CARD_PICK_WAIT?"Waiting for the other player...":"Left/Right inspects. A or touch plays.");
  if(m->swap)game_port_text(g->port,8,158,9,0x003366,GAME_ALIGN_LEFT,"Power: lower value wins with matching elements.");
  game_port_button(g->port,BUTTON_PICK,8,170,148,29,"Play card",card_model_playable(m,g->focus));
 }else{
  game_port_text(g->port,10,35,10,0xffffff,GAME_ALIGN_LEFT,m->phase==CARD_QUEUE?"Waiting for another ninja...":m->phase==CARD_REVEAL?"Round result":m->phase==CARD_OVER?"Match result":"Entering the match...");
  game_port_text(g->port,8,145,10,0x003366,GAME_ALIGN_LEFT,m->message);
 }
 if(m->phase==CARD_OVER&&m->rankAwarded){char line[96];snprintf(line,sizeof line,"New belt rank: %u",m->newRank);game_port_text(g->port,8,169,10,0x003366,GAME_ALIGN_LEFT,line);}
 game_port_button(g->port,BUTTON_HELP,164,170,148,29,"How to play",true);game_port_button(g->port,BUTTON_BACK,8,207,304,29,m->phase==CARD_OVER?"Return to room":"Leave match",true);
}
static bool top_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){400.0f/760,0,0,400.0f/760,0,-6};return true;}
static bool lower_view(void *game,SWLMatrix *view){CardGame *g=game;if(g->model.phase==CARD_MENU||g->help)return false;*view=(SWLMatrix){320.0f/760,0,0,320.0f/760,0,0};return true;}
static unsigned lower_frame(void *game,unsigned bank){(void)game;return bank?0:2;}
static const char *bank_resource(void *game,unsigned bank){CardGame *g=game;return bank==2?g->preview.resource:bank==1?GP_CARD_ACTORS_BANK:GP_CARD_BANK;}
static bool bank_view(void *game,unsigned bank,SWLMatrix *view){CardGame *g=game;*view=(SWLMatrix){1,0,0,1,0,0};return bank==2?g->preview.id!=0:bank!=1||!g->preview.id;}
const GameController GAME_CARD_CONTROLLER={.key="card",.kind=GAME_CARD,.room=998,.rate=24,.banks={GP_CARD_BANK,GP_CARD_ACTORS_BANK,GP_CARD_PREVIEW_PREFIX "1.gpk"},.lazy=1u<<2,.bank_resource=bank_resource,.order={0,1,2},.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.top_view=top_view,.bank_view=bank_view,.lower_view=lower_view,.lower_frame=lower_frame};
#endif
