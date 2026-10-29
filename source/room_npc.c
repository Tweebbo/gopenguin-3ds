#include "game_features.h"
#ifdef GP_NINJA_GAMES
#include "room_npc.h"
#include "room_npc_data.h"
#include <stdio.h>
#include <string.h>
const RoomNpcSpec *room_npc_spec(unsigned room){for(unsigned i=0;i<ROOM_NPC_COUNT;i++)if(ROOM_NPC_SPECS[i].room==room)return &ROOM_NPC_SPECS[i];return NULL;}
void room_npc_free(RoomNpc *npc){island_free(npc->navigation);memset(npc,0,sizeof *npc);}
bool room_npc_enter(RoomNpc *npc,unsigned room,SWLScene *scene,const char *folder){
 room_npc_free(npc);const RoomNpcSpec *spec=room_npc_spec(room);if(!spec)return true;
 char file[256];if(!scene||!folder||snprintf(file,sizeof file,"%s/%s.nav",folder,spec->key)>=(int)sizeof file)return false;
 const char *error=NULL;IslandNav *nav=island_load(file,&error);
 if(!nav||!swl_target(scene,spec->path,spec->frame,NULL,false)){island_free(nav);return false;}
 npc->spec=spec;npc->navigation=nav;return true;
}
bool room_npc_click(RoomNpc *npc,int x,int y,float *tx,float *ty){
 IslandButton button;if(!npc->navigation||!island_button(npc->navigation,x,y,&button))return false;
 *tx=button.x;*ty=button.y;npc->approaching=true;return true;
}
bool room_npc_approaching(RoomNpc *npc,float x,float y,bool walking,bool cancelled){
 if(!npc->spec||cancelled||!walking){npc->approaching=false;return false;}
 float dx=x-npc->spec->x,dy=y-npc->spec->y;
 if(dx*dx+dy*dy<=4)npc->approaching=false;
 return npc->approaching;
}
const char *room_npc_near(const RoomNpc *npc,float x,float y){if(!npc->spec)return NULL;float dx=x-npc->spec->x,dy=y-npc->spec->y;return dx*dx+dy*dy<=70*70?npc->spec->game:NULL;}
const char *room_npc_route(unsigned room,const char *game){const RoomNpcSpec *spec=room_npc_spec(room);return spec&&game&&!strcmp(spec->game,game)?spec->controller:NULL;}
#endif
