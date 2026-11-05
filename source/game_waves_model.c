#include "game_waves_model.h"
#include <math.h>
#include <string.h>

#define WAVE_MS 25000.0
#define BASE_SPEED 280.0   /* stage px/s; the web model's constant speed, so distance points match */
#define MIN_SPEED 170.0
#define MAX_SPEED 520.0
#define FACE_RATE 240.0    /* px/s up or down the face */
#define GRAVITY 1150.0     /* px/s^2 in the air */
#define JUMP 430.0         /* px/s ollie off the face */
#define SPIN_RATE 540.0    /* deg/s, as the web model */
#define CURL_START 40.0
#define CURL_LIMIT -160.0
#define SHARK_START -420.0
#define WIPE_MS 1300.0
#define ASSIST_RATE 300.0 /* deg/s: with no turn held, a rotation within 100 deg of upright eases back */
enum {HELD_LEFT=1,HELD_RIGHT=2};

double waves_model_surface(const WavesModel *m){return m->air?m->launchY:m->y;}
double waves_model_progress(const WavesModel *m){double p=m->time/WAVE_MS;return p<0?0:p>1?1:p;}
unsigned waves_model_coins(const WavesModel *m){unsigned coins=(unsigned)(m->score/10.0);return coins>200?200:coins;}
static void queue(WavesModel *m,const char *name){if(m->soundCount<WAVES_MAX_SOUNDS)m->sounds[m->soundCount++]=name;}
static double clamp(double v,double lo,double hi){return v<lo?lo:v>hi?hi:v;}
static void reset_ride(WavesModel *m){
 m->air=false;m->vy=0;m->ride=0;m->spin=0;m->flip=0;m->y=150;m->launchY=m->y;m->x=480;m->speed=BASE_SPEED;
 m->curl=CURL_START;m->sharkX=SHARK_START;m->wipe=0;
}
void waves_model_start(WavesModel *m){
 unsigned mode=m->mode;memset(m,0,sizeof *m);m->mode=(WavesMode)mode;m->phase=WAVES_PLAYING;m->wave=1;m->lives=3;reset_ride(m);
}
void waves_model_init(WavesModel *m,WavesMode mode){memset(m,0,sizeof *m);m->mode=mode;waves_model_start(m);}
static void launch(WavesModel *m,double velocity){
 m->air=true;m->latch=HELD_LEFT|HELD_RIGHT;m->vy=-velocity/1000.0;m->launchY=m->y;m->spin=0;m->flip=0;queue(m,"jump");
}
bool waves_model_jump(WavesModel *m){
 if(m->phase!=WAVES_PLAYING||m->air||m->wipe>0)return false;
 launch(m,JUMP+(m->speed-BASE_SPEED)*.35);return true;
}
/** Eases a rotation toward the nearest full turn when it is within 100 degrees of it. */
static double assist(double angle,double dt){
 double turn=floor(angle/360.0+.5)*360.0,off=angle-turn,step=ASSIST_RATE*dt;
 if(fabs(off)>100.0)return angle;
 return fabs(off)<=step?turn:angle-(off>0?step:-step);
}
static bool upright(double angle){double value=fmod(angle,360.0);if(value<0)value+=360.0;return value<45.0||value>315.0;}
static void finish(WavesModel *m,bool success){if(m->phase==WAVES_RESULT)return;m->success=success;m->phase=WAVES_RESULT;queue(m,success?"win":"lose");}
void waves_model_finish(WavesModel *m,bool success){finish(m,success);}
static void wipeout(WavesModel *m,bool shark){
 if(m->lives)m->lives--;
 m->combo=0;queue(m,"wipeout");m->wipe=WIPE_MS;m->sharkHit=shark;m->air=false;m->vy=0;m->ride=0;
 if(m->lives==0)finish(m,false);
}
static void land(WavesModel *m){
 double surface=m->launchY;
 if(upright(m->spin)&&upright(m->flip)){
  unsigned points=(unsigned)(fabs(m->spin)/180.0)*50+(unsigned)(fabs(m->flip)/180.0)*80;
  if(points){m->trickScore+=points;m->score+=points;m->combo++;if(m->combo>m->maxCombo)m->maxCombo=m->combo;m->lastTrick=points;m->trickShow=1400;queue(m,"trick");}
  m->air=false;m->vy=0;m->spin=0;m->flip=0;m->ride=0;m->y=surface;
 }else{m->y=surface;m->spin=0;m->flip=0;wipeout(m,false);}
 if(m->mode==WAVES_LESSON&&m->trickScore>0)finish(m,true);
}
static void end_wave(WavesModel *m){
 queue(m,"wave");
 if(m->wave>=3){
  bool success=m->mode==WAVES_COMPETITION?m->trickScore>=1000:m->mode==WAVES_SURVIVAL?m->lives>0:true;
  finish(m,success);return;
 }
 m->wave++;m->time=0;reset_ride(m);
}
void waves_model_update(WavesModel *m,double milliseconds,bool up,bool down,bool left,bool right){
 if(m->phase!=WAVES_PLAYING)return;
 double ms=milliseconds<0?0:milliseconds>50?50:milliseconds,dt=ms/1000.0;
 m->time+=ms;
 if(m->trickShow>0)m->trickShow-=ms;
 double waveSpeed=BASE_SPEED+18.0+14.0*(m->wave-1);
 if(m->wipe>0){
  /* Wiped out: the board tumbles back, then the surfer paddles in ahead of the curl. */
  m->wipe-=ms;m->distance+=BASE_SPEED*.5*dt;m->x=clamp(m->x-90*dt,300,560);
  if(m->wipe<=0&&m->phase==WAVES_PLAYING){double score=m->score;unsigned wave=m->wave;reset_ride(m);m->score=score;m->wave=wave;m->curl=CURL_LIMIT;}
 }else if(m->air){
  /* Directions held at take-off (climbing into the lip) do not turn until released and pressed again. */
  m->latch&=(left?HELD_LEFT:0u)|(right?HELD_RIGHT:0u);
  left=left&&!(m->latch&HELD_LEFT);right=right&&!(m->latch&HELD_RIGHT);
  m->vy+=GRAVITY/1000.0*dt;m->y+=m->vy*ms;
  if(left!=right)m->spin+=(right?1:-1)*SPIN_RATE*dt;else m->spin=assist(m->spin,dt);
  /* Up and down do not turn in the air (both would read as the same in-plane turn on screen). */
  (void)up;(void)down;m->flip=assist(m->flip,dt);
  if(m->y>=m->launchY&&m->vy>0)land(m);
 }else{
  /* Riding: up climbs toward the lip, down drops into the trough and gains speed;
   * right pumps, left stalls. Climbing hard into the lip launches an air. */
  double target=up?-FACE_RATE:down?FACE_RATE:0.0;
  m->ride+=(target-m->ride)*(dt*7.0<1?dt*7.0:1.0);
  m->y+=m->ride*dt;
  m->speed+=m->ride*.55*dt+(right?110.0:left?-140.0:0.0)*dt;
  m->speed+=(BASE_SPEED-m->speed)*(dt*.6<1?dt*.6:1.0);
  m->speed=clamp(m->speed,MIN_SPEED,MAX_SPEED);
  if(m->y<=WAVES_FACE_TOP){m->y=WAVES_FACE_TOP;if(m->ride<-150.0)launch(m,-m->ride*1.25+(m->speed-BASE_SPEED)*.3);else m->ride=0;}
  if(m->y>WAVES_FACE_BOTTOM){m->y=WAVES_FACE_BOTTOM;if(m->ride>0)m->ride=0;}
  m->spin=0;m->flip=0;
 }
 m->distance+=m->speed*dt;m->score+=m->speed*dt/10.0;
 m->x=clamp(m->x+(m->speed-BASE_SPEED)*.35*dt,330,560);
 /* The breaking wave gains on a slow surfer; riding near it is tube riding (bonus points). */
 m->curl+=(waveSpeed-m->speed)*dt;if(m->curl<CURL_LIMIT)m->curl=CURL_LIMIT;
 m->tube=!m->air&&m->wipe<=0&&m->x-m->curl<190.0;
 if(m->tube)m->score+=12.0*dt;
 if(m->wipe<=0&&!m->air&&m->curl>=m->x-40.0){wipeout(m,false);m->curl=CURL_LIMIT;}
 if(m->mode==WAVES_SURVIVAL){
  m->sharkX+=(waveSpeed+30.0-m->speed)*dt;if(m->sharkX<SHARK_START)m->sharkX=SHARK_START;
  if(m->wipe<=0&&!m->air&&m->sharkX>=m->x-60.0){wipeout(m,true);m->sharkX=SHARK_START;}
 }
 if(m->phase==WAVES_PLAYING&&m->time>=WAVE_MS)end_wave(m);
}
