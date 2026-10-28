#pragma once
#include "penguin_net.h"
#define INVENTORY_LIMIT 8192
typedef struct {unsigned coins,age,stamps,inventory[INVENTORY_LIMIT],count,revision,inventoryRevision,epfRevision;bool ready,epfReady,epfAgent;char error[120];} GPProfile;
void profile_init(GPProfile *profile,bool offline);
void profile_apply(GPProfile *profile,const PNetMessage *message,unsigned mine);
bool profile_has(const GPProfile *profile,unsigned id);
void profile_epf_reset(GPProfile *profile);
