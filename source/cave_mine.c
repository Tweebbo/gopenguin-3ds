#include "cave_mine.h"
#define MACHINE "foreground_mc.lightmachine_mc."
#define BAR "background_mc.loadbar_mc"
// The original room's handleThrow: a snowball in the funnel recharges the light machine.
bool cave_mine_throw(const CaveMineHooks *h,float x,float y){
 if(x<=413||x>=513||y<=22||y>=58)return false;
 h->target(h->user,"background_mc.funnel_mc",2,true);
 if(h->frame(h->user,MACHINE "bigbulb_mc")==2){
  h->target(h->user,MACHINE "bigbulb_mc",1,false);h->target(h->user,MACHINE "boxtop_mc",1,false);
  h->target(h->user,"foreground_mc.darkeffect_mc",3,false);h->target(h->user,"foreground_mc.allbulbs_mc",3,false);
 }
 unsigned charge=h->frame(h->user,BAR);h->target(h->user,BAR,charge>60?charge-60:1,true);charge=h->frame(h->user,BAR);
 if(charge<40)h->target(h->user,MACHINE "smallbulb04_mc",1,false);
 if(charge<80)h->target(h->user,MACHINE "smallbulb03_mc",1,false);
 if(charge<120)h->target(h->user,MACHINE "smallbulb02_mc",1,false);
 if(charge<160)h->target(h->user,MACHINE "smallbulb01_mc",1,false);
 return true;
}
