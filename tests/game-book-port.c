#include "game_controller.h"
#include "game_book.h"
#include "game_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,coins,left;char menu[96],result[96],sound[32];} Fixture;
static void sound(void *user,const char *bank,const char *name,unsigned volume,bool loop){Fixture *f=user;assert(!strncmp(bank,"book",4)&&volume==80&&!loop);snprintf(f->sound,sizeof f->sound,"%s",name);}
static void text(void *user,float x,float y,float size,uint32_t rgb,GameAlign align,const char *value){
 Fixture *f=user;(void)x;(void)y;(void)size;(void)rgb;(void)align;
 if(!strncmp(value,"Read all ",9))snprintf(f->menu,sizeof f->menu,"%s",value);
 if(!strncmp(value,"Book score:",11))snprintf(f->result,sizeof f->result,"%s",value);
 assert(strncmp(value,"You earned ",11));
}
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool sendp(void *user,char extension,const char *command,unsigned count,const char *const *args){
 Fixture *f=user;
 if(!strcmp(command,"j#jr")){assert(extension=='s'&&count==3);assert(!strcmp(args[0],"917")||!strcmp(args[0],"918")||!strcmp(args[0],"919"));f->join++;}
 else if(!strcmp(command,"zo")){assert(extension=='z'&&count==1);assert(!strcmp(args[0],"480"));f->zo++;}else assert(false);
 return true;
}
static void leave(void *user,unsigned room,int x,int y){Fixture *f=user;assert(room==121&&x==380&&y==300);f->left++;}
static void coins(void *user,unsigned total){((Fixture *)user)->coins=total;}
static void tick(GamePort *p,unsigned count){for(unsigned i=0;i<count;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static void press(GamePort *p,unsigned key){game_port_update(p,1.0f/24,&(GameInput){.down=key});tick(p,2);}
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}
static void tap(GamePort *p,float x,float y){game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);game_port_update(p,1.0f/24,&(GameInput){0});}

/** The page bank on screen (exactly one page slot is drawn while reading). */
static SWLScene *shown(GamePort *p){
 const GameController *c=game_port_controller(p);SWLMatrix local;SWLScene *scene=NULL;unsigned count=0;
 for(unsigned bank=0;bank<2;bank++)if(c->bank_view(game_port_game(p),bank,&local)){scene=game_bank_scene(game_port_bank(p,bank));count++;}
 assert(count==1&&scene);return scene;
}
/** Every painted page shows the last original paint frame of each clip a step drives. */
static void check_painted(GamePort *p,const BookData *book,unsigned page){
 SWLScene *scene=shown(p);assert(swl_frame(scene,"root.page")>=1);
 const BookPaint *steps=book->paint[page];
 for(unsigned i=0;i<BOOK_PAINT_MAX&&steps[i].frame;i++){
  unsigned last=0;for(unsigned j=0;j<BOOK_PAINT_MAX&&steps[j].frame;j++)if(!strcmp(steps[j].path,steps[i].path))last=steps[j].frame;
  char path[96];snprintf(path,sizeof path,steps[i].path[0]?"root.page.%s":"root.page",steps[i].path);
  unsigned frame=swl_frame(scene,path);
  if(frame!=last)fprintf(stderr,"%s page %u %s: frame %u, want %u\n",book->id,page+1,path,frame,last);
  assert(frame==last);
 }
 assert(render(scene)>0);
}

