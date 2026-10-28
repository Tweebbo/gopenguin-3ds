#pragma once
#include "penguin_net.h"
#define IGLOO_ITEMS 99
#define IGLOO_OWNED 2048
#define IGLOO_LAYOUTS 16
#define IGLOO_LAYOUT_TEXT 6144
typedef struct {unsigned id;int x,y;unsigned rotation,frame;} IglooFurniture;
typedef struct {unsigned owner,type,music,floor,count;IglooFurniture items[IGLOO_ITEMS];} IglooLayout;
typedef struct {unsigned id,amount;} IglooOwned;
typedef struct {unsigned id,location;bool locked;IglooLayout layout;} IglooSlot;
typedef struct {unsigned owner;char name[49];} OpenIgloo;
typedef struct {
 IglooLayout layout;IglooOwned owned[IGLOO_OWNED];unsigned nOwned,wantedOwner;
 unsigned types[128],nTypes;IglooSlot slots[IGLOO_LAYOUTS];unsigned nSlots,activeSlot;
 bool ready,ownedReady,editing,dirty,awaitingSave,saveVerified,saveFailed,awaitingTypes,activeKnown;
 bool typesReady,slotsReady,awaitingStyle,styleVerified,styleFailed,awaitingMusic,musicVerified,musicFailed;
 unsigned targetType,targetMusic;
 unsigned revision;char expected[IGLOO_LAYOUT_TEXT];
 OpenIgloo open[256];unsigned nOpen;bool openReady;
} Igloo;
bool igloo_request(Igloo *igloo,unsigned owner);
bool igloo_apply(Igloo *igloo,const PNetMessage *message);
bool igloo_serialize(const IglooLayout *layout,char *text,size_t capacity);
bool igloo_begin_edit(Igloo *igloo,unsigned player);
unsigned igloo_available(const Igloo *igloo,unsigned item);
bool igloo_add(Igloo *igloo,unsigned id,int x,int y);
bool igloo_remove(Igloo *igloo,unsigned index);
bool igloo_place(Igloo *igloo,unsigned index,int x,int y,unsigned rotation,unsigned frame);
bool igloo_save_prepare(Igloo *igloo);
bool igloo_styles_request(Igloo *igloo,unsigned player);
bool igloo_style_prepare(Igloo *igloo,unsigned player,unsigned type);
bool igloo_floor_purchase(Igloo *igloo,const PNetMessage *message,unsigned player);
bool igloo_music_prepare(Igloo *igloo,unsigned player,unsigned music);
