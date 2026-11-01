#include "chat_keyboard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned widthCalls,sends;
static bool acceptSend;
static char sent[CHAT_KEYBOARD_BYTES];
static unsigned count(const char *s){unsigned n=0;for(const unsigned char *p=(const unsigned char *)s;*p;p++)if((*p&0xc0)!=0x80)n++;return n;}
static float width(unsigned size,const char *s){(void)size;widthCalls++;return count(s)*6;}
static float wide(unsigned size,const char *s){(void)size;widthCalls++;return count(s)*30;}
static void box(float x,float y,float w,float h,unsigned color){(void)x;(void)y;(void)w;(void)h;(void)color;}
static void text(float x,float y,unsigned size,unsigned color,const char *s){(void)x;(void)y;(void)size;(void)color;(void)s;}
static bool send_text(const char *s,void *user){(void)user;sends++;snprintf(sent,sizeof sent,"%s",s);return acceptSend;}
static void tap(ChatKeyboard *k,int x,int y){chat_keyboard_touch(k,x,y,true,false);chat_keyboard_touch(k,x,y,false,true);}

int main(void){
 ChatKeyboard k;chat_keyboard_open(&k,send_text,NULL);
 chat_keyboard_touch(&k,16,110,true,false);assert(!strcmp(k.text,"q"));
 chat_keyboard_touch(&k,16,110,false,false);assert(!strcmp(k.text,"q"));
 chat_keyboard_touch(&k,48,110,false,true);assert(!strcmp(k.text,"q"));
 tap(&k,48,110);assert(!strcmp(k.text,"qw"));
 tap(&k,24,202);assert(k.shift&&!k.caps);tap(&k,16,110);assert(!strcmp(k.text,"qwQ")&&!k.shift);
 tap(&k,152,202);assert(!strcmp(k.text,"qwQ "));tap(&k,240,202);assert(!strcmp(k.text,"qwQ"));
 chat_keyboard_touch(&k,292,202,true,false);assert(!sends&&k.active&&k.row==4&&k.column==4);
 chat_keyboard_touch(&k,200,230,false,true);assert(!sends&&k.active);
 tap(&k,292,202);assert(sends==1&&k.active&&k.error[0]&&!strcmp(sent,"qwQ"));
 acceptSend=true;tap(&k,292,202);assert(sends==2&&!k.active);
 chat_keyboard_open(&k,send_text,NULL);chat_keyboard_touch(&k,303,10,true,false);assert(k.active);
 chat_keyboard_touch(&k,0,230,false,true);assert(k.active);tap(&k,303,10);assert(!k.active);

 chat_keyboard_open(&k,send_text,NULL);
 for(unsigned i=0;i<CHAT_KEYBOARD_CHARS;i++)chat_keyboard_input(&k,CHAT_ACCEPT,0,i);
 assert(strlen(k.text)==120);
 ChatKeyboardWidgets widgets={box,text,text,width};
 chat_keyboard_draw(&k,0,&widgets);unsigned measured=widthCalls;
 assert(measured<=25&&k.lastLine==1&&strlen(k.lines[0])==48&&strlen(k.lines[1])==24&&k.lastWidth==144);
 for(unsigned frame=0;frame<250;frame++)chat_keyboard_draw(&k,frame*33,&widgets);
 assert(widthCalls==measured);
 chat_keyboard_input(&k,CHAT_ACCEPT,0,0);chat_keyboard_draw(&k,500,&widgets);assert(widthCalls==measured);
 chat_keyboard_input(&k,CHAT_DELETE,CHAT_DELETE,1000);assert(strlen(k.text)==119);
 chat_keyboard_input(&k,0,CHAT_DELETE,1449);assert(strlen(k.text)==119);
 chat_keyboard_input(&k,0,CHAT_DELETE,1450);assert(strlen(k.text)==118);
 chat_keyboard_draw(&k,1500,&widgets);assert(widthCalls>measured);

 chat_keyboard_open(&k,send_text,NULL);chat_keyboard_input(&k,CHAT_PAGE,0,0);
 for(unsigned i=0;i<CHAT_KEYBOARD_CHARS;i++)chat_keyboard_input(&k,CHAT_ACCEPT,0,i);
 assert(count(k.text)==120&&strlen(k.text)==240);
 chat_keyboard_draw(&k,0,&widgets);assert(count(k.lines[0])==48&&count(k.lines[1])==24);
 widgets.width=wide;chat_keyboard_draw(&k,0,&widgets);
 assert(count(k.lines[0])==9&&count(k.lines[1])==3&&k.lastWidth==90);
 chat_keyboard_input(&k,CHAT_DELETE,CHAT_DELETE,0);assert(count(k.text)==119&&strlen(k.text)==238);
 puts("PASS keyboard: letters/edit keys on touch-down, no release duplicate, guarded Send/Close, send failure retention, hold-delete timing, UTF-8 wrapping, 120-character limit and zero idle layout measurements.");
 printf("120-character layout: %u measurements after edit, 0 more across 250 idle frames.\n",measured);
}
