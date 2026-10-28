#include "newspaper.h"
#include <stddef.h>

static const struct {unsigned first;const char *label;} pages[NEWSPAPER_PAGES]={
 {1,"Main"},{11,"page02"},{19,"page03"},{27,"advice"},{35,"top secrets"},
 {43,"jokes"},{51,"artcomics"},{58,"puzzle"},{66,"events"},{76,"weNeedYou"}
};
unsigned newspaper_page_first(unsigned page){return page&&page<=NEWSPAPER_PAGES?pages[page-1].first:0;}
unsigned newspaper_page_last(unsigned page){return page&&page<NEWSPAPER_PAGES?pages[page].first-1:page==NEWSPAPER_PAGES?NEWSPAPER_FRAMES:0;}
const char *newspaper_page_label(unsigned page){return page&&page<=NEWSPAPER_PAGES?pages[page-1].label:NULL;}
void newspaper_open(Newspaper *paper){if(paper)*paper=(Newspaper){.open=true,.page=1,.frame=1};}
void newspaper_close(Newspaper *paper){if(paper)*paper=(Newspaper){0};}
bool newspaper_goto(Newspaper *paper,unsigned frame){
 if(!paper||!paper->open||!frame||frame>NEWSPAPER_FRAMES)return false;
 unsigned page=NEWSPAPER_PAGES;while(page>1&&frame<pages[page-1].first)page--;
 paper->page=page;paper->frame=frame;return true;
}
bool newspaper_page(Newspaper *paper,unsigned page){return newspaper_goto(paper,newspaper_page_first(page));}
unsigned newspaper_resource(const Newspaper *paper){return paper&&paper->open&&paper->page&&paper->page<=NEWSPAPER_PAGES?NEWSPAPER_RESOURCE_BASE+paper->page:0;}
const char *newspaper_notice(unsigned originalControl){
 switch(originalControl){
  case 296:case 534:case 538:case 540:case 542:return "Newspaper submissions are not open yet.";
  case 536:return "Fan art submissions are not open yet.";
  default:return NULL;
 }
}
