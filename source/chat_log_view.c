#include "chat_log_view.h"
#include <stdio.h>
#include <string.h>

static unsigned character_bytes(const char *p){unsigned char c=(unsigned char)*p;return c<0x80?1:c<0xe0?2:c<0xf0?3:4;}
static unsigned last_page(const ChatLogView *v){return v->total>CHAT_LOG_PAGE_ROWS?v->total-CHAT_LOG_PAGE_ROWS:0;}
static void row(ChatLogView *v,unsigned entry,unsigned start,unsigned end,bool heading){
 if(v->total<CHAT_LOG_VIEW_ROWS)v->rows[v->total++]=(ChatLogRow){(unsigned short)entry,(unsigned short)start,(unsigned short)end,heading};
}
static void layout(ChatLogView *v,const ChatLog *log,const ChatLogWidgets *w){
 v->total=0;
 for(unsigned i=0;i<log->count;i++){
  const ChatLogEntry *entry=chat_log_get(log,i);row(v,i,0,0,true);
  const char *text=entry->text;unsigned start=0;
  while(text[start]){
   unsigned end=start,space=start;float width=0;
   while(text[end]){
    unsigned bytes=character_bytes(text+end);char glyph[5]={0};memcpy(glyph,text+end,bytes);float next=w->width(10,glyph);
    if(width+next>286&&end>start)break;
    width+=next;if(text[end]==' ')space=end;end+=bytes;
   }
   if(text[end]&&space>start)end=space;
   row(v,i,start,end,false);start=end;while(text[start]==' ')start++;
  }
 }
 v->revision=log->revision;v->laidOut=true;
 if(v->follow||v->offset>last_page(v))v->offset=last_page(v);
}
void chat_log_view_open(ChatLogView *v){v->open=true;v->follow=true;v->laidOut=false;}
unsigned chat_log_view_input(ChatLogView *v,unsigned keys){
 if(keys&CHAT_LOG_CLOSE){v->open=false;return CHAT_LOG_CLOSE;}
 if(keys&CHAT_LOG_WRITE){v->open=false;return CHAT_LOG_WRITE;}
 if(keys&(CHAT_LOG_OLDER|CHAT_LOG_UP)){unsigned step=keys&CHAT_LOG_OLDER?CHAT_LOG_PAGE_ROWS:1;v->offset=v->offset>step?v->offset-step:0;v->follow=false;}
 if(keys&(CHAT_LOG_NEWER|CHAT_LOG_DOWN)){unsigned step=keys&CHAT_LOG_NEWER?CHAT_LOG_PAGE_ROWS:1,latest=last_page(v);v->offset=v->offset+step<latest?v->offset+step:latest;v->follow=v->offset==latest;}
 return 0;
}
unsigned chat_log_view_touch(ChatLogView *v,int x,int y){
 if(y<180||y>=204||x<8||x>=312)return 0;
 return chat_log_view_input(v,x<82?CHAT_LOG_OLDER:x<160?CHAT_LOG_NEWER:x<238?CHAT_LOG_WRITE:CHAT_LOG_CLOSE);
}
void chat_log_view_draw(ChatLogView *v,const ChatLog *log,const ChatLogWidgets *w){
 if(!v->laidOut||v->revision!=log->revision)layout(v,log,w);
 char label[96];snprintf(label,sizeof label,"%u messages / last %u",log->count,CHAT_LOG_CAPACITY);w->text(12,33,10,0x003366,label);
 w->box(8,49,304,126,0xffffff);
 if(!log->count){w->text(16,75,12,0x003366,"No messages yet.");w->text(16,98,10,0x003366,"Chat from the room appears here.");}
 for(unsigned n=0;n<CHAT_LOG_PAGE_ROWS&&v->offset+n<v->total;n++){
  const ChatLogRow *r=&v->rows[v->offset+n];const ChatLogEntry *entry=chat_log_get(log,r->entry);char text[CHAT_LOG_TEXT_BYTES];
  if(r->heading){unsigned minutes=(unsigned)(entry->time/60000%1440);snprintf(text,sizeof text,"%02u:%02u  %s  (#%u)",minutes/60,minutes%60,entry->name,entry->room);
   while(w->width(10,text)>286){size_t length=strlen(text);if(!length)break;do{length--;}while(length&&((unsigned char)text[length]&0xc0)==0x80);text[length]=0;}
  }else{unsigned length=r->end-r->start;memcpy(text,entry->text+r->start,length);text[length]=0;}
  w->text(16,52+n*11,10,r->heading?0x07508c:0x003366,text);
 }
 w->button(8,180,70,24,"L Older",v->offset>0);w->button(86,180,70,24,"R Newer",v->offset<last_page(v));
 w->button(164,180,70,24,"A Write",true);w->button(242,180,70,24,"B Back",true);
 snprintf(label,sizeof label,"Lines %u-%u / %u",v->total?v->offset+1:0,v->offset+CHAT_LOG_PAGE_ROWS<v->total?v->offset+CHAT_LOG_PAGE_ROWS:v->total,v->total);w->text(12,212,10,0x003366,label);
 w->text(12,226,9,0x003366,"Up / Down: scroll. History clears when you exit.");
}
