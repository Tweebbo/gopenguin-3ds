#pragma once
#include <stdbool.h>

#define LOBBY_ENTRIES 8u
#define LOBBY_SEATS 4u
#define LOBBY_NAME 64u
#define LOBBY_JOIN_TIMEOUT 10.0f
#define LOBBY_QUEUE_TIMEOUT 120.0f

typedef enum {LOBBY_TABLE,LOBBY_WADDLE} LobbyKind;
typedef enum {LOBBY_IDLE,LOBBY_JOINING,LOBBY_WAITING,LOBBY_READY,LOBBY_LAUNCHED,LOBBY_FAILED} LobbyState;
typedef bool (*LobbySend)(void *user,char extension,const char *command,unsigned count,const char *const *args);
typedef struct {
 unsigned id,capacity,count;bool known;
 char names[LOBBY_SEATS][LOBBY_NAME];
} LobbyEntry;
typedef struct {
 char key[16],username[LOBBY_NAME];
 LobbyKind kind;unsigned contextId;int seat;bool alreadyJoined;
} DescriptorLobbyLaunch;
typedef struct {
 LobbySend send;void *user;
 unsigned room,entryCount,selected;int seat;LobbyKind kind;LobbyState state;
 LobbyEntry entries[LOBBY_ENTRIES];
 char username[LOBBY_NAME];const char *error;
 float elapsed;bool needsLeave;
} Lobby;

void lobby_init(Lobby *lobby,LobbySend send,void *user);
/* Enter after the public room is confirmed. Cancel pending membership first. */
bool lobby_enter(Lobby *lobby,unsigned room,const char *username);
const char *lobby_key(unsigned room,LobbyKind kind,unsigned id);
const LobbyEntry *lobby_entry(const Lobby *lobby,unsigned id);
bool lobby_refresh(Lobby *lobby);
bool lobby_begin(Lobby *lobby,LobbyKind kind,unsigned id);
bool lobby_packet(Lobby *lobby,const char *command,unsigned count,char *const *args);
void lobby_tick(Lobby *lobby,float seconds);
/* Before handoff: s/lt for tables, z/lw for queues. Ready Sled already changed room. */
bool lobby_cancel(Lobby *lobby);
/* Copies once; the game host owns membership/return after successful handoff. */
bool lobby_take(Lobby *lobby,DescriptorLobbyLaunch *launch);
/* Call after the game host has finished its own leave/return operation. */
void lobby_release(Lobby *lobby);
