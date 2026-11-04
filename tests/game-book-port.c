#include "game_controller.h"
#include "game_book.h"
#include "game_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {unsigned join,zo,coins,left;} Fixture;
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
static bool emit(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)art;(void)colour;assert(isfinite(matrix->x)&&isfinite(matrix->y));(*(unsigned *)user)++;return true;}
static bool mask(void *user,unsigned event){(void)user;assert(event>=1&&event<=3);return true;}
static unsigned render(SWLScene *scene){unsigned draws=0;bool ok=swl_render(scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws);if(!ok)fprintf(stderr,"render: %s\n",swl_error(scene));assert(ok);return draws;}

static GamePort *open_book(const char *key,unsigned room,Fixture *f){
 memset(f,0,sizeof *f);GameDescriptor descriptor={.key="",.returnRoom=121,.returnX=380,.returnY=300,.colourRGB=0x0099ff,.online=true};
 snprintf(descriptor.key,sizeof descriptor.key,"%s",key);
 GameHost host={.user=f,.resource=resource,.send=sendp,.leave=leave,.coins=coins};
 GamePort *p=game_port_open(&descriptor,&host);assert(p);tick(p,1);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
 char roomArg[8];snprintf(roomArg,sizeof roomArg,"%u",room);char *join[]={roomArg};
 assert(game_port_packet(p,"jg",1,join));assert(f->join==1);assert(game_port_session(p)==GAME_SESSION_PLAYING);
 return p;
}

int main(void){
 Fixture f1;GamePort *b1=open_book("book1",917,&f1);
 const BookData *book1=game_book_data(game_port_game(b1));assert(book1&&!strcmp(book1->id,"book1")&&book1->pages==6&&book1->room==917);
 game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(game_book_page(game_port_game(b1))==0);
 assert(swl_visible(game_bank_scene(game_port_bank(b1,0)),"root.page0",true));
 assert(render(game_bank_scene(game_port_bank(b1,0)))>0);
 for(unsigned page=1;page<3;page++)game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 assert(game_book_page(game_port_game(b1))==2);
 const GameController *c1=game_port_controller(b1);SWLMatrix local1;assert(!c1->bank_view(game_port_game(b1),0,&local1));assert(c1->bank_view(game_port_game(b1),1,&local1));
 assert(swl_visible(game_bank_scene(game_port_bank(b1,1)),"root.page0",true));
 assert(render(game_bank_scene(game_port_bank(b1,1)))>0);
 game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 assert(game_book_page(game_port_game(b1))==4);
 assert(render(game_bank_scene(game_port_bank(b1,2)))>0);
 for(unsigned page=5;page<6;page++){game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});assert(game_book_page(game_port_game(b1))==page);}
 game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 game_port_update(b1,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(f1.zo==1);
 char *reply1[]={"510"};assert(game_port_packet(b1,"zo",1,reply1));assert(f1.left==1&&f1.coins==510);
 game_port_free(b1);

 Fixture f;GamePort *p=open_book("book2",918,&f);
 const BookData *book=game_book_data(game_port_game(p));assert(book&&!strcmp(book->id,"book2")&&book->pages==10&&book->room==918);
 for(unsigned bank=0;bank<3;bank++){SWLScene *scene=game_bank_scene(game_port_bank(p,bank));assert(scene);render(scene);}
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(game_book_page(game_port_game(p))==0);
 SWLScene *first=game_bank_scene(game_port_bank(p,0));assert(swl_visible(first,"root.page0",true));
 for(unsigned page=1;page<5;page++){game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});assert(game_book_page(game_port_game(p))==page);}
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 assert(game_book_page(game_port_game(p))==5);
 const GameController *controller=game_port_controller(p);SWLMatrix local;assert(!controller->bank_view(game_port_game(p),0,&local));assert(controller->bank_view(game_port_game(p),1,&local));
 assert(swl_visible(game_bank_scene(game_port_bank(p,1)),"root.page0",true));
 for(unsigned page=6;page<10;page++)game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 assert(game_book_page(game_port_game(p))==9);
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_RIGHT});
 game_port_update(p,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(f.zo==1);
 char *reply[]={"510"};assert(game_port_packet(p,"zo",1,reply));assert(f.left==1&&f.coins==510);
 game_port_free(p);

 Fixture f3;GamePort *q=open_book("book3",919,&f3);
 const BookData *book3=game_book_data(game_port_game(q));assert(book3&&!strcmp(book3->id,"book3")&&book3->pages==10&&book3->room==919);
 game_port_update(q,1.0f/24,&(GameInput){.down=GAME_KEY_A});
 assert(swl_visible(game_bank_scene(game_port_bank(q,0)),"root.page0",true));
 assert(render(game_bank_scene(game_port_bank(q,0)))>0);
 game_port_close(q);assert(f3.zo==0);tick(q,1);assert(f3.left==1);
 game_port_free(q);
 puts("PASS Paint By Letters native controller: book1, book2 and book3 banks render, page navigation crosses banks, finish pays 480 once, early exit leaves");
 return 0;
}
