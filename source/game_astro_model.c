// Astro Barrier rules, a line-for-line port of src/games/astro-model.ts (trace parity: tests/game-astro.mjs).
// Rebuilt from AstroBarrier.swf sprite 235: shots, gun, blaster-bullet and bullet clip actions per 24 Hz frame.
#include "game_astro_model.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GUN_MIN 48
#define GUN_MAX 352
#define GUN_SPEED 5
#define BULLET_SPEED 24
#define TURRET_SPEED 30
#define TOTAL_LEVELS 40

static bool overlap(const double a[4],const double b[4]){return a[0]<b[2]&&a[2]>b[0]&&a[1]<b[3]&&a[3]>b[1];}
static void apply(const double m[6],const double box[4],double out[4]){
 const double xs[4]={box[0],box[2],box[0],box[2]},ys[4]={box[1],box[1],box[3],box[3]};
 for(int i=0;i<4;i++){double x=m[0]*xs[i]+m[2]*ys[i]+m[4],y=m[1]*xs[i]+m[3]*ys[i]+m[5];
  if(!i||x<out[0])out[0]=x;
  if(!i||y<out[1])out[1]=y;
  if(!i||x>out[2])out[2]=x;
  if(!i||y>out[3])out[3]=y;}
}
static void multiply(const double p[6],const double q[6],double out[6]){
 double r[6]={p[0]*q[0]+p[2]*q[1],p[1]*q[0]+p[3]*q[1],p[0]*q[2]+p[2]*q[3],p[1]*q[2]+p[3]*q[3],p[0]*q[4]+p[2]*q[5]+p[4],p[1]*q[4]+p[3]*q[5]+p[5]};
 memcpy(out,r,sizeof r);
}
static void sound(AstroModel *m,const char *name){if(m->soundCount<ASTRO_MAX_SOUNDS)m->sounds[m->soundCount++]=name;}
static void stamp(AstroModel *m,unsigned id){for(unsigned i=0;i<m->stampCount;i++)if(m->stamps[i]==id)return;if(m->stampCount<12)m->stamps[m->stampCount++]=id;}
static unsigned ordinal(const char *name){size_t n=strlen(name);while(n&&isdigit((unsigned char)name[n-1]))n--;return (unsigned)atoi(name+n);}

unsigned astro_model_index(const AstroModel *m){return m->secretSet==0?m->level-1:m->secretSet==1?39+m->level:49+m->level;}
const AstroLevelData *astro_model_level(const AstroModel *m){unsigned i=astro_model_index(m);return &ASTRO_LEVEL_DATA[i<ASTRO_LEVELS?i:0];}
static const AstroIntroData *intro_for(unsigned level){for(unsigned i=0;i<ASTRO_INTROS;i++)if(ASTRO_INTRO_DATA[i].level==level)return &ASTRO_INTRO_DATA[i];return NULL;}
const AstroIntroData *astro_model_intro(const AstroModel *m){return m->phase==ASTRO_INTRO?intro_for(m->level):NULL;}
unsigned astro_model_display_frame(const AstroModel *m){
 if(m->phase==ASTRO_TITLE)return 1;
 if(m->phase==ASTRO_OVER)return 601;
 if(m->phase==ASTRO_WON)return 602;
 const AstroIntroData *intro=astro_model_intro(m);return intro?intro->frame:astro_model_level(m)->frame;
}
unsigned astro_model_shots(const AstroModel *m){return m->shotsFrame>2?m->shotsFrame-2:0;}
unsigned astro_model_score(const AstroModel *m){return m->score;}
bool astro_model_can_fire(const AstroModel *m){return m->phase==ASTRO_PLAYING&&!m->bullet.active&&m->gunFrame==1&&(m->shotsFrame>2||astro_model_level(m)->unlimitedShots);}

