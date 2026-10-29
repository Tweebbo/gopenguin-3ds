#include "game_features.h"
#ifdef GP_NINJA_GAMES
#include "game_controller.h"
#include "game_fire.h"
#include "game_card_catalog.h"
#include "game_card_art.h"
#include "game_card_preview.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
enum {F_SENSEI=32,F_MATCH,F_BACK,F_HELP,F_SPIN,F_CW,F_CCW,F_FIRE,F_WATER,F_SNOW,F_OPP0,F_OPP1,F_OPP2,F_OPP3,F_PICK,F_STARTER};
static const char BOARD_TYPES[16]={'b','s','w','f','c','s','f','w','b','s','w','f','c','w','s','f'};
static const uint32_t PENGUIN_COLOURS[]={0x003366,0x003366,0x009900,0xff3399,0x333333,0xcc0000,0xff6600,0xffcc00,0x660099,0x996600,0xff6666,0x006600,0x0099cc,0x8ae302,0x93a0a4,0x02a797};
static const uint32_t CARD_COLOURS[]={1132705,6404422,16225579,10721738,14826534,16509741};
typedef struct {GamePort *port;GameBank *bank;SWLScene *scene;FireModel model;CardPreview preview;unsigned focus,deck;bool joined,queried,deckKnown,starterRequested,left,help,stylus,matched;} FireGame;
const FireModel *game_fire_model(const void *game){return &((const FireGame *)game)->model;}
static bool fire_send(FireGame *g,const char *command,unsigned count,const char *const *args){return game_port_send(g->port,'z',command,count,args);}
static const char *element_name(char e){return e=='f'?"Fire":e=='w'?"Water":e=='s'?"Snow":e=='c'?"Choose element":"Card-Jitsu battle";}
static SWLColor rgb(uint32_t value){return (SWLColor){{0,0,0,1},{(float)(value>>16&255),(float)(value>>8&255),(float)(value&255),0}};}
static unsigned icon(unsigned type){for(unsigned i=0;i<CARD_ICON_COUNT;i++)if(CARD_ICON_IDS[i]==type)return i+1;return 0;}
static void card_view(FireGame *g,const char *name,unsigned type,bool focused,bool enabled){
 char base[64],path[96];snprintf(base,sizeof base,"root.%s",name);const CardInfo *card=card_info(type);unsigned art=icon(type);swl_visible(g->scene,base,card&&art);if(!card||!art)return;
 swl_target(g->scene,base,enabled?1:3,NULL,false);snprintf(path,sizeof path,"%s.mc_art",base);swl_target(g->scene,path,art,NULL,false);snprintf(path,sizeof path,"%s.mc_atr",base);swl_target(g->scene,path,card->element=='f'?1:card->element=='w'?2:3,NULL,false);
 const char *c=strchr("bgopry",card->colour);snprintf(path,sizeof path,"%s.mc_col",base);swl_color(g->scene,path,rgb(CARD_COLOURS[c?(unsigned)(c-"bgopry"):0]));snprintf(path,sizeof path,"%s.mc_glow",base);swl_visible(g->scene,path,focused||card->power>0);
 char value[12];snprintf(value,sizeof value,"%u",card->value);snprintf(path,sizeof path,"%s.mc_pt.tf_pt",base);game_bank_text(g->bank,path,value);
}
static void sync(FireGame *g){
 FireModel *m=&g->model;bool reveal=m->phase==FIRE_REVEAL||m->phase==FIRE_OVER;swl_visible(g->scene,"root.fire_mc.board_mc",!reveal);
 for(unsigned i=0;i<5;i++){char path[24];snprintf(path,sizeof path,"hand%u",i);card_view(g,path,m->hand[i],i==g->focus,fire_model_playable(m,i));}
 for(unsigned i=0;i<4;i++){
  char path[80];snprintf(path,sizeof path,"root.piece%u",i);swl_visible(g->scene,path,i<m->players&&m->energy[i]>0&&!reveal);
  if(i<m->players){unsigned tile=m->position[i];swl_transform(g->scene,path,(SWLMatrix){.75f,0,0,.75f,FIRE_TILE_POSITIONS[tile][0],FIRE_TILE_POSITIONS[tile][1]-6});
   static const char *const body[]={"body_mc","frontArm_mc","backArm_mc"};for(unsigned b=0;b<3;b++){snprintf(path,sizeof path,"root.piece%u.art.%s",i,body[b]);swl_color(g->scene,path,rgb(PENGUIN_COLOURS[m->colour[i]<=15?m->colour[i]:1]));}
   snprintf(path,sizeof path,"root.piece%u.highlight",i);swl_visible(g->scene,path,i==m->active);
  }
  snprintf(path,sizeof path,"battle%u",i);card_view(g,path,reveal?m->played[i]:0,false,true);snprintf(path,sizeof path,"root.battle%u",i);swl_transform(g->scene,path,(SWLMatrix){.55f,0,0,.55f,m->players==2?225+200*i:95+150*i,175});
 }
}
static void close_game(void *game){FireGame *g=game;if(g->left)return;g->left=true;if(g->model.phase==FIRE_QUEUE)fire_send(g,"lmm",0,NULL);else if(g->matched)fire_send(g,"lz",0,NULL);game_port_leave(g->port);}
static void begin(FireGame *g,bool sensei){if(!g->model.menuReady||!g->deckKnown||g->deck<6)return;if(fire_send(g,sensei?"jsen":"jmm",0,NULL))fire_model_begin(&g->model,sensei);}
static void choose(FireGame *g,unsigned slot){if(!fire_model_playable(&g->model,slot))return;char value[8];snprintf(value,sizeof value,"%u",slot);if(fire_send(g,"zm",2,(const char *[]){"cc",value})){g->model.inputPending=true;g->model.phase=FIRE_WAIT;}sync(g);}
static void destination(FireGame *g,unsigned tile){if(!fire_model_destination(&g->model,tile))return;char value[8];snprintf(value,sizeof value,"%u",tile);if(fire_send(g,"zm",2,(const char *[]){"cb",value}))g->model.inputPending=true;}
static void pump(FireGame *g){
 FireModel *m=&g->model;
 if(m->requestGet&&fire_send(g,"gz",1,(const char *[]){m->mode?"1":"0"}))m->requestGet=false;
 if(m->requestReady&&!m->readySent&&m->elapsed>=4&&fire_send(g,"zm",1,(const char *[]){"ir"})){m->readySent=true;m->requestReady=false;}
}
static void *create(GamePort *port){FireGame *g=calloc(1,sizeof *g);if(g){g->port=port;fire_model_init(&g->model);if(game_port_descriptor(port)->fromSensei)g->model.mode=1;}return g;}
static void destroy(void *game){free(game);}
static bool ready(void *game){FireGame *g=game;g->bank=game_port_bank(g->port,0);g->scene=game_bank_scene(g->bank);if(!g->scene)return false;sync(g);return true;}
static void sensei_art(FireGame *g){
 if(g->model.phase!=FIRE_MENU||!game_port_descriptor(g->port)->fromSensei){game_port_release_bank(g->port,1);return;}
 if(game_port_require_bank(g->port,1)!=GAME_RESOURCE_READY)return;
 SWLScene *s=game_bank_scene(game_port_bank(g->port,1));swl_visible(s,"root.fighter1",false);
 for(unsigned i=0;i<3;i++){char path[80];const char *parts[]={"body_mc","frontArm_mc","backArm_mc"};snprintf(path,sizeof path,"root.fighter0.%s",parts[i]);swl_color(s,path,rgb(PENGUIN_COLOURS[14]));}
 const char *hat[]={"sensay_mc","sensay2_mc","sensay_mc2"};for(unsigned i=0;i<3;i++){char path[80];snprintf(path,sizeof path,"root.fighter0.%s",hat[i]);swl_visible(s,path,true);}
}
static void frame(void *game,float seconds,const GameInput *input){
 FireGame *g=game;FireModel *m=&g->model;if(!game_port_connected(g->port))return;
 if(!g->joined)g->joined=game_port_send(g->port,'s',"j#jr",3,(const char *[]){"953","0","0"});
 if(m->menuReady&&!g->queried){bool level=game_port_send(g->port,'s',"ni#gfl",0,NULL),deck=game_port_send(g->port,'s',"ni#gcd",0,NULL);g->queried=level&&deck;}
 m->elapsed+=seconds;unsigned click=game_port_clicked(g->port);
 if(click==F_BACK||input->down&(GAME_KEY_START|GAME_KEY_B)){close_game(g);return;}
 if(click==F_HELP||input->down&GAME_KEY_SELECT)g->help=!g->help;
 if(m->phase==FIRE_MENU){
  if(click==F_SENSEI)begin(g,true);
  if(click==F_MATCH)begin(g,false);
  if(click==F_STARTER&&!g->starterRequested&&g->deckKnown&&!g->deck)g->starterRequested=game_port_send(g->port,'s',"i#ai",1,(const char *[]){"821"});
 }else if(!g->help){
  if(m->phase==FIRE_BOARD&&(click==F_SPIN||input->down&GAME_KEY_A)&&!m->inputPending&&fire_send(g,"zm",3,(const char *[]){"is","","1"})){m->inputPending=true;m->phase=FIRE_SPIN_WAIT;}
  if(m->phase==FIRE_MOVE){if(click==F_CW||input->down&GAME_KEY_LEFT)destination(g,m->spin[1]);if(click==F_CCW||input->down&GAME_KEY_RIGHT)destination(g,m->spin[2]);}
  if(m->phase==FIRE_TRUMP&&!m->inputPending){const char *trump=click==F_FIRE?"f":click==F_WATER?"w":click==F_SNOW?"s":NULL;if(trump&&fire_send(g,"zm",2,(const char *[]){"ct",trump}))m->inputPending=true;}
  if(m->phase==FIRE_OPPONENT&&click>=F_OPP0&&click<=F_OPP3&&!m->inputPending&&m->opponents[click-F_OPP0]){char seat[8];snprintf(seat,sizeof seat,"%u",click-F_OPP0);if(fire_send(g,"zm",2,(const char *[]){"co",seat}))m->inputPending=true;}
  if(m->phase==FIRE_SELECT){if(input->down&GAME_KEY_LEFT)g->focus=g->focus?g->focus-1:4;if(input->down&GAME_KEY_RIGHT)g->focus=(g->focus+1)%5;if(input->down&GAME_KEY_A||click==F_PICK)choose(g,g->focus);}
 }
 if(m->phase==FIRE_HANDSHAKE&&m->elapsed>20){m->phase=FIRE_ERROR;snprintf(m->message,sizeof m->message,"The Fire game did not respond. Return and try again.");}
 pump(g);if(input->down&(GAME_KEY_LEFT|GAME_KEY_RIGHT))sync(g);sensei_art(g);
 bool hand=m->phase==FIRE_SELECT||(m->phase==FIRE_WAIT&&m->seat>=0&&m->participants[m->seat]);
 card_preview_update(g->port,&g->preview,hand&&g->focus<5?m->hand[g->focus]:0);
 const CardInfo *selected=g->preview.id?card_info(g->preview.id):NULL;if(selected)card_preview_stats(g->port,&g->preview,(int)selected->value,selected->colour,!fire_model_playable(m,g->focus)&&m->phase==FIRE_SELECT);
}
static void tick(void *game){FireGame *g=game;swl_tick(g->scene);SWLScene *s=game_bank_scene(game_port_bank(g->port,1));if(s)swl_tick(s);}
static void touch(void *game,GameTouch phase,float x,float y){FireGame *g=game;if(g->help||g->model.phase!=FIRE_SELECT)return;int slot=x>=5&&x<320&&y>=55&&y<139?(int)((x-5)/63):-1;if(phase==GAME_TOUCH_DOWN){g->stylus=slot>=0&&slot<5;if(g->stylus){g->focus=(unsigned)slot;sync(g);}}else if(phase==GAME_TOUCH_UP){bool picked=g->stylus&&slot>=0&&(unsigned)slot==g->focus;g->stylus=false;if(picked)choose(g,(unsigned)slot);}}
static bool packet(void *game,const char *command,unsigned count,char *const *args){
 FireGame *g=game;
 if(!strcmp(command,"gcd")){unsigned total=0;for(unsigned i=0;i<count;i++){const char *p=args[i];while(*p){char *end;strtoul(p,&end,10);if(end==p||*end!=',')return false;p=end+1;unsigned long n=strtoul(p,&end,10);if(end==p||n>10000||(*end&&*end!='|'))return false;total+=(unsigned)n;p=*end?end+1:end;}}g->deck=total;g->deckKnown=true;return true;}
 if(!strcmp(command,"gfl")&&count>=2){g->model.rank=(unsigned)strtoul(args[0],NULL,10);g->model.percentage=(unsigned)strtoul(args[1],NULL,10);return true;}
 if(!strcmp(command,"ai")&&g->starterRequested){game_port_send(g->port,'s',"ni#gcd",0,NULL);return true;}
 if(!strcmp(command,"e")){g->model.phase=FIRE_ERROR;snprintf(g->model.message,sizeof g->model.message,"The server refused this Fire request.");return true;}
 if(!fire_model_packet(&g->model,command,count,args))return false;
 if(!strcmp(command,"scard"))g->matched=true;
 sync(g);pump(g);return true;
}
static void draw_bottom(void *game){
 FireGame *g=game;FireModel *m=&g->model;char line[128];game_port_rect(g->port,0,0,320,48,0x003366);game_port_text(g->port,8,7,12,0xffffff,GAME_ALIGN_LEFT,"Card-Jitsu Fire");
 if(g->help){game_port_rect(g->port,0,28,320,212,0xdaf2ff);static const char *const help[]={"Keep your energy. The last ninja wins.","On your turn spin, then choose one", "of the two available board spaces.","For an element battle, play that", "element when you have one. The", "highest value wins. For Card-Jitsu", "battles use fire, snow and water.","Help never pauses the match."};for(unsigned i=0;i<8;i++)game_port_text(g->port,8,36+i*17,10,0x003366,GAME_ALIGN_LEFT,help[i]);game_port_button(g->port,F_HELP,8,205,148,30,"Back to match",true);game_port_button(g->port,F_BACK,164,205,148,30,"Leave",true);return;}
 if(m->phase==FIRE_MENU){game_port_rect(g->port,0,28,320,212,0xdaf2ff);game_port_text(g->port,8,37,10,0x003366,GAME_ALIGN_LEFT,m->menuReady?"Fire battles support two to four players.":"Connecting to the local Fire Dojo...");bool enabled=m->menuReady&&g->deckKnown&&g->deck>=6;game_port_button(g->port,F_SENSEI,8,67,304,31,"Challenge Fire Sensei",enabled);game_port_button(g->port,F_MATCH,8,105,304,31,"Find Fire opponents",enabled);if(g->deckKnown&&!g->deck)game_port_button(g->port,F_STARTER,8,143,304,31,"Get starter deck",!g->starterRequested);else{if(g->deckKnown&&g->deck<6)snprintf(line,sizeof line,"%u cards; at least 6 are needed for Fire.",g->deck);else snprintf(line,sizeof line,"%u cards  |  Fire rank %u (%u%%)",g->deck,m->rank,m->percentage);game_port_text(g->port,8,149,10,0x003366,GAME_ALIGN_LEFT,line);}game_port_button(g->port,F_HELP,8,203,148,30,"How to play",true);game_port_button(g->port,F_BACK,164,203,148,30,"Back",true);return;}
 bool hand=m->phase==FIRE_SELECT||(m->phase==FIRE_WAIT&&m->seat>=0&&m->participants[m->seat]);game_port_rect(g->port,0,hand?139:28,320,hand?101:212,0xdaf2ff);
 if(hand){
  snprintf(line,sizeof line,"%s  |  Your energy: %u",element_name(m->trump),m->energy[m->seat]);game_port_text(g->port,8,31,10,0xffffff,GAME_ALIGN_LEFT,line);
  for(unsigned i=0;i<5;i++){const CardInfo *card=card_info(m->hand[i]);if(card)card_preview_hand(g->port,i,(int)card->value,card->element,card->colour,i==g->focus,!fire_model_playable(m,i)&&m->phase==FIRE_SELECT);}
  game_port_text(g->port,8,146,10,0x003366,GAME_ALIGN_LEFT,m->inputPending?"Waiting for the other battle cards...":"Left/Right inspects. A or touch plays.");game_port_button(g->port,F_PICK,8,170,148,29,"Play card",fire_model_playable(m,g->focus));
 }else if(m->phase==FIRE_BOARD){game_port_text(g->port,8,37,11,0x003366,GAME_ALIGN_LEFT,"Your turn: spin the stone.");game_port_button(g->port,F_SPIN,8,65,304,36,"Spin",!m->inputPending);}
 else if(m->phase==FIRE_MOVE){snprintf(line,sizeof line,"Spin: %u  |  Choose your destination",m->spin[0]);game_port_text(g->port,8,37,11,0x003366,GAME_ALIGN_LEFT,line);snprintf(line,sizeof line,"Clockwise: %s",element_name(BOARD_TYPES[m->spin[1]]));game_port_button(g->port,F_CW,8,65,304,34,line,!m->inputPending);snprintf(line,sizeof line,"Counterclockwise: %s",element_name(BOARD_TYPES[m->spin[2]]));game_port_button(g->port,F_CCW,8,105,304,34,line,!m->inputPending);}
 else if(m->phase==FIRE_TRUMP){game_port_text(g->port,8,37,11,0x003366,GAME_ALIGN_LEFT,"Choose the battle element.");game_port_button(g->port,F_FIRE,8,65,96,36,"Fire",!m->inputPending);game_port_button(g->port,F_WATER,112,65,96,36,"Water",!m->inputPending);game_port_button(g->port,F_SNOW,216,65,96,36,"Snow",!m->inputPending);}
 else if(m->phase==FIRE_OPPONENT){game_port_text(g->port,8,37,11,0x003366,GAME_ALIGN_LEFT,"Choose a Card-Jitsu opponent.");unsigned row=0;for(unsigned i=0;i<m->players;i++)if(m->opponents[i])game_port_button(g->port,F_OPP0+i,8,62+row++*35,304,30,m->names[i],!m->inputPending);}
 else{game_port_text(g->port,8,37,10,0x003366,GAME_ALIGN_LEFT,m->message);for(unsigned i=0;i<m->players;i++){snprintf(line,sizeof line,"%s: %u energy%s",m->names[i],m->energy[i],i==m->active?"  *":"");game_port_text(g->port,8,65+i*18,10,0x003366,GAME_ALIGN_LEFT,line);}if(m->phase==FIRE_OVER&&m->seat>=0){snprintf(line,sizeof line,"Place: %u  |  Fire rank: %u",m->standing[m->seat],m->rank);game_port_text(g->port,8,146,11,0x003366,GAME_ALIGN_LEFT,line);}}
 if(m->phase==FIRE_BOARD||m->phase==FIRE_MOVE||m->phase==FIRE_TRUMP){if(m->players==2)snprintf(line,sizeof line,"Energy by seat: %u / %u",m->energy[0],m->energy[1]);else snprintf(line,sizeof line,"Energy by seat: %u / %u / %u / %u",m->energy[0],m->energy[1],m->energy[2],m->energy[3]);game_port_text(g->port,8,145,9,0x003366,GAME_ALIGN_LEFT,line);snprintf(line,sizeof line,"Your seat: %u  |  Round %u",(unsigned)m->seat+1,m->round);game_port_text(g->port,8,157,9,0x003366,GAME_ALIGN_LEFT,line);}
 game_port_button(g->port,F_HELP,164,170,148,29,"How to play",true);game_port_button(g->port,F_BACK,8,207,304,29,m->phase==FIRE_OVER?"Return to room":"Leave match",true);
}
static bool top_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){400.0f/760,0,0,400.0f/760,0,-6};return true;}
static bool lower_view(void *game,SWLMatrix *view){FireGame *g=game;if(g->help||g->model.seat<0||!(g->model.phase==FIRE_SELECT||(g->model.phase==FIRE_WAIT&&g->model.participants[g->model.seat])))return false;*view=(SWLMatrix){320.0f/760,0,0,320.0f/760,0,0};return true;}
static unsigned lower_frame(void *game,unsigned bank){(void)game;return bank?0:2;}
static const char *bank_resource(void *game,unsigned bank){FireGame *g=game;return bank==2?g->preview.resource:bank==1?GP_CARD_ACTORS_BANK:GP_FIRE_BANK;}
static bool bank_view(void *game,unsigned bank,SWLMatrix *view){FireGame *g=game;*view=(SWLMatrix){1,0,0,1,0,0};return bank==2?g->preview.id!=0:bank!=1||(g->model.phase==FIRE_MENU&&game_port_descriptor(g->port)->fromSensei);}
const GameController GAME_FIRE_CONTROLLER={.key="fire",.kind=GAME_CARD,.room=997,.rate=24,.banks={GP_FIRE_BANK,GP_CARD_ACTORS_BANK,GP_CARD_PREVIEW_PREFIX "1.gpk"},.lazy=(1u<<1)|(1u<<2),.bank_resource=bank_resource,.order={0,1,2},.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=draw_bottom,.top_view=top_view,.bank_view=bank_view,.lower_view=lower_view,.lower_frame=lower_frame};
#endif
