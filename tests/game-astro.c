#include "../source/game_astro_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void){
 AstroModel model;astro_model_init(&model);astro_model_start(&model);
 int direction=0,fire=0,action=0;
 while(scanf("%d %d %d",&direction,&fire,&action)==3){
  if(action==1)astro_model_next(&model);
  else if(action==2)astro_model_restart(&model);
  if(direction<0)astro_model_press(&model,-1);
  else if(direction>0)astro_model_press(&model,1);
  else astro_model_release(&model);
  if(fire)astro_model_fire(&model);
  astro_model_step(&model);
  const AstroLevelData *level=astro_model_level(&model);
  printf("%u %u %d %u %u %u %.9f %.9f %u %u %u %u %u %u",model.level,model.score,model.lives,model.shots,model.hits,
   (unsigned)model.phase,model.gunX,model.bulletY,model.bullet?1u:0u,level->frame,model.totalShots,model.oneUps,model.restarted?1u:0u,model.stampCount);
  for(unsigned i=0;i<model.stampCount;i++)printf(" %u",model.stamps[i]);
  for(unsigned i=0;i<model.objectCount;i++)printf(" %u %u",model.objects[i].frame,model.objects[i].hit?1u:0u);
  putchar('\n');
  model.soundCount=0;
  if(model.phase==ASTRO_WON)break;
 }
 assert(feof(stdin)||model.phase==ASTRO_WON);
 AstroModel stamps;astro_model_init(&stamps);astro_model_start(&stamps);
 stamps.level=4;astro_model_load(&stamps);stamps.totalShots=5;stamps.hits=astro_model_level(&stamps)->hits;astro_model_step(&stamps);
 assert(stamps.phase==ASTRO_DONE);assert(stamps.stampCount==2&&stamps.stamps[0]==51&&stamps.stamps[1]==52);
 AstroModel last;astro_model_init(&last);astro_model_start(&last);last.level=ASTRO_NORMAL_LEVELS-1;astro_model_load(&last);last.hits=astro_model_level(&last)->hits;astro_model_step(&last);
 assert(last.phase==ASTRO_DONE);assert(last.stampCount==1&&last.stamps[0]==53);
 return 0;
}
