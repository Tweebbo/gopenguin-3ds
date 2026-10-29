#include "game_features.h"
#ifdef GP_NINJA_GAMES
#include "game_card_preview.h"
#include "game_card_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint32_t COLOURS[]={1132705,6404422,16225579,10721738,14826534,16509741};
static uint32_t card_colour(char colour){const char *c=strchr("bgopry",colour);return COLOURS[c&&*c?(unsigned)(c-"bgopry"):0];}
void card_preview_stats(GamePort *port,const CardPreview *preview,int value,char colour,bool blocked){
 if(!preview->id)return;
 SWLScene *scene=game_bank_scene(game_port_bank(port,2));if(!scene)return;
 uint32_t rgb=card_colour(colour);swl_color(scene,"root.colour",(SWLColor){{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}});
 char text[16];snprintf(text,sizeof text,"%d",value);size_t length=strlen(text);
 for(unsigned i=0;i<3;i++){char path[32];snprintf(path,sizeof path,"root.digit%u",i);unsigned frame=i>=length?12:text[i]=='-'?11:(unsigned)(text[i]-'0')+1;swl_target(scene,path,frame,NULL,false);}
 swl_target(scene,"root.status",blocked?2:1,NULL,false);
}
void card_preview_hand(GamePort *port,unsigned slot,int value,char element,char colour,bool focused,bool blocked){
 float x=5+63*slot;uint32_t border=focused?0x003366:0xffffff;
 if(focused){game_port_rect(port,x-1,55,60,2,border);game_port_rect(port,x-1,55,2,66,border);game_port_rect(port,x+57,55,2,66,border);}
 game_port_rect(port,x,117,58,4,card_colour(colour));game_port_rect(port,x+2,60,25,25,0xffffff);
 char text[16];snprintf(text,sizeof text,"%d",value);game_port_text(port,x+14,61,15,0x003366,GAME_ALIGN_CENTER,text);
 game_port_rect(port,x-1,122,60,17,0xdaf2ff);game_port_text(port,x+29,124,11,blocked?0xa01922:0x003366,GAME_ALIGN_CENTER,blocked?"Blocked":element=='f'?"Fire":element=='w'?"Water":"Snow");
}
bool card_preview_resource_name(const char *name){
 static const char prefix[]=GP_CARD_PREVIEW_PREFIX;
 if(!name||strncmp(name,prefix,sizeof prefix-1))return false;
 const char *start=name+sizeof prefix-1;if(*start<'1'||*start>'9')return false;
 char *end;unsigned long id=strtoul(start,&end,10);if(strcmp(end,".gpk"))return false;
 for(unsigned i=0;i<CARD_ICON_COUNT;i++)if(CARD_ICON_IDS[i]==id)return true;
 return false;
}
void card_preview_update(GamePort *port,CardPreview *preview,unsigned id){
 if(preview->id==id)return;
 game_port_release_bank(port,2);preview->id=0;
 if(!id)return;
 snprintf(preview->resource,sizeof preview->resource,GP_CARD_PREVIEW_PREFIX "%u.gpk",id);
 if(game_port_require_bank(port,2)==GAME_RESOURCE_READY)preview->id=id;
}
#endif
