#include "game_controller.h"
#include "game_book_data.h"
#include "game_book.h"
#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Paint By Letters: type each letter of the page to paint the story, as in the original books.
   Upper screen: the original open book (cover board, left page, spine) with the story on the left page and the
   page picture painting itself in the right-page window. Lower screen: the current word, a large keyboard and
   page controls. Each page is its own bank (games/bookN-pK.gpk, original art with every paint frame); two page
   slots alternate so the next page loads while the current one is read. Slot 2 holds the cover and the spread. */
#define BOOK_PAGE_SCORE 480
#define BOOK_SLOTS 2
#define BOOK_COVER_SLOT 2
#define BOOK_ENDING 0xffu
#define BOOK_NONE 0xfeu
enum {SCREEN_MENU,SCREEN_READING,SCREEN_RESULT};
enum {BUTTON_START=1,BUTTON_PREV,BUTTON_NEXT,BUTTON_EXIT,BUTTON_PAY,BUTTON_AGAIN,BUTTON_REVEAL,BUTTON_KEY=100};
static const char KEY_ROWS[3][11]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
#define BOOK_HINT_AFTER 3
/* Keyboard geometry (lower screen). */
#define KEY_W 29.6f
#define KEY_H 32.0f
#define KEY_GAP 2.0f
#define KEY_TOP 92.0f
#define KEY_PITCH 35.0f
/* Colours. */
#define INK_TYPED 0x1b1b2cu
#define INK_UNTYPED 0x8a8278u
#define INK_WORD 0xa8361cu
#define INK_MARK 0xe0731fu
#define BOOK_RED 0x7a1212u
#define PARCHMENT 0xf4ead3u

typedef struct {
 GamePort *port;
 const BookData *book;
 SWLScene *cover,*slot[BOOK_SLOTS];
 unsigned slotPage[BOOK_SLOTS];
 unsigned short applied[BOOK_SLOTS][BOOK_PAINT_MAX];
 char resource[BOOK_SLOTS][32],coverResource[32];
 unsigned screen,page,revealed,errors,misses;
 float clock,wrongAt,rightAt;char wrongKey;
 bool started;
} Book;

