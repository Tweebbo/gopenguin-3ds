#pragma once
#include <stddef.h>
#include <stdint.h>
#include "lobby.h"

/* Generated from original room timelines by tools/export-lobby-geometry.mjs. */
/* public/swf/book/timelines.json SHA256 e23a514f13a7cfdca385a9996677390dd8ef57564a9031842e5cd24a4e669779 */
/* public/swf/lodge/timelines.json SHA256 a9505d8068fb42bc9b8385a7320e18a13eeef24da7f909894ff7ccd0c305bd48 */
/* public/swf/attic/timelines.json SHA256 34205b1bc0cc9d5570b659af1ec885e167298aeb47b8eb3adf5c181c1a821b9c */
/* public/swf/mtn/timelines.json SHA256 5705111f24869bbe47f83d26a019fecd17c42dbb9931d4a0cba70ef6b750907f */

typedef struct {
 int16_t x,y,doneX,doneY;
 uint8_t frame;
 const char *seatPath,*donePath;
} LobbyGeometrySeat;
typedef struct {
 uint16_t room,id;
 LobbyKind kind;
 uint8_t firstSeat,capacity;
 const char *path;
 const LobbyGeometrySeat *seats;
} LobbyGeometryEntry;

static const LobbyGeometrySeat lobbyGeometrySeats[]={
 {291,205,271,245,24,"table100_mc.seat1_mc","table100_mc.done1_mc"},
 {351,241,311,271,20,"table100_mc.seat2_mc","table100_mc.done2_mc"},
 {441,205,421,245,24,"table101_mc.seat1_mc","table101_mc.done1_mc"},
 {501,241,461,271,20,"table101_mc.seat2_mc","table101_mc.done2_mc"},
 {181,305,161,345,24,"table102_mc.seat1_mc","table102_mc.done1_mc"},
 {241,341,201,371,20,"table102_mc.seat2_mc","table102_mc.done2_mc"},
 {368,322,348,362,24,"table103_mc.seat1_mc","table103_mc.done1_mc"},
 {428,358,388,388,20,"table103_mc.seat2_mc","table103_mc.done2_mc"},
 {571,355,551,395,24,"table104_mc.seat1_mc","table104_mc.done1_mc"},
 {631,391,591,421,20,"table104_mc.seat2_mc","table104_mc.done2_mc"},
 {270,236,237,275,24,"table205_mc.seat1_mc","table205_mc.done1_mc"},
 {325,272,296,303,20,"table205_mc.seat2_mc","table205_mc.done2_mc"},
 {318,376,344,410,18,"table206_mc.seat1_mc","table206_mc.done1_mc"},
 {267,407,283,429,22,"table206_mc.seat2_mc","table206_mc.done2_mc"},
 {476,412,495,438,22,"table207_mc.seat1_mc","table207_mc.done1_mc"},
 {541,386,560,428,18,"table207_mc.seat2_mc","table207_mc.done2_mc"},
 {270,296,237,335,24,"table200_mc.seat1_mc","table200_mc.done1_mc"},
 {325,332,296,363,20,"table200_mc.seat2_mc","table200_mc.done2_mc"},
 {402,274,375,310,24,"table201_mc.seat1_mc","table201_mc.done1_mc"},
 {452,307,436,329,20,"table201_mc.seat2_mc","table201_mc.done2_mc"},
 {248,366,274,400,18,"table202_mc.seat1_mc","table202_mc.done1_mc"},
 {197,397,213,419,22,"table202_mc.seat2_mc","table202_mc.done2_mc"},
 {372,364,345,400,24,"table203_mc.seat1_mc","table203_mc.done1_mc"},
 {422,397,406,419,20,"table203_mc.seat2_mc","table203_mc.done2_mc"},
 {536,412,555,438,22,"table204_mc.seat1_mc","table204_mc.done1_mc"},
 {601,386,620,428,18,"table204_mc.seat2_mc","table204_mc.done2_mc"},
 {130,255,176,222,18,"waddle100_mc.seat0_mc","waddle100_mc.done0_mc"},
 {158,270,206,232,18,"waddle100_mc.seat1_mc","waddle100_mc.done1_mc"},
 {186,289,236,242,18,"waddle100_mc.seat2_mc","waddle100_mc.done2_mc"},
 {216,304,266,252,18,"waddle100_mc.seat3_mc","waddle100_mc.done3_mc"},
 {281,329,303,286,18,"waddle101_mc.seat0_mc","waddle101_mc.done0_mc"},
 {307,348,335,292,18,"waddle101_mc.seat1_mc","waddle101_mc.done1_mc"},
 {351,347,369,300,18,"waddle101_mc.seat2_mc","waddle101_mc.done2_mc"},
 {426,368,425,312,17,"waddle102_mc.seat0_mc","waddle102_mc.done0_mc"},
 {476,370,478,312,17,"waddle102_mc.seat1_mc","waddle102_mc.done1_mc"},
 {551,333,487,299,24,"waddle103_mc.seat0_mc","waddle103_mc.done0_mc"},
 {581,313,536,274,24,"waddle103_mc.seat1_mc","waddle103_mc.done1_mc"},
};
static const LobbyGeometryEntry lobbyGeometryEntries[]={
 {111,100,LOBBY_TABLE,1,2,"table100_mc",&lobbyGeometrySeats[0]},
 {111,101,LOBBY_TABLE,1,2,"table101_mc",&lobbyGeometrySeats[2]},
 {111,102,LOBBY_TABLE,1,2,"table102_mc",&lobbyGeometrySeats[4]},
 {111,103,LOBBY_TABLE,1,2,"table103_mc",&lobbyGeometrySeats[6]},
 {111,104,LOBBY_TABLE,1,2,"table104_mc",&lobbyGeometrySeats[8]},
 {220,205,LOBBY_TABLE,1,2,"table205_mc",&lobbyGeometrySeats[10]},
 {220,206,LOBBY_TABLE,1,2,"table206_mc",&lobbyGeometrySeats[12]},
 {220,207,LOBBY_TABLE,1,2,"table207_mc",&lobbyGeometrySeats[14]},
 {221,200,LOBBY_TABLE,1,2,"table200_mc",&lobbyGeometrySeats[16]},
 {221,201,LOBBY_TABLE,1,2,"table201_mc",&lobbyGeometrySeats[18]},
 {221,202,LOBBY_TABLE,1,2,"table202_mc",&lobbyGeometrySeats[20]},
 {221,203,LOBBY_TABLE,1,2,"table203_mc",&lobbyGeometrySeats[22]},
 {221,204,LOBBY_TABLE,1,2,"table204_mc",&lobbyGeometrySeats[24]},
 {230,100,LOBBY_WADDLE,0,4,"waddle100_mc",&lobbyGeometrySeats[26]},
 {230,101,LOBBY_WADDLE,0,3,"waddle101_mc",&lobbyGeometrySeats[30]},
 {230,102,LOBBY_WADDLE,0,2,"waddle102_mc",&lobbyGeometrySeats[33]},
 {230,103,LOBBY_WADDLE,0,2,"waddle103_mc",&lobbyGeometrySeats[35]},
};
#define LOBBY_GEOMETRY_ENTRIES 17u
#define LOBBY_GEOMETRY_SEATS 37u

