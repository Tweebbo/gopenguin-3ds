#pragma once
#include "client_data.h"
#include "profile.h"
#include "demo.h"
typedef struct {
 void (*box)(float,float,float,float,unsigned);
 void (*text)(float,float,unsigned,unsigned,const char *);
 void (*center)(float,float,unsigned,unsigned,const char *);
 void (*portrait)(Actor *);
 int (*portraitHit)(const Actor *,int,int);
 bool (*keyboard)(char *,size_t,const char *,bool);
 void (*notice)(const char *);
 unsigned (*colour)(unsigned);
 void (*rename)(void);
} InventoryWidgets;
typedef struct {unsigned filtered[INVENTORY_LIMIT],count,page,filter,stamp;bool filters;char query[41],selected[64];} InventoryUI;
void inventory_prepare(InventoryUI *ui,const ClientData *data,const GPProfile *profile,const Actor *actor);
void inventory_draw(InventoryUI *ui,const ClientData *data,const GPProfile *profile,Actor *actor,const InventoryWidgets *widgets);
void inventory_touch(InventoryUI *ui,const ClientData *data,const GPProfile *profile,Actor *actor,const InventoryWidgets *widgets,int x,int y);
