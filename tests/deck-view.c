/* Host test for the Starter Deck view: gcc -std=c11 -Wall -Wextra -Werror -Isource tests/deck-view.c source/deck_view.c (add -fsanitize=address,undefined where available) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "deck_view.h"
int main(void){
 static DeckView v;deck_view_reset(&v);
 /* The exact reply production sent to CheeseyHobb (penguin 5633): the 821 starter deck. */
 char a[]="1,1|6,1|9,1|14,1|17,1|20,1|22,1|23,1|26,1|81,1";char *args[]={a};
 assert(deck_view_parse(&v,1,args));
 assert(v.known&&v.total==10&&v.kinds==10);
 assert(v.fire+v.water+v.snow<=v.total);
 assert(v.cards[0].element=='f');
 for(unsigned i=1;i<v.kinds;i++){const char *r="fws";const char *p=strchr(r,v.cards[i-1].element),*q=strchr(r,v.cards[i].element);assert(p&&q&&p<=q);}
 for(unsigned i=0;i<v.kinds;i++)assert(v.cards[i].name&&*v.cards[i].name);
 assert(deck_view_pages(&v)==2);
 /* Duplicates and zero counts. */
 char b[]="1,3|6,0";char *bargs[]={b};assert(deck_view_parse(&v,1,bargs));assert(v.total==3&&v.kinds==1&&v.cards[0].count==3);
 /* Empty deck. */
 char c[]="";char *cargs[]={c};assert(deck_view_parse(&v,1,cargs));assert(v.known&&v.total==0&&deck_view_pages(&v)==1);
 /* Malformed input keeps the previous view. */
 assert(deck_view_parse(&v,1,args));char d[]="1;1";char *dargs[]={d};assert(!deck_view_parse(&v,1,dargs));assert(v.total==10);
 char e[]="1,99999";char *eargs[]={e};assert(!deck_view_parse(&v,1,eargs));
 /* Unknown ids get a placeholder name. */
 char f[]="99999,1";char *fargs[]={f};assert(deck_view_parse(&v,1,fargs));assert(!strcmp(v.cards[0].name,"CARD"));
 assert(!strcmp(deck_view_colour('p'),"Purple")&&!strcmp(deck_view_element('w'),"Water"));
 puts("PASS deck view: starter deck, duplicates, empty, malformed, unknown ids.");
 return 0;
}
