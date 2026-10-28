#pragma once
// Offline preview economy: without a server, catalogue purchases, inventory and the owner's igloo
// work against a local store on SD (sdmc:/3ds/gopenguin/offline.txt). It answers with the packets
// the server would send (ai/af/au/ag, gi, gf, gm) so the normal handlers apply them. Nothing here
// is ever sent to a server; online play always uses the server's balance and inventory.
#include <stdbool.h>
#include <stddef.h>

#define OFFLINE_OWNER 1u
#define OFFLINE_START_COINS 10000u
#define OFFLINE_ITEMS 2048
#define OFFLINE_FURNITURE 512
#define OFFLINE_IGLOOS 64
#define OFFLINE_LAYOUT 6144

typedef struct {unsigned id,count;} OfflineFurniture;
typedef struct {
 unsigned coins,igloo,floor,nItems,nFurniture,nIgloos;
 unsigned items[OFFLINE_ITEMS],igloos[OFFLINE_IGLOOS];
 OfflineFurniture furniture[OFFLINE_FURNITURE];
 char layout[OFFLINE_LAYOUT];
} OfflineShop;

/** Loads the store, or starts a fresh one (10000 coins, the basic igloo) if the file is missing. */
void offline_load(OfflineShop *shop,const char *path);
bool offline_save(const OfflineShop *shop,const char *path);
bool offline_has_item(const OfflineShop *shop,unsigned id);
bool offline_has_igloo(const OfflineShop *shop,unsigned id);
/** kind: 0 clothing, 1 furniture, 2 igloo, 3 flooring (as in client_data). Charges and records the
 *  purchase and writes the server's reply packet; false if it cannot be afforded or is owned. */
bool offline_buy(OfflineShop *shop,unsigned kind,unsigned id,unsigned cost,char *packet,size_t cap);
/** The server's inventory (gi), furniture (gf) and igloo (gm) packets for the local store. */
bool offline_inventory_packet(const OfflineShop *shop,char *packet,size_t cap);
bool offline_furniture_packet(const OfflineShop *shop,char *packet,size_t cap);
bool offline_igloo_packet(const OfflineShop *shop,char *packet,size_t cap);
/** Stores a saved layout (the `g#ur` furniture list joined with commas). */
bool offline_set_layout(OfflineShop *shop,const char *layout);