const BookData *game_book_data(void *game){return ((Book *)game)->book;}
unsigned game_book_page(void *game){return ((Book *)game)->page;}
static bool letter(char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');}
static char upper(char c){return c>='a'&&c<='z'?(char)(c-32):c;}
static const char *page_text(const Book *b){return b->book->texts[b->page];}
static unsigned text_length(const Book *b){return (unsigned)strlen(page_text(b));}
static bool page_complete(const Book *b){return b->revealed>=text_length(b);}
/* Punctuation and spaces before the first letter, and after each typed letter, appear on their own. */
static void skip_marks(Book *b){const char *s=page_text(b);unsigned n=text_length(b);while(b->revealed<n&&!letter(s[b->revealed]))b->revealed++;}
static char expected_letter(const Book *b){const char *s=page_text(b);for(unsigned i=b->revealed;s[i];i++)if(letter(s[i]))return upper(s[i]);return 0;}
static void letter_sound(Book *b,bool correct){if(b->book->room!=917)game_port_sound(b->port,b->book->room==919?(correct?"sndCorrect":"sndWrong"):(correct?"sndPianoSoft":"sndPianoHard"),80,false);}
static void page_sound(Book *b,bool finished){game_port_sound(b->port,b->book->room==917?"snd2":b->book->room==919?(finished?"sndLightON":"sndSWISH"):(finished?"sndDrumHard":"sndBoing"),80,false);}
unsigned game_book_revealed(void *game){return ((Book *)game)->revealed;}
unsigned game_book_errors(void *game){return ((Book *)game)->errors;}
bool game_book_type(void *game,char c){
 Book *b=game;if(b->screen!=SCREEN_READING||page_complete(b))return false;
 char want=expected_letter(b);if(!want){b->revealed=text_length(b);return false;}
 if(upper(c)!=want){b->errors++;b->misses++;b->wrongKey=upper(c);b->wrongAt=b->clock;letter_sound(b,false);return false;}
 const char *s=page_text(b);while(s[b->revealed]&&!letter(s[b->revealed]))b->revealed++;
 b->revealed++;b->misses=0;b->rightAt=b->clock;skip_marks(b);letter_sound(b,true);return true;
}
static void open_page(Book *b){b->revealed=0;b->misses=0;b->wrongKey=0;skip_marks(b);}

/* ---- Page banks ---- */
static const char *book_key(const Book *b){return b->book->id;}
static void slot_name(const Book *b,unsigned page,char *out,size_t size){
 if(page==BOOK_ENDING)snprintf(out,size,"games/%s-end.gpk",book_key(b));else snprintf(out,size,"games/%s-p%u.gpk",book_key(b),page+1);
}
static const char *bank_resource(void *game,unsigned bank){
 Book *b=game;if(bank==BOOK_COVER_SLOT)return b->coverResource;
 return bank<BOOK_SLOTS&&b->slotPage[bank]!=BOOK_NONE?b->resource[bank]:NULL;
}
static void paint_slot(Book *b,unsigned s);
static void attach(Book *b,unsigned s){
 b->slot[s]=game_bank_scene(game_port_bank(b->port,s));
 memset(b->applied[s],0,sizeof b->applied[s]);
 if(b->slot[s]&&b->slotPage[s]==b->page&&b->screen==SCREEN_READING){paint_slot(b,s);game_port_snap(b->port);}
}
/** The slot holding `page` once its bank is ready; starts (or continues) its load otherwise. */
static int need(Book *b,unsigned page){
 int s=-1;
 for(unsigned i=0;i<BOOK_SLOTS;i++)if(b->slotPage[i]==page)s=(int)i;
 if(s<0){
  /* Reuse a slot that holds neither the page on screen nor this one. */
  for(unsigned i=0;i<BOOK_SLOTS&&s<0;i++)if(b->slotPage[i]==BOOK_NONE)s=(int)i;
  for(unsigned i=0;i<BOOK_SLOTS&&s<0;i++)if(b->slotPage[i]!=b->page||b->screen!=SCREEN_READING)s=(int)i;
  if(s<0)return -1;
  game_port_release_bank(b->port,(unsigned)s);b->slot[s]=NULL;
  b->slotPage[s]=page;slot_name(b,page,b->resource[s],sizeof b->resource[s]);
 }
 if(b->slot[s])return s;
 if(game_port_require_bank(b->port,(unsigned)s)!=GAME_RESOURCE_READY)return -1;
 attach(b,(unsigned)s);return b->slot[s]?s:-1;
}
static int shown_slot(const Book *b){
 unsigned want=b->screen==SCREEN_RESULT?BOOK_ENDING:b->screen==SCREEN_READING?b->page:BOOK_NONE;
 if(want==BOOK_NONE)return -1;
 for(unsigned i=0;i<BOOK_SLOTS;i++)if(b->slotPage[i]==want&&b->slot[i])return (int)i;
 return -1;
}
/* Original paint progression: every clip the text has reached shows the art frame of its last step. */
static void paint_slot(Book *b,unsigned s){
 SWLScene *scene=b->slot[s];if(!scene||b->slotPage[s]>=b->book->pages)return;
 const BookPaint *steps=b->book->paint?b->book->paint[b->slotPage[s]]:NULL;if(!steps)return;
 bool current=b->slotPage[s]==b->page&&b->screen==SCREEN_READING;unsigned reached=current?b->revealed:0xffffu;
 for(unsigned i=0;i<BOOK_PAINT_MAX&&steps[i].frame;i++){
  bool first=true;for(unsigned j=0;j<i;j++)if(!strcmp(steps[j].path,steps[i].path))first=false;
  if(!first)continue;
  unsigned frame=1;for(unsigned j=i;j<BOOK_PAINT_MAX&&steps[j].frame;j++)if(!strcmp(steps[j].path,steps[i].path)&&reached>=steps[j].at)frame=steps[j].frame;
  if(b->applied[s][i]==frame)continue;
  char path[96];snprintf(path,sizeof path,steps[i].path[0]?"root.page.%s":"root.page",steps[i].path);
  /* Nested clips are not instantiated until their parent's frame is drawn, so record the frame as an override
     (swl_target) and only remember it once accepted; otherwise a failed step would never be retried. */
  if(swl_target(scene,path,frame,NULL,false))b->applied[s][i]=(unsigned short)frame;
 }
}
static void paint_page(Book *b){int s=shown_slot(b);if(s>=0)paint_slot(b,(unsigned)s);}
static void show_page(Book *b){
 for(unsigned i=0;i<BOOK_SLOTS;i++)if(b->slotPage[i]==b->page)memset(b->applied[i],0,sizeof b->applied[i]);
 need(b,b->page);paint_page(b);game_port_snap(b->port);
}
static void sync_cover(Book *b){
 if(!b->cover)return;
 swl_visible(b->cover,"root.cover",b->screen==SCREEN_MENU||(b->screen==SCREEN_RESULT&&shown_slot(b)<0));
 swl_visible(b->cover,"root.spread",b->screen==SCREEN_READING);
}
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){
 (void)local;Book *b=game;
 if(bank==BOOK_COVER_SLOT)return true;
 return (int)bank==shown_slot(b);
}
static void *create(GamePort *port){
 Book *b=calloc(1,sizeof *b);if(!b)return NULL;b->port=port;
 const char *key=game_port_descriptor(port)->key;
 b->book=&BOOK_DATA[!strcmp(key,"book3")?2:!strcmp(key,"book1")?0:1];
 for(unsigned i=0;i<BOOK_SLOTS;i++)b->slotPage[i]=BOOK_NONE;
 snprintf(b->coverResource,sizeof b->coverResource,"games/%s-cover.gpk",b->book->id);
 return b;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Book *b=game;b->cover=game_bank_scene(game_port_bank(b->port,BOOK_COVER_SLOT));if(!b->cover)return false;
 b->screen=SCREEN_MENU;b->page=0;sync_cover(b);
 /* The first page loads behind the cover. */
 need(b,0);
 return true;
}
static void tick(void *game){
 Book *b=game;
 for(unsigned i=0;i<BOOK_SLOTS;i++){
  if(b->slotPage[i]!=BOOK_NONE&&!b->slot[i]&&game_port_require_bank(b->port,i)==GAME_RESOURCE_READY)attach(b,i);
  if(b->slot[i])swl_tick_deferred(b->slot[i]);
 }
 if(b->cover)swl_tick_deferred(b->cover);
 if(b->screen==SCREEN_READING){
#if defined(GP_LAN_ONLY)&&defined(BOOK_DEV_AUTOTYPE)
  /* Emulator captures only (tools-local/build-capture.sh): types a letter per tick up to BOOK_DEV_AUTOTYPE %
     of the page, so the paint progression is visible. Never defined in normal builds. */
  if(b->revealed*100<text_length(b)*BOOK_DEV_AUTOTYPE){char c=expected_letter(b);if(c)game_book_type(b,c);}
#endif
  paint_page(b);
  /* Next page (or the closing page) loads in the other slot once this page is painted: the port's "Loading..."
     box then never covers the keyboard while the player types. */
  if(shown_slot(b)>=0&&page_complete(b))need(b,b->page+1<b->book->pages?b->page+1:BOOK_ENDING);
 }
 sync_cover(b);
}
static void start_reading(Book *b){b->page=0;b->screen=SCREEN_READING;b->started=true;b->errors=0;open_page(b);show_page(b);sync_cover(b);}
static void turn_to(Book *b,unsigned page,bool painted){
 b->page=page;open_page(b);if(painted)b->revealed=text_length(b);show_page(b);page_sound(b,false);
}
static void frame(void *game,float seconds,const GameInput *input){
 Book *b=game;b->clock+=seconds;unsigned clicked=game_port_clicked(b->port);
 if(b->screen==SCREEN_MENU){
  if(clicked==BUTTON_EXIT){game_port_leave(b->port);return;}
  if(clicked==BUTTON_START||(input->down&GAME_KEY_A))start_reading(b);
  return;
 }
 if(b->screen==SCREEN_READING){
  paint_page(b);
  if(clicked==BUTTON_EXIT){game_port_leave(b->port);return;}
  if(clicked>=BUTTON_KEY&&clicked<BUTTON_KEY+26){game_book_type(b,(char)('A'+clicked-BUTTON_KEY));paint_page(b);return;}
  if(clicked==BUTTON_REVEAL&&!page_complete(b)){b->revealed=text_length(b);paint_page(b);return;}
  if((input->down&GAME_KEY_Y)&&!page_complete(b)){b->revealed=text_length(b);paint_page(b);return;}
#if defined(GP_LAN_ONLY)&&defined(BOOK_DEV_RESULT)
  if(page_complete(b)){b->screen=SCREEN_RESULT;need(b,BOOK_ENDING);sync_cover(b);return;}
#endif
#if defined(GP_LAN_ONLY)&&defined(BOOK_DEV_AUTOTYPE)
  if(input->down&(GAME_KEY_LEFT|GAME_KEY_RIGHT|GAME_KEY_A))return;
#endif
  if(clicked==BUTTON_PREV||(input->down&(GAME_KEY_LEFT|GAME_KEY_L))){if(b->page)turn_to(b,b->page-1,true);return;}
  /* A (or the centre button) turns the page once it is painted; the arrows and Next always move on, as on PC. */
  bool turn=clicked==BUTTON_NEXT||(input->down&(GAME_KEY_RIGHT|GAME_KEY_R))||((clicked==BUTTON_REVEAL||(input->down&GAME_KEY_A))&&page_complete(b));
  if(turn){
   if(b->page+1<b->book->pages)turn_to(b,b->page+1,false);
   else{b->screen=SCREEN_RESULT;page_sound(b,true);need(b,BOOK_ENDING);sync_cover(b);game_port_snap(b->port);}
  }
  return;
 }
#if defined(GP_LAN_ONLY)&&defined(BOOK_DEV_RESULT)
 return;
#endif
 if(clicked==BUTTON_PAY||(input->down&GAME_KEY_A))game_port_finish(b->port,BOOK_PAGE_SCORE);
 else if(clicked==BUTTON_AGAIN||(input->down&GAME_KEY_B)){b->screen=SCREEN_MENU;b->page=0;sync_cover(b);need(b,0);}
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Book *b=game;if(b->screen==SCREEN_RESULT)game_port_finish(b->port,BOOK_PAGE_SCORE);else game_port_leave(b->port);}

