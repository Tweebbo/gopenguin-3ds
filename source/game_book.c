#include "game_controller.h"
#include "game_book_data.h"
#include "game_book.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOK_BANKS 3
#define BOOK_PAGE_SCORE 480
enum {SCREEN_MENU,SCREEN_READING,SCREEN_RESULT};
enum {BUTTON_START=1,BUTTON_PREV,BUTTON_NEXT,BUTTON_EXIT,BUTTON_PAY,BUTTON_AGAIN};

typedef struct {
 GamePort *port;
 GameBank *bank[BOOK_BANKS];
 SWLScene *scene[BOOK_BANKS];
 const BookData *book;
 unsigned screen,page;
 bool started;
} Book;

const BookData *game_book_data(void *game){return ((Book *)game)->book;}
unsigned game_book_page(void *game){return ((Book *)game)->page;}
static const unsigned BOOK1_START[]={0,2,4},BOOK2_START[]={0,5,8},BOOK3_START[]={0,3,6};

static unsigned page_bank(const Book *b,unsigned page){
 if(b->book->room==918)return page<5?0:page<8?1:2;
 if(b->book->room==919)return page<3?0:page<6?1:2;
 return page<2?0:page<4?1:2;
}
static unsigned bank_start(const Book *b,unsigned bank){
 if(b->book->room==918)return BOOK2_START[bank];
 if(b->book->room==919)return BOOK3_START[bank];
 return BOOK1_START[bank];
}
static void hide_pages(Book *b){
 char path[24];
 for(unsigned bank=0;bank<BOOK_BANKS;bank++)if(b->scene[bank])for(unsigned slot=0;slot<5;slot++){snprintf(path,sizeof path,"root.page%u",slot);swl_visible(b->scene[bank],path,false);}
}
static void show_page(Book *b){
 hide_pages(b);unsigned bank=page_bank(b,b->page),slot=b->page-bank_start(b,bank);
 char path[24];snprintf(path,sizeof path,"root.page%u",slot);
 swl_visible(b->scene[bank],path,true);
 game_port_snap(b->port);
}
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)local;Book *b=game;return bank==page_bank(b,b->page);}
static void *create(GamePort *port){
 Book *b=calloc(1,sizeof *b);if(!b)return NULL;b->port=port;
 const char *key=game_port_descriptor(port)->key;
 b->book=&BOOK_DATA[!strcmp(key,"book3")?2:!strcmp(key,"book1")?0:1];
 return b;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Book *b=game;
 for(unsigned i=0;i<BOOK_BANKS;i++){
  b->bank[i]=game_port_bank(b->port,i);b->scene[i]=game_bank_scene(b->bank[i]);if(!b->scene[i])return false;
 }
 hide_pages(b);b->screen=SCREEN_MENU;b->page=0;return true;
}
static void tick(void *game){Book *b=game;for(unsigned i=0;i<BOOK_BANKS;i++)if(b->scene[i])swl_tick_deferred(b->scene[i]);}
static void start_reading(Book *b){b->page=0;b->screen=SCREEN_READING;b->started=true;show_page(b);}
static void frame(void *game,float seconds,const GameInput *input){
 Book *b=game;(void)seconds;unsigned clicked=game_port_clicked(b->port);
 if(b->screen==SCREEN_MENU){
  if(clicked==BUTTON_EXIT){game_port_leave(b->port);return;}
  if(clicked==BUTTON_START||(input->down&GAME_KEY_A))start_reading(b);
  return;
 }
 if(b->screen==SCREEN_READING){
  if(clicked==BUTTON_EXIT){game_port_leave(b->port);return;}
  if(clicked==BUTTON_PREV||(input->down&GAME_KEY_LEFT)){
   if(b->page){b->page--;show_page(b);}return;
  }
  if(clicked==BUTTON_NEXT||(input->down&GAME_KEY_RIGHT)||(input->down&GAME_KEY_A)){
   if(b->page+1<b->book->pages){b->page++;show_page(b);}else b->screen=SCREEN_RESULT;
  }
  return;
 }
 if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(b->port,BOOK_PAGE_SCORE);
 else if(clicked==BUTTON_AGAIN){b->screen=SCREEN_MENU;b->page=0;}
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Book *b=game;if(b->screen==SCREEN_RESULT)game_port_finish(b->port,BOOK_PAGE_SCORE);else game_port_leave(b->port);}
static unsigned wrap(const char *text,char lines[][48],unsigned capacity){
 unsigned count=0,length=0;lines[0][0]=0;
 for(const char *word=text;;){
  while(*word==' ')word++;
  if(!*word)break;
  const char *end=word;while(*end&&*end!=' ')end++;
  unsigned size=(unsigned)(end-word);
  if(length&&length+1+size>=47){count++;if(count>=capacity)return count;length=0;lines[count][0]=0;}
  if(length){lines[count][length++]=' ';lines[count][length]=0;}
  if(size>46)size=46;
  memcpy(lines[count]+length,word,size);length+=size;lines[count][length]=0;
  word=end;
 }
 return count+1;
}
static void draw_bottom(void *game){
 Book *b=game;GamePort *p=b->port;char line[96],lines[14][48];
 if(b->screen==SCREEN_MENU){
  game_port_title_band(p);
  game_port_text(p,160,54,14,0xffffff,GAME_ALIGN_CENTER,b->book->title);
  game_port_text(p,160,80,10,0xd6ecff,GAME_ALIGN_CENTER,"Paint By Letters");
  game_port_text(p,160,104,9,0xffffff,GAME_ALIGN_CENTER,"Read all ten pages to earn 48 coins.");
  game_port_button(p,BUTTON_START,90,150,140,40,"Start reading",true);
  game_port_button(p,BUTTON_EXIT,260,2,54,20,"Exit",true);
  return;
 }
 if(b->screen==SCREEN_RESULT){
  game_port_text(p,160,70,15,0xffffff,GAME_ALIGN_CENTER,"Book complete!");
  snprintf(line,sizeof line,"You earned %u coins.",BOOK_PAGE_SCORE/10);
  game_port_text(p,160,102,11,0xffffff,GAME_ALIGN_CENTER,line);
  game_port_button(p,BUTTON_PAY,70,150,90,40,"Get coins",true);
  game_port_button(p,BUTTON_AGAIN,180,150,90,40,"Read again",true);
  return;
 }
 unsigned pages=wrap(b->book->texts[b->page],lines,14);
 game_port_text(p,6,14,10,0x6b4a2b,GAME_ALIGN_LEFT,b->book->title);
 snprintf(line,sizeof line,"Page %u of %u",b->page+1,b->book->pages);
 game_port_text(p,314,14,9,0x6b4a2b,GAME_ALIGN_RIGHT,line);
 for(unsigned i=0;i<pages;i++)game_port_text(p,8,34+i*14,9,0x3b2a18,GAME_ALIGN_LEFT,lines[i]);
 game_port_button(p,BUTTON_PREV,6,192,80,40,"< Prev",b->page>0);
 game_port_button(p,BUTTON_NEXT,122,192,90,40,b->page+1<b->book->pages?"Next >":"Finish",true);
 game_port_button(p,BUTTON_EXIT,240,192,74,40,"Exit",true);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}

const GameController GAME_BOOK1_CONTROLLER={
 .key="book1",.kind=GAME_SOLO,.room=917,.rate=24,
 .banks={"games/book1a.gpk","games/book1b.gpk","games/book1c.gpk"},.order={0,1,2},.backgroundRGB=0xeaeaff,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,
};
const GameController GAME_BOOK2_CONTROLLER={
 .key="book2",.kind=GAME_SOLO,.room=918,.rate=24,
 .banks={"games/book2a.gpk","games/book2b.gpk","games/book2c.gpk"},.order={0,1,2},.backgroundRGB=0xf6efdc,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,
};
const GameController GAME_BOOK3_CONTROLLER={
 .key="book3",.kind=GAME_SOLO,.room=919,.rate=24,
 .banks={"games/book3a.gpk","games/book3b.gpk","games/book3c.gpk"},.order={0,1,2},.backgroundRGB=0xf6efdc,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,
};
