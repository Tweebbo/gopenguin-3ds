// Astro Barrier rules mirrored from the PC model (web/src/games/astro-model.ts).
#include "game_astro_model.h"
#include <string.h>

#define GUN_MIN 48.0
#define GUN_MAX 352.0
#define GUN_SPEED 5.0
#define BULLET_SPEED 24.0
#define BULLET_TOP 0.0
#define GUN_Y 459.95

static bool overlap(const double first[4],const double second[4]){
 return first[0]<second[2]&&first[2]>second[0]&&first[1]<second[3]&&first[3]>second[1];
}

static void sound(AstroModel *model,const char *name){
 if(model->soundCount<ASTRO_MAX_SOUNDS)model->sounds[model->soundCount++]=name;
}

void astro_model_init(AstroModel *model){
 memset(model,0,sizeof *model);
 model->phase=ASTRO_READY;model->lives=2;model->gunX=200;model->bulletY=GUN_Y;
}

void astro_model_start(AstroModel *model){
 model->level=0;model->score=0;model->lives=2;astro_model_load(model);
}

void astro_model_load(AstroModel *model){
 const AstroLevelData *level=&ASTRO_LEVEL_DATA[model->level];
 model->shots=level->shots;model->hits=0;model->gunX=200;model->bullet=false;
 model->bulletY=GUN_Y;model->velocity=0;model->objectCount=level->count;
 for(unsigned i=0;i<level->count;i++){
  model->objects[i].data=&ASTRO_OBJECT_DATA[level->first+i];
  model->objects[i].frame=1;model->objects[i].hit=false;
 }
 model->phase=ASTRO_PLAYING;
}

void astro_model_press(AstroModel *model,int direction){model->velocity=(direction<0?-1:1)*GUN_SPEED;}
void astro_model_release(AstroModel *model){model->velocity=0;}

bool astro_model_fire(AstroModel *model){
 if(model->phase!=ASTRO_PLAYING||model->bullet||!model->shots)return false;
 model->bullet=true;model->bulletY=GUN_Y;model->shots--;sound(model,"sound_shoot");
 return true;
}

static void destroy(AstroModel *model,AstroObject *object,unsigned points,const char *name){
 object->hit=true;object->frame=2;model->hits++;model->score+=points;model->bullet=false;sound(model,name);
}

static void resolve(AstroModel *model){
 double bullet[4]={model->gunX-1.7,model->bulletY-3.6,model->gunX+2.3,model->bulletY+0.4};
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_BARRIER||object->hit)continue;
  if(overlap(bullet,object->data->box)){destroy(model,object,10,"sound_barrierHit");return;}
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_ONEUP||object->hit)continue;
  if(!overlap(bullet,object->data->box))continue;
  object->hit=true;object->frame=model->hits==0?3:2;model->hits++;
  if(object->frame==3){model->lives++;model->score+=100;sound(model,"sound_oneup");}
  else{model->score+=25;sound(model,"sound_barrierHit");}
  model->bullet=false;return;
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_YELLOW||object->hit)continue;
  if(!overlap(bullet,object->data->box))continue;
  model->bullet=false;
  if(object->frame==1){
   object->frame=2;model->hits++;model->score+=25;sound(model,"sound_yellowHit");
  }else if(object->frame==2){
   object->frame=3;object->hit=true;model->hits++;model->score+=10;sound(model,"sound_barrierHit");
  }
  return;
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_SLIDE||object->hit)continue;
  if(overlap(bullet,object->data->box)){destroy(model,object,25,"sound_slide");return;}
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind==ASTRO_BLOCK&&overlap(bullet,object->data->box)){model->bullet=false;return;}
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_SWITCH)continue;
  if(!overlap(bullet,object->data->box))continue;
  object->frame=2;model->score+=5;sound(model,"sound_orangeSwitch");
  for(unsigned j=0;j<model->objectCount;j++)if(model->objects[j].data->kind==ASTRO_ORANGE)model->objects[j].frame=18;
  model->bullet=false;return;
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind==ASTRO_ORANGE&&object->frame<18&&overlap(bullet,object->data->box)){model->bullet=false;return;}
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_TURRET||object->frame>=5)continue;
  if(overlap(bullet,object->data->box)){
   object->frame=5;model->score+=5;model->bullet=false;sound(model,"sound_turretSplode");return;
  }
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind==ASTRO_TRAIN&&overlap(bullet,object->data->box)){model->bullet=false;return;}
 }
 for(unsigned i=0;i<model->objectCount;i++){
  AstroObject *object=&model->objects[i];
  if(object->data->kind!=ASTRO_BONUS||object->hit)continue;
  if(overlap(bullet,object->data->box)){destroy(model,object,500,"sound_startgame");return;}
 }
}

void astro_model_step(AstroModel *model){
 if(model->phase!=ASTRO_PLAYING)return;
 model->gunX+=model->velocity;
 if(model->gunX<GUN_MIN)model->gunX=GUN_MIN;
 if(model->gunX>GUN_MAX)model->gunX=GUN_MAX;
 if(model->bullet){
  model->bulletY-=BULLET_SPEED;
  if(model->bulletY<=BULLET_TOP)model->bullet=false;
  else resolve(model);
 }
 if(!model->bullet){
  const AstroLevelData *level=&ASTRO_LEVEL_DATA[model->level];
  if(model->hits>=level->hits){
   model->score+=model->shots*10;model->phase=ASTRO_DONE;sound(model,"sound_levelDone");
  }else if(!model->shots){
   model->lives--;model->phase=model->lives<=0?ASTRO_OVER:ASTRO_LOST;
   sound(model,model->phase==ASTRO_OVER?"sound_gameOver":"sound_barrierHit");
   if(model->phase==ASTRO_OVER)model->score+=(unsigned)(model->lives>0?model->lives:0)*50;
  }
 }
}

void astro_model_next(AstroModel *model){
 if(model->phase!=ASTRO_DONE)return;
 if(model->level>=ASTRO_NORMAL_LEVELS-1){model->phase=ASTRO_WON;sound(model,"sound_win");return;}
 model->level++;astro_model_load(model);
}

void astro_model_restart(AstroModel *model){
 if(model->phase==ASTRO_LOST)astro_model_load(model);
}

const AstroLevelData *astro_model_level(const AstroModel *model){return &ASTRO_LEVEL_DATA[model->level];}
unsigned astro_model_score(const AstroModel *model){return model->score;}
