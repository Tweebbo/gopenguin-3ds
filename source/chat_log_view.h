#pragma once
#include "chat_log.h"
#define CHAT_LOG_PAGE_ROWS 11u
#define CHAT_LOG_VIEW_ROWS (CHAT_LOG_CAPACITY*34u)
typedef struct {unsigned short entry,start,end;bool heading;} ChatLogRow;
typedef struct {
 bool open,follow,laidOut;
 unsigned offset,total,revision;
 ChatLogRow rows[CHAT_LOG_VIEW_ROWS];
} ChatLogView;
typedef struct {
 void (*box)(float,float,float,float,unsigned);
 void (*text)(float,float,unsigned,unsigned,const char *);
 float (*width)(unsigned,const char *);
 void (*button)(float,float,float,float,const char *,bool);
} ChatLogWidgets;
enum {CHAT_LOG_OLDER=1,CHAT_LOG_NEWER=2,CHAT_LOG_UP=4,CHAT_LOG_DOWN=8,CHAT_LOG_WRITE=16,CHAT_LOG_CLOSE=32};
void chat_log_view_open(ChatLogView *view);
unsigned chat_log_view_input(ChatLogView *view,unsigned keys);
unsigned chat_log_view_touch(ChatLogView *view,int x,int y);
void chat_log_view_draw(ChatLogView *view,const ChatLog *log,const ChatLogWidgets *widgets);