/* ---- Text layout with the client font ---- */
static unsigned font_row(unsigned size){return (size<FONT_MIN?FONT_MIN:size>FONT_MAX?FONT_MAX:size)-FONT_MIN;}
static float advance(unsigned size,char c){unsigned char u=(unsigned char)c;return fontGlyphs[font_row(size)][(u>=32&&u<127?u:'?')-32].advance;}
static float span_width(unsigned size,const char *s,unsigned n){float w=0;for(unsigned i=0;i<n&&s[i];i++)w+=advance(size,s[i]);return w;}
/** Greedy word wrap in pixels; returns the line count (starts/ends index into text, spaces excluded). */
static unsigned wrap(const char *text,unsigned size,float width,unsigned starts[],unsigned ends[],unsigned capacity){
 unsigned count=0;float used=0;const char *s=text;
 while(*s){
  while(*s==' ')s++;
  if(!*s)break;
  const char *end=s;while(*end&&*end!=' ')end++;
  float w=span_width(size,s,(unsigned)(end-s)),space=advance(size,' ');
  if(count&&used>0&&used+space+w<=width){ends[count-1]=(unsigned)(end-text);used+=space+w;}
  else{if(count>=capacity)return count;starts[count]=(unsigned)(s-text);ends[count]=(unsigned)(end-text);count++;used=w;}
  s=end;
 }
 return count;
}
static void text_span(GamePort *p,float x,float y,unsigned size,uint32_t rgb,const char *s,unsigned n){
 char buffer[128];if(n>=sizeof buffer)n=sizeof buffer-1;memcpy(buffer,s,n);buffer[n]=0;
 if(n)game_port_text(p,x,y,(float)size,rgb,GAME_ALIGN_LEFT,buffer);
}
/* The word being typed: letters with inner apostrophes and hyphens ("wouldn't", "lime-colored"). */
static void current_word(const Book *b,unsigned *begin,unsigned *end){
 const char *s=page_text(b);unsigned n=text_length(b),at=b->revealed<n?b->revealed:n;
 unsigned i=at;while(i&&(letter(s[i-1])||((s[i-1]=='\''||s[i-1]=='-')&&i>1&&letter(s[i-2]))))i--;
 unsigned j=at;while(j<n&&(letter(s[j])||((s[j]=='\''||s[j]=='-')&&letter(s[j+1]))))j++;
 *begin=i;*end=j;
}

