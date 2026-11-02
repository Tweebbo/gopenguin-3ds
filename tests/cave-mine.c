#include "../source/cave_mine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {const char *path;unsigned frame;} parts[]={{"foreground_mc.lightmachine_mc.bigbulb_mc",1},{"foreground_mc.lightmachine_mc.boxtop_mc",1},{"foreground_mc.darkeffect_mc",1},{"foreground_mc.allbulbs_mc",1},{"background_mc.loadbar_mc",1},{"background_mc.funnel_mc",1},{"foreground_mc.lightmachine_mc.smallbulb01_mc",2},{"foreground_mc.lightmachine_mc.smallbulb02_mc",2},{"foreground_mc.lightmachine_mc.smallbulb03_mc",2},{"foreground_mc.lightmachine_mc.smallbulb04_mc",2}};
static unsigned *part(const char *path){for(unsigned i=0;i<sizeof parts/sizeof *parts;i++)if(!strcmp(parts[i].path,path))return &parts[i].frame;assert(!"unknown path");return NULL;}
static unsigned frame(void *u,const char *path){(void)u;return *part(path);}
static void target(void *u,const char *path,unsigned value,bool playing){(void)u;(void)playing;*part(path)=value;}
int main(void){
 CaveMineHooks h={NULL,frame,target};
 assert(!cave_mine_throw(&h,300,40)&&!cave_mine_throw(&h,460,100)&&!cave_mine_throw(&h,413,40));
 *part("foreground_mc.lightmachine_mc.bigbulb_mc")=2;*part("foreground_mc.lightmachine_mc.boxtop_mc")=2;*part("foreground_mc.darkeffect_mc")=2;*part("foreground_mc.allbulbs_mc")=2;*part("background_mc.loadbar_mc")=200;
 assert(cave_mine_throw(&h,460,40));
 assert(frame(0,"background_mc.funnel_mc")==2&&frame(0,"foreground_mc.lightmachine_mc.bigbulb_mc")==1&&frame(0,"foreground_mc.darkeffect_mc")==3&&frame(0,"foreground_mc.allbulbs_mc")==3);
 assert(frame(0,"background_mc.loadbar_mc")==140&&frame(0,"foreground_mc.lightmachine_mc.smallbulb01_mc")==1&&frame(0,"foreground_mc.lightmachine_mc.smallbulb02_mc")==2);
 *part("background_mc.loadbar_mc")=30;assert(cave_mine_throw(&h,460,40));assert(frame(0,"background_mc.loadbar_mc")==1&&frame(0,"foreground_mc.lightmachine_mc.smallbulb04_mc")==1);
 puts("PASS Cave Mine funnel bounds, relighting, recharge and indicator bulbs");return 0;
}
