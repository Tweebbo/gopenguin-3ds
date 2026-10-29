#include "chat_log_view.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned draws,buttons;
static float width(unsigned size,const char *text){(void)size;unsigned count=0;for(;*text;text++)if(((unsigned char)*text&0xc0)!=0x80)count++;return count*6;}
static void box(float x,float y,float w,float h,unsigned colour){(void)colour;assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);}
static void text(float x,float y,unsigned size,unsigned colour,const char *value){(void)colour;assert(x>=0&&y>=0&&x+width(size,value)<=320&&y+size<=240);draws++;}
static void button(float x,float y,float w,float h,const char *label,bool enabled){(void)enabled;assert(width(10,label)<=w);box(x,y,w,h,0);buttons++;}
int main(void){
 static ChatLog log;static ChatLogView view;const ChatLogWidgets widgets={box,text,width,button};
 chat_log_view_open(&view);chat_log_view_draw(&view,&log,&widgets);assert(view.open&&view.total==0&&buttons==4);
 for(unsigned i=0;i<30;i++)assert(chat_log_append(&log,i+1,"Penguin","A long room message with words to wrap and ñ ♪ 😀. More words make this span several lines without losing text.",100,60000*i));
 chat_log_view_draw(&view,&log,&widgets);assert(view.offset>0&&view.total>CHAT_LOG_PAGE_ROWS&&view.follow);
 unsigned total=view.total,latest=view.offset;
 chat_log_view_input(&view,CHAT_LOG_OLDER);assert(view.offset<latest&&!view.follow);unsigned earlier=view.offset;
 chat_log_append(&log,55,"Other","A new message while reading older chat",200,99);chat_log_view_draw(&view,&log,&widgets);assert(view.offset==earlier&&!view.follow&&view.total>total);
 while(view.offset)chat_log_view_input(&view,CHAT_LOG_OLDER);
 assert(view.offset==0);
 chat_log_view_input(&view,CHAT_LOG_UP);assert(view.offset==0);
 while(!view.follow)chat_log_view_input(&view,CHAT_LOG_NEWER);
 assert(view.offset==view.total-CHAT_LOG_PAGE_ROWS);
 assert(chat_log_view_touch(&view,200,190)==CHAT_LOG_WRITE&&!view.open);
 chat_log_view_open(&view);assert(chat_log_view_input(&view,CHAT_LOG_CLOSE)==CHAT_LOG_CLOSE&&!view.open);
 assert(draws>10);puts("PASS history view: bounded rendering, empty state, UTF-8 wrapping, L/R paging, scroll bounds, stable reading, follow latest, compose and close");
}
