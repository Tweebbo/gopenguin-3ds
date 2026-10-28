#include "epf_access.h"
#include "epf_access_data.h"
#include <math.h>
#include <string.h>
#define COUNT(a) (sizeof(a)/sizeof *(a))
static bool hit(const uint32_t *ends,unsigned count,int x,int y){
 if(x<0||x>=760||y<0||y>=480)return false;
 unsigned bit=(unsigned)(y*760+x),left=0,right=count;while(left<right){unsigned mid=left+(right-left)/2;if(ends[mid]<=bit)left=mid+1;else right=mid;}return left<count&&(left&1);
}
static void target(const EpfAccessHooks *h,const char *path,unsigned frame,bool playing){if(h&&h->target)h->target(h->user,path,frame,playing);}
static void reset(const EpfAccessHooks *h){
 const char *paths[]={"eventplanner_mc","background_mc.pillartr_mc","background_mc.basetr_mc","background_mc.pillartr_mc.epftube_mc.commandstates_mc","triggers_mc.command_mc","block_mc.pillar2_mc","background_mc.phonebooth_mc","foreground_mc.aimer_mc","foreground_mc.camhudtext_mc.namemask_mc","foreground_mc.camhudtext_mc.statusmask_mc"};
 for(unsigned i=0;i<COUNT(paths);i++)target(h,paths[i],1,false);
}
static void open_gate(EpfAccess *a,const EpfAccessHooks *h,bool shortcut){
 a->open=true;a->revision++;
 target(h,"background_mc.pillartr_mc",shortcut?30:9,!shortcut);target(h,"background_mc.basetr_mc",shortcut?20:5,!shortcut);
 target(h,"background_mc.pillartr_mc.epftube_mc.commandstates_mc",2,false);target(h,"triggers_mc.command_mc",2,false);target(h,"block_mc.pillar2_mc",2,false);
}
void epf_access_enter(EpfAccess *a,const IslandNav *navigation,unsigned room,float x,float y,bool ready,bool agent,const EpfAccessHooks *h){
 memset(a,0,sizeof *a);a->navigation=navigation;a->room=room;a->revision=1;a->roleReady=ready;a->agent=ready&&agent;
 if(room!=212)return;
 reset(h);target(h,"agentcheck_mc",a->agent?2:1,false);
 a->shortcut=hit(EPF_ENTRY_PILLAR,COUNT(EPF_ENTRY_PILLAR),(int)floorf(x+.5f),(int)floorf(y+.5f));
 a->frame=a->shortcut?200:1;a->running=a->shortcut||ready;
 if(a->shortcut){open_gate(a,h,true);target(h,"foreground_mc.camhudtext_mc.namemask_mc",30,false);target(h,"foreground_mc.camhudtext_mc.statusmask_mc",30,false);}
}
void epf_access_role(EpfAccess *a,bool ready,bool agent,const EpfAccessHooks *h){
 agent=ready&&agent;if(a->roleReady==ready&&a->agent==agent)return;
 a->roleReady=ready;a->agent=agent;if(a->room!=212)return;
 target(h,"agentcheck_mc",agent?2:1,false);if(a->shortcut)return;
 reset(h);a->open=false;a->frame=1;a->running=ready;a->revision++;
}
void epf_access_tick(EpfAccess *a,const EpfAccessHooks *h){
 if(a->room!=212||!a->running)return;
 a->frame++;
 switch(a->frame){
  case 5:target(h,"foreground_mc.aimer_mc",2,true);break;
  case 49:target(h,"foreground_mc.camhudtext_mc.namemask_mc",10,true);break;
  case 54:target(h,"foreground_mc.camhudtext_mc.statusmask_mc",10,true);break;
  case 70:if(!a->agent)a->frame=130;break;
  case 75:case 170:target(h,"foreground_mc.aimer_mc",67,true);if(a->frame==170)a->running=false;break;
  case 90:open_gate(a,h,false);break;
  case 101:target(h,"background_mc.phonebooth_mc",2,false);break;
  case 110:a->running=false;break;
  case 215:target(h,"background_mc.phonebooth_mc",2,false);a->running=false;break;
  default:break;
 }
}
bool epf_access_blocked(const void *user,int x,int y){const EpfAccess *a=user;if(!a||!a->navigation)return false;return a->room==212&&a->open?hit(EPF_OPEN_BLOCK,COUNT(EPF_OPEN_BLOCK),x,y):island_blocked(a->navigation,x,y);}
bool epf_access_trigger(const EpfAccess *a,int x,int y,IslandTrigger *trigger){
 if(!a||!a->navigation)return false;
 if(a->room==212&&a->open&&hit(EPF_OPEN_COMMAND,COUNT(EPF_OPEN_COMMAND),x,y)){*trigger=(IslandTrigger){.room=323,.x=565,.y=150,.otherwise="",.framePath="",.index=EPF_COMMAND_TRIGGER};return true;}
 return island_trigger(a->navigation,x,y,trigger);
}
bool epf_access_button(const EpfAccess *a,int x,int y,IslandButton *button){
 if(!a||!a->navigation)return false;
 if(a->room==212&&a->open&&hit(EPF_OPEN_BUTTON,COUNT(EPF_OPEN_BUTTON),x,y)){*button=(IslandButton){.path="background_mc.pillartr_mc.epftube_mc.commandstates_mc.command01_btn",.content="",.x=535,.y=227,.index=EPF_COMMAND_BUTTON};return true;}
 return island_button(a->navigation,x,y,button);
}
