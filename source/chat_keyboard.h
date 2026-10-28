#pragma once
#include "common.h"
#define CHAT_KEYBOARD_CHARS 120u
#define CHAT_KEYBOARD_BYTES (CHAT_KEYBOARD_CHARS*4+1)
typedef struct {
 bool active,shift,caps,symbols;
 unsigned row,column;
 int pressed;
 u64 repeat;
 char text[CHAT_KEYBOARD_BYTES],error[96];
 bool (*send)(const char *text,void *user);
 void *user;
} ChatKeyboard;
enum {CHAT_UP=1,CHAT_DOWN=2,CHAT_LEFT=4,CHAT_RIGHT=8,CHAT_ACCEPT=16,CHAT_DELETE=32,CHAT_SHIFT=64,CHAT_SPACE=128,CHAT_PAGE=256,CHAT_SEND=512,CHAT_CANCEL=1024};
typedef struct {
 void (*box)(float,float,float,float,unsigned);
 void (*text)(float,float,unsigned,unsigned,const char *);
 void (*center)(float,float,unsigned,unsigned,const char *);
 float (*width)(unsigned,const char *);
} ChatKeyboardWidgets;
void chat_keyboard_open(ChatKeyboard *keyboard,bool (*send)(const char *,void *),void *user);
void chat_keyboard_cancel(ChatKeyboard *keyboard);
bool chat_keyboard_input(ChatKeyboard *keyboard,unsigned down,unsigned held,u64 now);
void chat_keyboard_touch(ChatKeyboard *keyboard,int x,int y,bool pressed,bool released);
void chat_keyboard_draw(const ChatKeyboard *keyboard,u64 now,const ChatKeyboardWidgets *widgets);
