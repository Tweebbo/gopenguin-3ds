#pragma once
#include <stdbool.h>
#include <stddef.h>
#define CARD_HAND 5u
#define CARD_HISTORY 512u
typedef enum {CARD_MENU,CARD_QUEUE,CARD_HANDSHAKE,CARD_DEALING,CARD_SELECT,CARD_PICK_WAIT,CARD_REVEAL,CARD_OVER,CARD_ERROR} CardPhase;
typedef struct {unsigned session,type,value,power,seat;char element,colour;bool held,scored;} CardItem;
typedef struct {
 CardPhase phase;int seat,winner,matchWinner;
 unsigned mode,round,count,hand[2][CARD_HAND],hands[2],picked[2],rank[2],colour[2],newRank;
 CardItem cards[CARD_HISTORY];
 char names[2][40],blocked[2],nextBlock[2];
 int modifier[2],nextModifier[2];bool swap,nextSwap;
 bool menuReady,requestGet,requestSeats,requestDeal,started,choicePending,rankAwarded,stampInfo;
 unsigned dealAmount,powerId,stampCount,stampTotal;
 float elapsed,revealTime;char message[160];
} CardModel;
void card_model_init(CardModel *model);
void card_model_begin(CardModel *model,bool sensei);
bool card_model_packet(CardModel *model,const char *command,unsigned count,char *const *args);
bool card_model_choose(CardModel *model,unsigned slot,unsigned *session);
void card_model_choice_failed(CardModel *model);
void card_model_next_round(CardModel *model);
const CardItem *card_model_find(const CardModel *model,unsigned session);
bool card_model_playable(const CardModel *model,unsigned slot);
unsigned card_model_score(const CardModel *model,unsigned seat,char element);
