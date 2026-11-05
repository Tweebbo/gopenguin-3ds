// Replays input lines "left right fire confirm start set" and prints the model state per frame
// for TS/C parity (tests/game-astro.mjs).
#include "../source/game_astro_model.h"
#include <stdio.h>

int main(void){
 AstroModel m;astro_model_init(&m);
 int left,right,fire,confirm,start,set;
 while(scanf("%d %d %d %d %d %d",&left,&right,&fire,&confirm,&start,&set)==6){
  if(start){astro_model_start(&m,(unsigned)start);if(set){m.secretSet=(unsigned)set;m.level=(unsigned)start;astro_model_load(&m);}}
  if(confirm)astro_model_confirm(&m);
  astro_model_release(&m,0);if(left)astro_model_press(&m,-1);if(right)astro_model_press(&m,1);
  astro_model_trigger(&m,fire!=0);
  astro_model_step(&m);
  printf("%u %u %u %u %d %u %u %u %u %.6f %u %d %.6f %.6f %d %.6f %.6f %u %u %u %u %u %u",(unsigned)m.phase,m.level,m.secretSet,m.score,m.lives,m.shotsFrame,m.hitCounter,m.totalShots,m.levelsStarted,
   m.gunX,m.gunFrame,m.bullet.active,m.bullet.x,m.bullet.y,m.turretBullet.active,m.turretBullet.x,m.turretBullet.y,m.overlay,m.introTicks,m.planeFrame,m.livesCollected,m.levelRestarted,m.stampCount);
  for(unsigned i=0;i<m.stampCount;i++)printf(" %u",m.stamps[i]);
  for(unsigned i=0;i<m.objectCount;i++){const AstroObject *o=&m.objects[i];printf(" %u %u %u %d %.6f %.6f",o->clipFrame,o->wallFrame,o->frame,o->hit,o->matrix[4],o->matrix[5]);for(unsigned p=0;p<o->data->partCount;p++)printf(" %u %d",o->parts[p].clipFrame,o->parts[p].hit);}
  putchar('\n');m.soundCount=0;
 }
 return 0;
}
