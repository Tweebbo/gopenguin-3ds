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
  printf("%u %u %d %u %u %u %.9f %.9f %u %u",model.level,model.score,model.lives,model.shots,model.hits,
   (unsigned)model.phase,model.gunX,model.bulletY,model.bullet?1u:0u,level->frame);
  for(unsigned i=0;i<model.objectCount;i++)printf(" %u %u",model.objects[i].frame,model.objects[i].hit?1u:0u);
  putchar('\n');
  model.soundCount=0;
  if(model.phase==ASTRO_WON)break;
 }
 assert(feof(stdin)||model.phase==ASTRO_WON);
 return 0;
}