/* ---- Upper screen ---- */
/* Stage framing: the open book (cover board 83..691 x 25..423) fills the screen. */
#define SPREAD_SCALE .6f
#define SPREAD_CX 387.0f
#define SPREAD_CY 224.0f
static float sx(float x){return (x-SPREAD_CX)*SPREAD_SCALE+200;}
static float sy(float y){return (y-SPREAD_CY)*SPREAD_SCALE+120;}
static bool top_view(void *game,SWLMatrix *view){
 Book *b=game;
 if(b->screen==SCREEN_READING){*view=(SWLMatrix){SPREAD_SCALE,0,0,SPREAD_SCALE,200-SPREAD_CX*SPREAD_SCALE,120-SPREAD_CY*SPREAD_SCALE};return true;}
 /* Cover and closing page: the 300x416 board at 246..547 x 21..437, centred. */
 float s=.56f;*view=(SWLMatrix){s,0,0,s,200-397*s,120-229*s};return true;
}
static void draw_top(void *game){
 Book *b=game;if(b->screen!=SCREEN_READING)return;
 GamePort *p=b->port;char line[64];
 const char *text=page_text(b);
 /* While the page picture loads, its window stays a blank page. */
 if(shown_slot(b)<0)game_port_rect(p,sx(392),sy(44),sx(660)-sx(392),sy(402)-sy(44),0xffffff);
 /* Left page writing area (stage 112..366 x 52..402). */
 float left=sx(112),right=sx(368),top=sy(48),bottom=sy(404);
 snprintf(line,sizeof line,"%s",b->book->title);
 game_port_text(p,(left+right)/2,top,10,BOOK_RED,GAME_ALIGN_CENTER,line);
 game_port_rect(p,(left+right)/2-30,top+15,60,1,0xd8c4a0);
 unsigned starts[24],ends[24],size=12,count=0;float lineH=17;
 for(;size>=10;size--){lineH=size+5.0f;count=wrap(text,size,right-left,starts,ends,24);if(top+24+count*lineH<=bottom-16)break;}
 if(size<10)size=10;
 unsigned wb,we;current_word(b,&wb,&we);bool done=page_complete(b);
 for(unsigned i=0;i<count;i++){
  float y=top+24+i*lineH,x=left;unsigned a=starts[i],z=ends[i];
  /* Segments: typed, current word (rest), untyped. */
  unsigned typedEnd=b->revealed<a?a:b->revealed>z?z:b->revealed;
  text_span(p,x,y,size,INK_TYPED,text+a,typedEnd-a);x+=span_width(size,text+a,typedEnd-a);
  if(done)continue;
  unsigned wordEnd=typedEnd;if(typedEnd>=wb&&typedEnd<we)wordEnd=we<z?we:z;
  if(wordEnd>typedEnd){
   /* Next letter: an orange underline under it. */
   if(b->revealed>=a&&b->revealed<z){float w=advance(size,text[b->revealed]);game_port_rect(p,x-1,y+size+1,w+1,2,INK_MARK);}
   text_span(p,x,y,size,INK_WORD,text+typedEnd,wordEnd-typedEnd);x+=span_width(size,text+typedEnd,wordEnd-typedEnd);
  }
  text_span(p,x,y,size,INK_UNTYPED,text+wordEnd,z-wordEnd);
 }
 snprintf(line,sizeof line,"- %u -",b->page+1);
 game_port_text(p,(left+right)/2,bottom-11,10,0x9a8a70,GAME_ALIGN_CENTER,line);
}