static inline const LobbyGeometryEntry *lobby_geometry_entry(unsigned room,LobbyKind kind,unsigned id){
 for(unsigned i=0;i<LOBBY_GEOMETRY_ENTRIES;i++){
  const LobbyGeometryEntry *entry=&lobbyGeometryEntries[i];
  if(entry->room==room&&entry->kind==kind&&entry->id==id)return entry;
 }
 return NULL;
}
static inline const LobbyGeometrySeat *lobby_geometry_lookup(unsigned room,LobbyKind kind,unsigned id,int seat){
 const LobbyGeometryEntry *entry=lobby_geometry_entry(room,kind,id);
 if(!entry||seat<(int)entry->firstSeat||seat>=(int)(entry->firstSeat+entry->capacity))return NULL;
 return &entry->seats[seat-entry->firstSeat];
}
static inline unsigned lobby_geometry_table_frame(unsigned room,unsigned id,unsigned count){
 const LobbyGeometryEntry *entry=lobby_geometry_entry(room,LOBBY_TABLE,id);
 return entry&&count<=entry->capacity?count+1:0;
}
static inline const char *lobby_geometry_population_path(unsigned room,LobbyKind kind,unsigned id,int seat){
 const LobbyGeometryEntry *entry=lobby_geometry_entry(room,kind,id);
 if(!entry)return NULL;
 if(kind==LOBBY_TABLE)return entry->path;
 const LobbyGeometrySeat *point=lobby_geometry_lookup(room,kind,id,seat);
 return point?point->seatPath:NULL;
}
