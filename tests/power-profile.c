#define POWER_PROFILE_TEST
#include "power_profile.h"
#include <assert.h>
#include <stdio.h>

static unsigned clocks,opens,closes,boosts,caps,last_cpu,last_cap;
static Result open_result,clock_result;
static bool speedup;
void osSetSpeedupEnable(bool enabled){boosts++;speedup=enabled;}
Result ptmSysmInit(void){opens++;return open_result;}
Result PTMSYSM_ConfigureNew3DSCPU(u8 flags){clocks++;last_cpu=flags;return clock_result;}
void ptmSysmExit(void){closes++;}
float C3D_FrameRate(float fps){caps++;last_cap=(unsigned)fps;return 60;}

int main(void){
 for(unsigned model=0;model<2;model++)for(unsigned saver=0;saver<2;saver++){
  PowerProfile p={0};PowerTarget t;
  for(unsigned ms=0;ms<6000;ms+=10){
   bool active=ms<1000||ms>=4000;
   t=power_profile_target(&p,ms,model,saver,active,false);
   bool fast=model&&!saver&&ms<2990;
   if(ms>=4000)fast=model&&!saver;
   assert(t.fps==(fast?60u:30u));assert(t.cpu==(model?(fast?3u:2u):0u));
   power_profile_apply(&p,t,model);
  }
  t=power_profile_target(&p,6010,model,saver,true,true);
  assert(t.fps==10);assert(t.cpu==(model?(saver?2u:3u):0u));
 }
 PowerProfile p={0};unsigned before=clocks,previous_caps=caps;
 assert(power_profile_apply(&p,(PowerTarget){30,2},true));
 assert(!speedup&&last_cpu==2&&last_cap==30&&clocks==before+1&&caps==previous_caps+1);
 for(unsigned i=0;i<1000;i++)assert(!power_profile_apply(&p,(PowerTarget){30,2},true));
 assert(clocks==before+1&&caps==previous_caps+1);
 assert(power_profile_apply(&p,(PowerTarget){60,3},true));assert(speedup&&last_cpu==3&&last_cap==60);
 p.applied=false;before=clocks;power_profile_apply(&p,(PowerTarget){60,3},true);assert(clocks==before+1);
 p.applied=false;open_result=-1;before=clocks;power_profile_apply(&p,(PowerTarget){30,2},true);
 assert(p.clock_result==-1&&clocks==before&&last_cap==30);
 open_result=0;clock_result=-2;p.applied=false;power_profile_apply(&p,(PowerTarget){30,2},true);assert(p.clock_result==-2);
 clock_result=0;p.applied=false;power_profile_apply(&p,(PowerTarget){30,2},true);assert(p.clock_result==0);
 assert(opens==closes+1&&boosts==opens);
 before=clocks;p=(PowerProfile){0};power_profile_apply(&p,(PowerTarget){30,0},false);assert(clocks==before);
 PowerProfile timer={0};power_profile_target(&timer,5000,true,false,true,false);
 PowerTarget t=power_profile_target(&timer,4000,true,false,false,false);assert(t.fps==60);
 t=power_profile_target(&timer,10000,true,false,false,false);assert(t.fps==60);
 puts("PASS power policy: Old/New x saver on/off, active/idle/input grace, 10 FPS foreground updates, L2 at normal clock, no repeated IPC, wake reapply, clock errors retain FPS cap, timer resets.");
 return 0;
}
