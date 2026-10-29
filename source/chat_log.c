#include "chat_log.h"
#include <string.h>

static void copy_text(char *out,size_t capacity,const char *source){
 size_t used=0;
 if(!source)source="";
 for(const unsigned char *p=(const unsigned char *)source;*p;){
  unsigned bytes=*p<0x80?1:*p>=0xc2&&*p<=0xdf?2:*p>=0xe0&&*p<=0xef?3:*p>=0xf0&&*p<=0xf4?4:0;
  bool valid=bytes!=0;
  for(unsigned i=1;valid&&i<bytes;i++)if(!p[i]||(p[i]&0xc0)!=0x80)valid=false;
  if(valid&&bytes==3&&((p[0]==0xe0&&p[1]<0xa0)||(p[0]==0xed&&p[1]>=0xa0)))valid=false;
  if(valid&&bytes==4&&((p[0]==0xf0&&p[1]<0x90)||(p[0]==0xf4&&p[1]>=0x90)))valid=false;
  if(!valid){if(used+1>=capacity)break;out[used++]='?';p++;continue;}
  if(used+bytes>=capacity)break;
  if(bytes==1&&(*p<32||*p==127||*p=='|'))out[used++]=' ';
  else{memcpy(out+used,p,bytes);used+=bytes;}
  p+=bytes;
 }
 out[used]=0;
}

void chat_log_clear(ChatLog *log){memset(log,0,sizeof *log);}

bool chat_log_append(ChatLog *log,unsigned penguinId,const char *name,const char *text,unsigned room,u64 time){
 if(!penguinId||!text||!text[0])return false;
 unsigned index=(log->first+log->count)%CHAT_LOG_CAPACITY;
 ChatLogEntry *entry=&log->entries[index];
 copy_text(entry->name,sizeof entry->name,name);
 copy_text(entry->text,sizeof entry->text,text);
 if(!entry->text[0])return false;
 entry->penguinId=penguinId;entry->room=room;entry->time=time;
 if(log->count<CHAT_LOG_CAPACITY)log->count++;
 else log->first=(log->first+1)%CHAT_LOG_CAPACITY;
 log->revision++;
 return true;
}

const ChatLogEntry *chat_log_get(const ChatLog *log,unsigned index){
 return index<log->count?&log->entries[(log->first+index)%CHAT_LOG_CAPACITY]:NULL;
}