static void make_object(AstroObject *o,const AstroObjectData *d){
 memset(o,0,sizeof *o);o->data=d;o->clipFrame=d->start;o->wallFrame=1;o->playing=d->clip>=0;o->frame=d->kind==ASTRO_TURRET?(d->dir?d->dir:1):1;
 memcpy(o->matrix,d->matrix,sizeof o->matrix);
 for(unsigned i=0;i<d->partCount&&i<ASTRO_MAX_PARTS;i++){const AstroPartData *p=&ASTRO_PART_DATA[d->partFirst+i];o->parts[i]=(AstroPart){p->start,1,true,false};}
}

void astro_model_init(AstroModel *m){memset(m,0,sizeof *m);m->phase=ASTRO_TITLE;m->level=1;m->lives=2;m->gunX=200;m->gunFrame=1;m->bullet.x=200;m->bullet.y=ASTRO_GUN_Y;m->turretBullet.x=-24;m->overlayText="";}

void astro_model_load(AstroModel *m){
 const AstroLevelData *l=astro_model_level(m);
 m->shotsFrame=l->unlimitedShots?0:l->shots+2;m->hitCounter=0;m->gunX=200;m->gunFrame=1;
 m->bullet=(AstroBullet){false,200,ASTRO_GUN_Y,0};m->turretBullet=(AstroBullet){false,-24,0,0};
 m->objectCount=l->count<ASTRO_MAX_OBJECTS?l->count:ASTRO_MAX_OBJECTS;
 for(unsigned i=0;i<m->objectCount;i++)make_object(&m->objects[i],&ASTRO_OBJECT_DATA[l->first+i]);
 m->overlay=0;m->phase=ASTRO_PLAYING;
}

static void enter(AstroModel *m){
 const AstroIntroData *intro=m->secretSet==0?intro_for(m->level):NULL;
 if(intro){
  if(m->level==11)m->instructions=true;
  m->phase=ASTRO_INTRO;m->introTicks=0;m->planeFrame=0;m->gunX=200;m->gunFrame=1;m->bullet.active=false;m->turretBullet.active=false;m->hitCounter=0;
  m->objectCount=0;
  for(unsigned i=0;i<intro->count&&m->objectCount<ASTRO_MAX_OBJECTS;i++)make_object(&m->objects[m->objectCount++],&ASTRO_OBJECT_DATA[intro->first+i]);
  if(intro->turret>=0&&m->objectCount<ASTRO_MAX_OBJECTS)make_object(&m->objects[m->objectCount++],&ASTRO_OBJECT_DATA[intro->turret]);
  return;
 }
 astro_model_load(m);
}

void astro_model_start(AstroModel *m,unsigned level){
 m->score=0;m->prevScore=0;m->lives=2;m->totalShots=0;m->livesCollected=0;m->levelsStarted=1;m->levelRestarted=false;m->secretSet=0;m->stampCount=0;m->instructions=false;
 m->level=level?level:1;enter(m);
}
void astro_model_press(AstroModel *m,int direction){if(direction<0)m->left=true;else m->right=true;}
void astro_model_release(AstroModel *m,int direction){if(direction<=0)m->left=false;if(direction>=0)m->right=false;}
void astro_model_trigger(AstroModel *m,bool held){m->fireHeld=held;}
void astro_model_confirm(AstroModel *m){
 if(m->phase==ASTRO_TITLE){astro_model_start(m,1);return;}
 if(m->phase==ASTRO_INTRO&&m->introTicks>ASTRO_PROMPT_READY){m->prevScore=m->score;m->hitCounter=0;astro_model_load(m);}
}

