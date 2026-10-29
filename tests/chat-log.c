#include "chat_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
 static ChatLog log;
 assert(!chat_log_get(&log,0));
 assert(!chat_log_append(&log,0,"Nobody","Ignored",100,0));
 assert(!chat_log_append(&log,1,"Penguin","",100,0));
 assert(chat_log_append(&log,42,"Gold0","Hello ñ ♪ 😀|second line\nthird",100,123));
 const ChatLogEntry *entry=chat_log_get(&log,0);
 assert(entry&&entry->penguinId==42&&entry->room==100&&entry->time==123);
 assert(!strcmp(entry->name,"Gold0"));assert(!strcmp(entry->text,"Hello ñ ♪ 😀 second line third"));
 assert(chat_log_append(&log,43,"Other","Hello",101,124));
 assert(chat_log_append(&log,43,"Other","Hello",101,125));
 assert(log.count==3);
 char longText[600];memset(longText,'x',511);memcpy(longText+511,"ñ",3);
 assert(chat_log_append(&log,1,"Long",longText,100,200));
 assert(strlen(chat_log_get(&log,3)->text)==511);
 assert(chat_log_append(&log,1,"Invalid","A\xf0\x80\x80\x80" "B",100,201));
 assert(!strcmp(chat_log_get(&log,4)->text,"A????B"));
 chat_log_clear(&log);
 for(unsigned i=0;i<CHAT_LOG_CAPACITY+25;i++)assert(chat_log_append(&log,i+1,"Penguin","Message",100+i,i));
 assert(log.count==CHAT_LOG_CAPACITY);
 for(unsigned i=0;i<CHAT_LOG_CAPACITY;i++){entry=chat_log_get(&log,i);assert(entry->penguinId==i+26&&entry->room==i+125&&entry->time==i+25);}
 assert(!chat_log_get(&log,CHAT_LOG_CAPACITY));
 chat_log_clear(&log);assert(!log.count&&!log.first&&!chat_log_get(&log,0));
 puts("PASS chat log: chronological ring, fixed capacity, names/room/time, repeated messages, UTF-8 boundaries, malformed text and clear");
}
