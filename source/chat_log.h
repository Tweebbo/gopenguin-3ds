#pragma once
#include "common.h"

#define CHAT_LOG_CAPACITY 100u
#define CHAT_LOG_TEXT_BYTES 513u
typedef struct {
 unsigned penguinId,room;
 u64 time;
 char name[49],text[CHAT_LOG_TEXT_BYTES];
} ChatLogEntry;
typedef struct {
 ChatLogEntry entries[CHAT_LOG_CAPACITY];
 unsigned first,count,revision;
} ChatLog;

void chat_log_clear(ChatLog *log);
bool chat_log_append(ChatLog *log,unsigned penguinId,const char *name,const char *text,unsigned room,u64 time);
const ChatLogEntry *chat_log_get(const ChatLog *log,unsigned index);