static bool clip_wall_box(int clip,unsigned clipFrame,unsigned wallFrame,const double matrix[6],double out[4]){
 if(clip<0)return false;
 const AstroClipData *c=&ASTRO_CLIP_DATA[clip];const AstroClipFrame *f=&ASTRO_CLIP_FRAMES[c->first+clipFrame-1];
 const AstroWallData *w=&ASTRO_WALL_DATA[f->wall];double full[6];multiply(matrix,f->m,full);
 unsigned state=wallFrame<w->frames?wallFrame:w->frames;apply(full,ASTRO_WALL_BOXES[w->first+state-1],out);return true;
}
bool astro_model_wall_box(const AstroModel *m,const AstroObject *o,double out[4]){(void)m;return clip_wall_box(o->data->clip,o->clipFrame,o->wallFrame,o->matrix,out);}
static const double *static_box(const AstroObject *o,double out[4]){
 if(o->data->kind==ASTRO_TURRET&&o->data->turretFirst>=0){const double *b=ASTRO_TURRET_BOXES[o->data->turretFirst+(o->frame<5?o->frame:5)-1];double dx=o->matrix[4]-o->data->matrix[4],dy=o->matrix[5]-o->data->matrix[5];for(unsigned i=0;i<4;i++)out[i]=b[i]+(i%2?dy:dx);return out;}
 return o->data->box;
}
static bool wall_stops(const AstroWallData *w,unsigned frame){for(unsigned i=0;i<w->stopCount;i++)if(w->stops[i]==frame)return true;return false;}

static void advance(AstroModel *m){
 if(m->phase==ASTRO_INTRO){
  m->introTicks++;
  if(m->introTicks>=ASTRO_PLANE_DELAY&&m->planeFrame<3)m->planeFrame=3;
  else if(m->planeFrame>=3&&m->planeFrame<11)m->planeFrame++;
 }
 if(m->phase==ASTRO_SECRET){if(++m->planeFrame>=20){m->secretSet=m->pendingSecret;m->level=1;astro_model_load(m);}return;}
 for(unsigned i=0;i<m->objectCount;i++){
  AstroObject *o=&m->objects[i];const AstroObjectData *d=o->data;
  if(d->tracking==2)o->matrix[4]=m->gunX;
  else if(o->frame!=5){if(d->tracking==1)o->matrix[5]=m->gunX+120;else if(d->tracking==3){o->matrix[4]=300-sin(m->gunX/40)*35;o->matrix[5]=200+cos(m->gunX/40)*100;}}
  if(d->clip>=0&&o->playing){const AstroClipData *c=&ASTRO_CLIP_DATA[d->clip];if(o->clipFrame<c->frames)o->clipFrame++;else if(c->loop)o->clipFrame=1;}
  if(d->clip>=0&&d->gunFrames[2]!=0&&o->wallFrame==1){const AstroClipData *c=&ASTRO_CLIP_DATA[d->clip];double frame=floor(floor(m->gunX*d->gunFrames[0]+d->gunFrames[1]+.5)*d->gunFrames[2]);o->clipFrame=frame<1?1:frame>c->frames?c->frames:(unsigned)frame;o->playing=false;}
  if(o->wallPlaying&&d->clip>=0){
   const AstroClipData *c=&ASTRO_CLIP_DATA[d->clip];const AstroWallData *w=&ASTRO_WALL_DATA[ASTRO_CLIP_FRAMES[c->first+o->clipFrame-1].wall];
   if(o->wallFrame<w->frames&&!wall_stops(w,o->wallFrame))o->wallFrame++;else o->wallPlaying=false;
  }
  if(d->kind==ASTRO_ORANGE&&o->frame>1&&o->frame<18)o->frame++;
  for(unsigned p=0;p<d->partCount&&p<ASTRO_MAX_PARTS;p++)if(o->parts[p].playing){
   const AstroClipData *c=&ASTRO_CLIP_DATA[ASTRO_PART_DATA[d->partFirst+p].clip];o->parts[p].clipFrame=o->parts[p].clipFrame<c->frames?o->parts[p].clipFrame+1:1;
  }
 }
}