static GamePort *open_book(const char *key,unsigned room,Fixture *f){
 memset(f,0,sizeof *f);GameDescriptor descriptor={.key="",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 snprintf(descriptor.key,sizeof descriptor.key,"%s",key);
 GameHost host={.user=f,.resource=resource,.send=sendp,.leave=leave,.coins=coins,.text=text,.sound=sound};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);
 char roomArg[8];snprintf(roomArg,sizeof roomArg,"%u",room);char *join[]={roomArg};
 assert(game_port_packet(p,"jg",1,join));assert(f->join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 tick(p,2);assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 return p;
}
/** Reads every page with Y (show page), checks its paint, and turns with A; returns at the closing page. */
static void read_all(GamePort *p,const BookData *book){
 void *g=game_port_game(p);
 for(unsigned page=0;page<book->pages;page++){
  assert(game_book_page(g)==page);tick(p,2);
  press(p,GAME_KEY_Y);assert(game_book_revealed(g)==strlen(book->texts[page]));
  check_painted(p,book,page);
  assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
  press(p,GAME_KEY_A);
 }
}

int main(void){
 Fixture f1;GamePort *b1=open_book("book1",917,&f1);
 const BookData *book1=game_book_data(game_port_game(b1));assert(book1&&!strcmp(book1->id,"book1")&&book1->pages==6&&book1->room==917);
 game_port_draw_bottom(b1);assert(!strcmp(f1.menu,"Read all 6 pages to finish the book."));
 press(b1,GAME_KEY_A);
 assert(game_book_page(game_port_game(b1))==0);
 assert(swl_visible(shown(b1),"root.page",true));assert(render(shown(b1))>0);
 /* Typing: "My puffle is blue..." accepts M then Y in any case, rejects a wrong letter, reveals on Y. */
 void *g1=game_port_game(b1);
 assert(game_book_revealed(g1)==0);
 assert(!game_book_type(g1,'Q')&&game_book_errors(g1)==1&&game_book_revealed(g1)==0);
 assert(game_book_type(g1,'m')&&game_book_revealed(g1)==1);
 assert(game_book_type(g1,'Y')&&game_book_revealed(g1)==3);
 game_port_draw_bottom(b1);
 /* Paint follows the text: "My puffle is blue " (18 characters) paints frame 2. */
 assert(swl_frame(shown(b1),"root.page")==1);
 for(const char *c="PUFFLEISBLUE";*c;c++)assert(game_book_type(g1,*c));
 tick(b1,1);assert(game_book_revealed(g1)>=18&&swl_frame(shown(b1),"root.page")==2);
 press(b1,GAME_KEY_A);assert(game_book_page(g1)==0);
 press(b1,GAME_KEY_Y);assert(game_book_revealed(g1)==strlen(book1->texts[0]));
 assert(!game_book_type(g1,'M'));
 press(b1,GAME_KEY_A);assert(game_book_page(g1)==1&&game_book_revealed(g1)==0);
 /* A touch on the on-screen keyboard types too: "It likes..." starts with I (top row, eighth key). */
 tap(b1,3+7*31.6f+14,92+16);assert(game_book_revealed(g1)==1);
 for(const char *c=book1->texts[1]+1;*c;c++)if((*c>='A'&&*c<='Z')||(*c>='a'&&*c<='z'))assert(game_book_type(g1,*c));
 assert(game_book_revealed(g1)==strlen(book1->texts[1]));
 /* Bottom-row key M (last registered key) is reachable: page 3 "My blue puffle..." starts with M. */
 press(b1,GAME_KEY_RIGHT);press(b1,GAME_KEY_RIGHT);
 assert(game_book_page(g1)==3&&game_book_revealed(g1)==0);
 tap(b1,50.4f+6*31.6f+14,92+2*35+16);assert(game_book_revealed(g1)==1);
 /* Going back shows the earlier page already painted. */
 press(b1,GAME_KEY_LEFT);press(b1,GAME_KEY_LEFT);press(b1,GAME_KEY_LEFT);
 assert(game_book_page(g1)==0&&game_book_revealed(g1)==strlen(book1->texts[0]));check_painted(b1,book1,0);
 read_all(b1,book1);
 game_port_draw_bottom(b1);assert(!strcmp(f1.result,"Book score: 480"));assert(f1.coins==0);
 tick(b1,2);assert(game_port_texture_bytes(b1)<=GAME_PORT_TEXTURE_LIMIT);
 press(b1,GAME_KEY_A);
 assert(f1.zo==1);
 char *reply1[]={"510"};assert(game_port_packet(b1,"zo",1,reply1));assert(f1.left==1&&f1.coins==510);
 game_port_free(b1);

 Fixture f;GamePort *p=open_book("book2",918,&f);
 const BookData *book=game_book_data(game_port_game(p));assert(book&&!strcmp(book->id,"book2")&&book->pages==10&&book->room==918);
 /* Original paint progression (Burnt Out Bulbs page 1: lights go out at letter 148, Halloween at 205). */
 assert(book->paint&&book->paint[0][0].at==40&&book->paint[0][0].frame==2&&book->paint[0][5].at==205&&book->paint[0][5].frame==7&&book->paint[0][6].frame==0);
 game_port_draw_bottom(p);assert(!strcmp(f.menu,"Read all 10 pages to finish the book."));
 press(p,GAME_KEY_A);
 assert(game_book_page(game_port_game(p))==0&&swl_frame(shown(p),"root.page")==1);
 /* Some pages have no art before the first paint step (the blank paper comes from the cover bank), so browsing
    only proves the page bank loads; the painted state is rendered in check_painted below. */
 for(unsigned page=1;page<10;page++){press(p,GAME_KEY_RIGHT);assert(game_book_page(game_port_game(p))==page);render(shown(p));}
 for(unsigned page=9;page>0;page--){press(p,GAME_KEY_LEFT);check_painted(p,book,page-1);}
 read_all(p,book);
 press(p,GAME_KEY_A);
 assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 game_port_free(p);

 Fixture f3;GamePort *q=open_book("book3",919,&f3);
 const BookData *book3=game_book_data(game_port_game(q));assert(book3&&!strcmp(book3->id,"book3")&&book3->pages==10&&book3->room==919);
 game_port_draw_bottom(q);assert(!strcmp(f3.menu,"Read all 10 pages to finish the book."));
 press(q,GAME_KEY_A);
 assert(swl_visible(shown(q),"root.page",true));assert(render(shown(q))>0);
 const char *firstLetter=book3->texts[0];while(*firstLetter&&!((*firstLetter>='A'&&*firstLetter<='Z')||(*firstLetter>='a'&&*firstLetter<='z')))firstLetter++;
 assert(*firstLetter);char wrong=(*firstLetter=='Z'||*firstLetter=='z')?'X':'Z';
 assert(!game_book_type(game_port_game(q),wrong));assert(!strcmp(f3.sound,"sndWrong"));
 assert(game_book_type(game_port_game(q),*firstLetter));assert(!strcmp(f3.sound,"sndCorrect"));
 press(q,GAME_KEY_RIGHT);assert(!strcmp(f3.sound,"sndSWISH"));
 press(q,GAME_KEY_LEFT);read_all(q,book3);
 game_port_close(q);assert(f3.zo==1);
 char *reply3[]={"510"};assert(game_port_packet(q,"zo",1,reply3));assert(f3.left==1);
 game_port_free(q);
 puts("PASS Paint By Letters native controller: per-page banks of book1, book2 and book3 load and render, original paint frames for every page, typing/touch keyboard/reveal, original correct/wrong/page audio cues, finish pays 480 once, early exit leaves");
 return 0;
}
