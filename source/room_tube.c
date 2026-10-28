#include "room_tube.h"
#include <string.h>

static unsigned frame(const RoomTubeHooks *h,const char *path){return h&&h->frame?h->frame(h->user,path):1;}
static void target(const RoomTubeHooks *h,const char *path,unsigned value,const char *label,bool playing){if(h&&h->target)h->target(h->user,path,value,label,playing);}
bool room_tube_supported(unsigned room){return room==323||room==213;}
void room_tube_enter(RoomTube *t,unsigned room){memset(t,0,sizeof *t);t->room=room;}
bool room_tube_locked(const RoomTube *t){return t->active;}
unsigned room_tube_frame(const RoomTube *t){return t->active?t->frame:0;}
static void vr_visuals(const RoomTubeHooks *h){target(h,"ztube_mc",10,NULL,true);target(h,"background_mc.tube_mc",2,NULL,false);}
static void start_vr(RoomTube *t,const RoomTubeHooks *h){
 unsigned door=frame(h,"background_mc.door_mc");
 if(door==26){vr_visuals(h);t->frame=36;}
 else if(door>=1&&door<=25)target(h,"background_mc.door_mc",2,NULL,true);
 else if(door>=27&&door<=31){target(h,"background_mc.door_mc",26,NULL,false);if(frame(h,"background_mc.tube_mc.tubedoor_mc")!=9)target(h,"background_mc.tube_mc.tubedoor_mc",2,NULL,true);}
 else if(door>=32&&door<=39)target(h,"background_mc.door_mc",14,NULL,true);
 else if(door>=40&&door<=45)target(h,"background_mc.door_mc",10,NULL,true);
 else if(door>=46&&door<=54)target(h,"background_mc.door_mc",2,NULL,true);
 if(frame(h,"background_mc.door_mc.doorclosetimer_mc")==2)target(h,"background_mc.door_mc.doorclosetimer_mc",1,NULL,false);
}
bool room_tube_observe(RoomTube *t,unsigned player,unsigned mine,bool inside,unsigned colour,const RoomTubeHooks *h){
 if(!mine||player!=mine||!room_tube_supported(t->room))return false;
 if(!inside&&t->active){t->inside=false;room_tube_cancel(t,h);return false;}
 if(!inside)t->blocked=false;
 bool start=inside&&!t->inside&&!t->blocked&&!t->active;t->inside=inside;
 if(!start)return false;
 t->active=true;t->ready=t->taken=t->blur=false;t->colour=colour;t->frame=t->room==323?1:2;
 if(t->room==213)start_vr(t,h);
 return true;
}
static void blur_colour(RoomTube *t,const RoomTubeHooks *h){
 static const char *const labels[]={"blue","blue","green","pink","black","red","orange","yellowMustard","darkPurple","brown","peach","darkGreen","lightBlue","limeGreen","blue","aqua"};
 const char *label=labels[t->colour<16?t->colour:0];const char *root=t->room==323?"tubefront_mc":"ztube_mc";
 const char *const command[]={"tubefront_mc.blur1_mc","tubefront_mc.blur2_mc","tubefront_mc.blur3_mc"};
 const char *const vr[]={"ztube_mc.blur1_mc","ztube_mc.blur2_mc","ztube_mc.blur3_mc"};
 if(!t->blur&&frame(h,root)==(t->room==323?55u:65u)){
  for(unsigned i=0;i<3;i++)target(h,t->room==323?command[i]:vr[i],0,label,false);
  t->blur=true;
 }
}
void room_tube_tick(RoomTube *t,const RoomTubeHooks *h){
 if(!t->active||t->ready)return;
 blur_colour(t,h);t->frame++;
 if(t->room==323){
  if(t->frame==2){unsigned door=frame(h,"tubefront_mc.tubedoor_mc");if(door!=1&&door<100)target(h,"tubefront_mc.tubedoor_mc",100,NULL,true);}
  if(t->frame==10){target(h,"tubefront_mc",10,NULL,true);target(h,"background_mc.tubeback_mc",10,NULL,true);}
 }else if(t->frame==28)vr_visuals(h);
 if(t->frame==(t->room==323?75u:114u))t->ready=true;
}
void room_tube_cancel(RoomTube *t,const RoomTubeHooks *h){
 if(t->active){
  if(t->room==323){target(h,"tubefront_mc",1,NULL,false);target(h,"background_mc.tubeback_mc",1,NULL,false);target(h,"tubefront_mc.tubedoor_mc",1,NULL,false);}
  else{target(h,"ztube_mc",1,NULL,false);target(h,"background_mc.tube_mc",1,NULL,false);target(h,"background_mc.door_mc",1,NULL,false);}
 }
 t->active=t->ready=t->taken=t->blur=false;t->frame=0;t->blocked=t->inside;
}
bool room_tube_take(RoomTube *t,RoomTubeDestination *destination){
 if(!t->ready||t->taken||!destination)return false;
 *destination=t->room==323?(RoomTubeDestination){213,140,325}:(RoomTubeDestination){323,380,160};t->taken=true;return true;
}