static bool overlay_open(const AstroModel *m){return m->phase==ASTRO_DONE||m->phase==ASTRO_RESTART;}
static bool tested(const AstroModel *m,const AstroObject *o){
 unsigned n=ordinal(o->data->name);
 switch(o->data->kind){
  case ASTRO_BARRIER:case ASTRO_YELLOW:case ASTRO_SLIDE:return n<(m->phase==ASTRO_INTRO?5u:astro_model_level(m)->hits);
  case ASTRO_BLOCK:case ASTRO_ORANGE:return n<10;
  case ASTRO_BONUS:return n<5&&m->level==10;
  default:return true;
 }
}
static AstroObject *turret_of(AstroModel *m){for(unsigned i=0;i<m->objectCount;i++)if(m->objects[i].data->kind==ASTRO_TURRET)return &m->objects[i];return NULL;}
static void open_restart(AstroModel *m,const char *text){m->phase=ASTRO_RESTART;m->overlay=0;m->overlayText=text;m->levelRestarted=true;}
static void projectile_box(const AstroBullet *b,const double local[4],bool rotate,double out[4]){
 if(rotate){out[0]=b->x+local[1];out[1]=b->y+local[0];out[2]=b->x+local[3];out[3]=b->y+local[2];}
 else{out[0]=b->x+local[0];out[1]=b->y+local[1];out[2]=b->x+local[2];out[3]=b->y+local[3];}
}

static void collide(AstroModel *m,const double box[4],bool blaster){
 AstroBullet *shot=blaster?&m->turretBullet:&m->bullet;bool stop=false;double wall[4];
 for(unsigned i=0;i<m->objectCount;i++){
  AstroObject *o=&m->objects[i];if(!tested(m,o))continue;
  bool hasWall=astro_model_wall_box(m,o,wall);
  switch(o->data->kind){
   case ASTRO_BARRIER:
    if(hasWall&&overlap(box,wall)){if(!o->hit){o->wallFrame=2;o->hit=true;o->playing=false;m->hitCounter++;m->score+=10;sound(m,"sound_barrierHit");}stop=true;}
    break;
   case ASTRO_ONEUP:
    if(hasWall&&overlap(box,wall)){
     if(!o->hit){
      if(m->hitCounter==0){m->lives++;m->livesCollected++;if(blaster)stamp(m,55);if(m->livesCollected==8)stamp(m,61);o->wallFrame=3;m->score+=100;sound(m,"sound_oneup");}
      else{o->wallFrame=2;m->score+=25;sound(m,"sound_barrierHit");}
      o->hit=true;m->hitCounter++;o->playing=false;
     }
     stop=true;
    }
    break;
   case ASTRO_YELLOW:
    if(hasWall&&overlap(box,wall)){
     unsigned frame=o->wallFrame;
     if(frame==1){o->wallFrame=2;m->hitCounter++;m->score+=25;sound(m,"sound_yellowHit");}
     else if(frame==2&&!o->hit){o->wallFrame=3;o->hit=true;m->hitCounter++;o->playing=false;m->score+=10;sound(m,"sound_barrierHit");}
     stop=true;
    }
    break;
   case ASTRO_SLIDE:
    if(hasWall&&overlap(box,wall)){if(!o->hit){o->wallFrame=2;o->wallPlaying=true;o->hit=true;m->hitCounter++;o->playing=false;m->score+=25;sound(m,"sound_slide");}stop=true;}
    break;
   case ASTRO_BLOCK:if(overlap(box,o->data->box))stop=true;break;
   case ASTRO_SWITCH:
    if(overlap(box,o->data->box)){
     if(!m->instructions)m->totalShots--;
     if(o->frame==1){
      for(unsigned j=0;j<m->objectCount;j++){AstroObject *b=&m->objects[j];if(b->data->kind==ASTRO_ORANGE&&ordinal(b->data->name)<10&&b->frame==1)b->frame=2;}
      for(int n=0;n<10;n++){m->score+=5;sound(m,"sound_orangeSwitch");}
     }
     o->frame=2;stop=true;
    }
    break;
   case ASTRO_ORANGE:if(o->frame<18&&overlap(box,o->data->box))stop=true;break;
   case ASTRO_TURRET:
    if(!blaster&&o->frame<5&&overlap(box,static_box(o,wall))){o->frame=5;m->score+=5;sound(m,"sound_turretSplode");stop=true;}
    break;
   case ASTRO_TRAIN:
    if(!blaster&&m->level==8)for(unsigned p=0;p<o->data->partCount&&p<ASTRO_MAX_PARTS;p++){
     const AstroPartData *pd=&ASTRO_PART_DATA[o->data->partFirst+p];double parent[6],partBox[4];multiply(o->matrix,pd->matrix,parent);
     if(!clip_wall_box(pd->clip,o->parts[p].clipFrame,o->parts[p].wallFrame,parent,partBox)||!overlap(box,partBox))continue;
     if(!o->parts[p].hit){
      o->parts[p].hit=true;o->parts[p].wallFrame=2;m->hitCounter++;
      for(unsigned q=0;q<o->data->partCount&&q<ASTRO_MAX_PARTS;q++)o->parts[q].playing=false;
      m->score+=10;sound(m,"sound_barrierHit");
     }
     stop=true;
    }
    break;
   case ASTRO_BONUS:
    if(!blaster&&overlap(box,o->data->box)){if(!o->hit){o->frame=2;o->hit=true;m->score+=500;sound(m,"sound_startgame");m->hitCounter++;}stop=true;}
    break;
  }
 }
 const AstroIntroData *intro=astro_model_intro(m);
 if(intro&&intro->hasPlane&&m->planeFrame>=3&&m->planeFrame<=11&&overlap(box,intro->plane)){
  m->prevScore=m->score;stop=true;m->bullet.active=false;
  m->pendingSecret=intro->secretSet?intro->secretSet:1;
  if(blaster)stamp(m,54);
  m->phase=ASTRO_SECRET;m->planeFrame=14;sound(m,"sound_orangeSwitch");
 }
 double gun[4]={m->gunX+ASTRO_GUN_BOX[0],ASTRO_GUN_Y+ASTRO_GUN_BOX[1],m->gunX+ASTRO_GUN_BOX[2],ASTRO_GUN_Y+ASTRO_GUN_BOX[3]};
 if(blaster&&(m->phase==ASTRO_PLAYING||m->phase==ASTRO_INTRO)&&m->gunFrame==1&&overlap(box,gun)){
  if(m->lives>0){m->gunFrame=2;open_restart(m,"Ship\rdestroyed.\r\rRestarting\rlevel...");sound(m,"sound_gunSplode");m->bullet.active=false;}
  else if(m->lives==0){m->phase=ASTRO_OVER;sound(m,"sound_gameOver");}
 }
 if(stop)shot->active=false;
}

