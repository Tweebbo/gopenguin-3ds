#pragma once
#include "world_state.h"
#define SOCIAL_LIMIT 100
#define SOCIAL_PLAYERS 256
typedef struct {unsigned id;char name[49];bool online,pending;} Buddy;
typedef struct {unsigned senderId,postcard,uid;char sender[49],details[256],timestamp[32];bool read;} Mail;
typedef struct {Buddy friends[SOCIAL_LIMIT],ignored[SOCIAL_LIMIT],requests[SOCIAL_LIMIT];unsigned nFriends,nIgnored,nRequests;Actor players[SOCIAL_PLAYERS];unsigned nPlayers;Mail mail[SOCIAL_LIMIT];unsigned nMail,unread,revision,foundRoom;} Social;
bool social_ignored(const Social *social,unsigned id);
const Buddy *social_friend(const Social *social,unsigned id);
Actor *social_player(Social *social,unsigned id);
void social_apply(Social *social,const PNetMessage *message);
void social_dismiss(Social *social,unsigned id);