/* ---- Lower screen ---- */
static void header(Book *b,const char *right){
 GamePort *p=b->port;
 game_port_rect(p,0,0,320,24,BOOK_RED);game_port_rect(p,0,24,320,1,0x4a0808);
 game_port_text(p,8,6,10,0xfff2dc,GAME_ALIGN_LEFT,b->book->title);
 if(right)game_port_text(p,252,6,10,0xffd9a8,GAME_ALIGN_RIGHT,right);
 game_port_button(p,BUTTON_EXIT,262,2,54,20,"Exit",true);
}
static void outline(GamePort *p,float x,float y,float w,float h,float t,uint32_t rgb){
 game_port_rect(p,x,y,w,t,rgb);game_port_rect(p,x,y+h-t,w,t,rgb);game_port_rect(p,x,y,t,h,rgb);game_port_rect(p,x+w-t,y,t,h,rgb);
}
static void word_card(Book *b){
 GamePort *p=b->port;const char *s=page_text(b);
 game_port_rect(p,6,28,308,44,0xfffdf8);outline(p,6,28,308,44,1,0xd6c29c);
 if(page_complete(b)){
  game_port_text(p,160,33,16,0x2e7d32,GAME_ALIGN_CENTER,"Page painted!");
  game_port_text(p,160,54,10,0x6b5a40,GAME_ALIGN_CENTER,b->page+1<b->book->pages?"Press A or tap Next to turn the page.":"Press A or tap Finish to close the book.");
  return;
 }
 unsigned wb,we;current_word(b,&wb,&we);unsigned n=we-wb;if(n>14)n=14;
 float tw=n>10?20:26,gap=3,total=n*tw+(n?n-1:0)*gap,x0=160-total/2;
 for(unsigned i=0;i<n;i++){
  unsigned at=wb+i;char c[2]={s[at],0};float x=x0+i*(tw+gap),y=32;
  bool typed=at<b->revealed,next=at==b->revealed;
  if(!letter(s[at])){game_port_text(p,x+tw/2,y+8,16,INK_UNTYPED,GAME_ALIGN_CENTER,c);continue;}
  c[0]=upper(c[0]);
  if(next){
   bool shake=b->wrongKey&&b->clock-b->wrongAt<.35f;float dx=shake?((int)((b->clock-b->wrongAt)*40)%2?2:-2):0;
   game_port_rect(p,x+dx-1,y-1,tw+2,38,INK_MARK);game_port_rect(p,x+dx+1,y+1,tw-2,34,0xfff1c9);
   game_port_text(p,x+dx+tw/2,y+9,16,INK_WORD,GAME_ALIGN_CENTER,c);
  }else if(typed){
   game_port_rect(p,x,y,tw,36,0xe9f2e4);outline(p,x,y,tw,36,1,0xa9c79b);
   game_port_text(p,x+tw/2,y+9,16,INK_TYPED,GAME_ALIGN_CENTER,c);
  }else{
   game_port_rect(p,x,y,tw,36,0xf3eee5);outline(p,x,y,tw,36,1,0xddd3c2);
   game_port_text(p,x+tw/2,y+9,16,0xb0a796,GAME_ALIGN_CENTER,c);
  }
 }
}
static void draw_bottom(void *game){
 Book *b=game;GamePort *p=b->port;char line[96];
 if(b->screen==SCREEN_MENU){
  game_port_rect(p,0,0,320,240,PARCHMENT);
  header(b,NULL);
  game_port_text(p,160,36,10,0x8a5a2a,GAME_ALIGN_CENTER,"Paint By Letters");
  game_port_text(p,160,52,16,BOOK_RED,GAME_ALIGN_CENTER,b->book->title);
  snprintf(line,sizeof line,"Read all %u pages to finish the book.",b->book->pages);
  game_port_text(p,160,80,10,0x4a3a28,GAME_ALIGN_CENTER,line);
  game_port_text(p,160,96,10,0x4a3a28,GAME_ALIGN_CENTER,"Type each word and the picture paints itself.");
  game_port_button(p,BUTTON_START,70,150,180,44,"A  Start reading",true);
  game_port_text(p,160,206,10,0x7a6a50,GAME_ALIGN_CENTER,"Y: show the whole page    L/R: turn pages");
  return;
 }
 if(b->screen==SCREEN_RESULT){
  game_port_rect(p,0,0,320,240,PARCHMENT);
  header(b,"The End");
  game_port_text(p,160,40,16,BOOK_RED,GAME_ALIGN_CENTER,"Book complete!");
  snprintf(line,sizeof line,"You read all %u pages of %s.",b->book->pages,b->book->title);
  game_port_text(p,160,66,10,0x4a3a28,GAME_ALIGN_CENTER,line);
  snprintf(line,sizeof line,"Book score: %u",BOOK_PAGE_SCORE);
  game_port_text(p,160,92,12,0x1b1b2c,GAME_ALIGN_CENTER,line);
  game_port_button(p,BUTTON_PAY,40,150,116,44,"A  Get coins",true);
  game_port_button(p,BUTTON_AGAIN,164,150,116,44,"B  Read again",true);
  return;
 }
 bool done=page_complete(b);
 game_port_rect(p,0,0,320,240,PARCHMENT);
 snprintf(line,sizeof line,"Page %u of %u",b->page+1,b->book->pages);
 header(b,line);
 word_card(b);
 char want=expected_letter(b);
 if(!done){
  bool wrong=b->wrongKey&&b->clock-b->wrongAt<1.2f;
  if(wrong||b->misses>=BOOK_HINT_AFTER)snprintf(line,sizeof line,"Not quite! Look for the letter %c.",want);
  else snprintf(line,sizeof line,"Tap the letter in the orange box.");
  game_port_text(p,160,75,10,wrong?0xb3261e:0x6b5a40,GAME_ALIGN_CENTER,line);
 }
 for(unsigned r=0;r<3;r++){
  unsigned keys=(unsigned)strlen(KEY_ROWS[r]);float x0=(320-(keys*KEY_W+(keys-1)*KEY_GAP))/2,y=KEY_TOP+r*KEY_PITCH;
  for(unsigned k=0;k<keys;k++){
   char label[2]={KEY_ROWS[r][k],0};float x=x0+k*(KEY_W+KEY_GAP);
   game_port_button(p,BUTTON_KEY+(unsigned)(label[0]-'A'),x,y,KEY_W,KEY_H,label,!done);
   if(!done&&b->misses>=BOOK_HINT_AFTER&&label[0]==want)outline(p,x-1,y-1,KEY_W+2,KEY_H+2,2,INK_MARK);
   if(!done&&b->wrongKey==label[0]&&b->clock-b->wrongAt<.35f)outline(p,x-1,y-1,KEY_W+2,KEY_H+2,2,0xd32f2f);
  }
 }
 game_port_button(p,BUTTON_PREV,4,202,72,34,"< Prev",b->page>0);
 game_port_button(p,BUTTON_REVEAL,82,202,156,34,done?(b->page+1<b->book->pages?"A  Next page":"A  Finish the book"):"Y  Show the page",true);
 game_port_button(p,BUTTON_NEXT,244,202,72,34,b->page+1<b->book->pages?"Next >":"Finish",true);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}

#define BOOK_CONTROLLER(name,keyName,roomId,pageBank,coverBank) \
const GameController name={ \
 .key=keyName,.kind=GAME_SOLO,.room=roomId,.rate=24, \
 .banks={pageBank,pageBank,coverBank},.lazy=3,.bank_resource=bank_resource,.order={0,1,2},.nightPanel=true,.backgroundRGB=0xeaeaff, \
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet, \
 .close=close_game,.draw_bottom=draw_bottom,.bank_view=bank_view,.top_view=top_view,.draw_top=draw_top, \
};
BOOK_CONTROLLER(GAME_BOOK1_CONTROLLER,"book1",917,"games/book1-p1.gpk","games/book1-cover.gpk")
BOOK_CONTROLLER(GAME_BOOK2_CONTROLLER,"book2",918,"games/book2-p1.gpk","games/book2-cover.gpk")
BOOK_CONTROLLER(GAME_BOOK3_CONTROLLER,"book3",919,"games/book3-p1.gpk","games/book3-cover.gpk")
