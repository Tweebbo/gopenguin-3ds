#include "deck_view.h"
#include <stdlib.h>
#include <string.h>
#include "game_card_catalog.h"

void deck_view_reset(DeckView *view){memset(view,0,sizeof *view);}

static const CardInfo *lookup(unsigned id){
 unsigned lo=0,hi=CARD_CATALOG_COUNT;
 while(lo<hi){unsigned mid=(lo+hi)/2;if(CARD_CATALOG[mid].id==id)return &CARD_CATALOG[mid];if(CARD_CATALOG[mid].id<id)lo=mid+1;else hi=mid;}
 for(unsigned i=0;i<CARD_CATALOG_COUNT;i++)if(CARD_CATALOG[i].id==id)return &CARD_CATALOG[i];
 return NULL;
}

static int order(const void *a,const void *b){
 const DeckCard *x=a,*y=b;static const char rank[]="fws";
 const char *ex=strchr(rank,x->element),*ey=strchr(rank,y->element);
 int ix=ex&&x->element?(int)(ex-rank):3,iy=ey&&y->element?(int)(ey-rank):3;
 if(ix!=iy)return ix-iy;
 if(x->value!=y->value)return x->value>y->value?-1:1;
 return x->id<y->id?-1:x->id>y->id;
}

bool deck_view_parse(DeckView *view,unsigned count,char *const *args){
 static DeckView next;deck_view_reset(&next); /* static: 24 KB is too much for the 3DS main stack */
 for(unsigned i=0;i<count;i++){
  const char *p=args[i];
  while(*p){
   char *end;unsigned long id=strtoul(p,&end,10);if(end==p||*end!=',')return false;p=end+1;
   unsigned long n=strtoul(p,&end,10);if(end==p||n>10000||(*end&&*end!='|'))return false;p=*end?end+1:end;
   if(!n)continue;
   if(next.kinds>=DECK_VIEW_LIMIT)return false;
   const CardInfo *info=lookup((unsigned)id);
   DeckCard *c=&next.cards[next.kinds++];
   *c=(DeckCard){.id=(unsigned)id,.count=(unsigned)n,.value=info?info->value:0,.power=info?info->power:0,.element=info?info->element:0,.colour=info?info->colour:0,.name=info?info->name:"CARD"};
   next.total+=(unsigned)n;
   if(c->element=='f')next.fire+=(unsigned)n;else if(c->element=='w')next.water+=(unsigned)n;else if(c->element=='s')next.snow+=(unsigned)n;
  }
 }
 qsort(next.cards,next.kinds,sizeof next.cards[0],order);
 next.known=true;*view=next;return true;
}

unsigned deck_view_pages(const DeckView *view){return view->kinds?(view->kinds+DECK_VIEW_ROWS-1)/DECK_VIEW_ROWS:1;}
const char *deck_view_element(char e){return e=='f'?"Fire":e=='w'?"Water":e=='s'?"Snow":"";}
const char *deck_view_colour(char c){
 switch(c){case 'b':return "Blue";case 'g':return "Green";case 'o':return "Orange";case 'p':return "Purple";case 'r':return "Red";case 'y':return "Yellow";default:return "";}
}