static void shots_clip(AstroModel *m){
 m->counted=false;
 if(m->phase!=ASTRO_PLAYING||!m->fireHeld||m->bullet.active||astro_model_level(m)->unlimitedShots)return;
 if(m->shotsFrame>2&&m->gunFrame==1){m->shotsFrame--;m->totalShots++;m->counted=true;}
}
static void win(AstroModel *m);
static void gun_clip(AstroModel *m){
 if(m->gunFrame!=1)return;
 if(m->left&&m->gunX>GUN_MIN)m->gunX-=GUN_SPEED;
 else if(m->right&&m->gunX<GUN_MAX)m->gunX+=GUN_SPEED;
 if(m->phase==ASTRO_PLAYING&&astro_model_level(m)->finalHits&&m->hitCounter>=astro_model_level(m)->finalHits){for(unsigned i=0;i<m->stampCount;i++)if(m->stamps[i]==56&&m->levelsStarted==45){stamp(m,62);break;}stamp(m,59);win(m);}
}
static void turret_clip(AstroModel *m){
 AstroObject *t=turret_of(m);AstroBullet *s=&m->turretBullet;if(!t)return;
 if(!s->active){s->x=t->matrix[4];s->y=t->matrix[5];}
 else if(t->frame==1){s->rotation=0;s->y-=TURRET_SPEED;}
 else if(t->frame==2){s->rotation=90;s->x+=TURRET_SPEED;}
 else if(t->frame==3){s->rotation=180;s->y+=TURRET_SPEED;}
 else if(t->frame==4){s->rotation=270;s->x-=TURRET_SPEED;}
 else s->y=0;
 if(m->fireHeld&&!m->bullet.active&&!overlay_open(m)&&t->frame<5&&(m->phase==ASTRO_INTRO||m->counted))s->active=true;
 if(s->y<0||s->y>480||s->x<-40||s->x>400){s->active=false;return;}
 if(!s->active)return;
 double box[4];projectile_box(s,ASTRO_TURRET_BULLET_BOX,s->rotation%180!=0,box);collide(m,box,true);
}

static void win(AstroModel *m){m->score+=(unsigned)(m->lives>0?m->lives:0)*50;m->phase=ASTRO_WON;sound(m,"sound_win");}
static void level_check(AstroModel *m){
 const AstroLevelData *l=astro_model_level(m);
 if(l->unlimitedShots)return;
 if(m->hitCounter>=l->hits){
  if(m->level>=TOTAL_LEVELS){if(m->levelsStarted==40)stamp(m,53);win(m);return;}
  if(m->level==5&&m->levelsStarted==5&&m->secretSet==0){stamp(m,51);if(m->totalShots<=15&&!m->levelRestarted)stamp(m,52);}
  if(m->secretSet==0){
   if(m->level==10&&m->levelsStarted==10&&m->totalShots<=32&&!m->levelRestarted)stamp(m,57);
   else if(m->level==20&&m->levelsStarted==20&&m->totalShots<=71&&!m->levelRestarted)stamp(m,58);
   else if(m->level==30&&m->levelsStarted==30&&m->totalShots<=112&&!m->levelRestarted)stamp(m,60);
  }
  m->turretBullet.active=false;
  if(m->shotsFrame>1){sound(m,"sound_levelDone");m->score+=(m->shotsFrame-2)*10;m->shotsFrame=1;}
  m->phase=ASTRO_DONE;m->overlay=0;
 }else if(m->shotsFrame==2&&!m->bullet.active&&m->lives<=0){m->phase=ASTRO_OVER;sound(m,"sound_gameOver");}
 else if(m->shotsFrame==2&&!m->bullet.active)open_restart(m,"No shots\rleft.\r\rRestarting\rlevel...");
}
static void bullet_clip(AstroModel *m){
 AstroBullet *s=&m->bullet;
 if(!s->active){s->x=m->gunX;s->y=ASTRO_GUN_Y;}else s->y-=BULLET_SPEED;
 if(m->fireHeld&&!s->active&&!overlay_open(m)&&m->gunFrame==1&&(m->phase==ASTRO_INTRO||m->counted||astro_model_level(m)->unlimitedShots)){s->active=true;sound(m,"sound_shoot");}
 if(s->y<0)s->active=false;
 if(s->active){double box[4];projectile_box(s,ASTRO_BULLET_BOX,false,box);collide(m,box,false);}
 if(m->phase==ASTRO_PLAYING)level_check(m);
}

static void next_level(AstroModel *m){
 m->level++;m->prevScore=m->score;m->levelsStarted++;
 if(m->secretSet==1&&m->level==11){stamp(m,56);m->secretSet=0;m->level=16;enter(m);return;}
 if(m->secretSet==2&&m->level>10){win(m);return;}
 if(m->level>TOTAL_LEVELS){win(m);return;}
 enter(m);
}
static void restart_level(AstroModel *m){m->lives--;m->score=m->prevScore;astro_model_load(m);m->levelRestarted=true;}

void astro_model_step(AstroModel *m){
 if(m->phase==ASTRO_TITLE||m->phase==ASTRO_OVER||m->phase==ASTRO_WON)return;
 advance(m);
 if(m->phase==ASTRO_SECRET)return;
 if(m->phase==ASTRO_DONE||m->phase==ASTRO_RESTART){
  if(++m->overlay>=(m->phase==ASTRO_DONE?ASTRO_OVERLAY_DONE:ASTRO_OVERLAY_RESTART)){if(m->phase==ASTRO_DONE)next_level(m);else restart_level(m);}
  return;
 }
 shots_clip(m);gun_clip(m);if(m->phase==ASTRO_WON)return;turret_clip(m);bullet_clip(m);
}
