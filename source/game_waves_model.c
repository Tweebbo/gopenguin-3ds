#include "game_waves_model.h"
#include <math.h>
#include <string.h>

#define WAVE_MS 25000.0
#define SPEED 280.0
#define GRAVITY 0.0011
#define JUMP 0.42

double waves_model_surface(const WavesModel *m){return 350.0+sin(m->distance/190.0)*42.0;}
unsigned waves_model_coins(const WavesModel *m){unsigned coins=(unsigned)(m->score/10.0);return coins>200?200:coins;}
static void queue(WavesModel *m,const char *name){if(m->soundCount<WAVES_MAX_SOUNDS)m->sounds[m->soundCount++]=name;}
void waves_model_start(WavesModel *m){
 unsigned mode=m->mode;memset(m,0,sizeof *m);m->mode=(WavesMode)mode;m->phase=WAVES_PLAYING;m->wave=1;m->lives=3;m->sharkX=900;m->y=waves_model_surface(m);
}
void waves_model_init(WavesModel *m,WavesMode mode){memset(m,0,sizeof *m);m->mode=mode;m->lives=3;m->sharkX=900;waves_model_start(m);}
bool waves_model_jump(WavesModel *m){
 if(m->phase!=WAVES_PLAYING||m->air)return false;
 m->air=true;m->vy=-JUMP;m->y=waves_model_surface(m)-6;m->spin=0;m->flip=0;queue(m,"jump");return true;
}
static bool upright(double angle){double value=fmod(angle,360.0);if(value<0)value+=360.0;return value<45.0||value>315.0;}
static void finish(WavesModel *m,bool success){if(m->phase==WAVES_RESULT)return;m->success=success;m->phase=WAVES_RESULT;queue(m,success?"win":"lose");}
void waves_model_finish(WavesModel *m,bool success){finish(m,success);}
static void wipeout(WavesModel *m){
 if(m->lives)m->lives--;
 m->combo=0;queue(m,"wipeout");m->sharkX=m->distance+900;
 if(m->lives==0)finish(m,false);
}
static void land(WavesModel *m,double surface){
 if(upright(m->spin)&&upright(m->flip)){
  unsigned points=(unsigned)(fabs(m->spin)/180.0)*50+(unsigned)(fabs(m->flip)/180.0)*80;
  if(points){m->trickScore+=points;m->score+=points;m->combo++;if(m->combo>m->maxCombo)m->maxCombo=m->combo;queue(m,"trick");}
 }else wipeout(m);
 m->air=false;m->vy=0;m->spin=0;m->flip=0;m->ride=0;m->y=surface;
 if(m->mode==WAVES_LESSON&&m->trickScore>0)finish(m,true);
}
static void end_wave(WavesModel *m){
 queue(m,"wave");
 if(m->wave>=3){
  bool success=m->mode==WAVES_COMPETITION?m->trickScore>=1000:m->mode==WAVES_SURVIVAL?m->lives>0:true;
  finish(m,success);return;
 }
 m->wave++;m->time=0;m->air=false;m->vy=0;m->ride=0;m->spin=0;m->flip=0;m->sharkX=m->distance+900;m->y=waves_model_surface(m);
}
void waves_model_update(WavesModel *m,double milliseconds,bool up,bool down,bool left,bool right){
 if(m->phase!=WAVES_PLAYING)return;
 double dt=milliseconds<0?0:milliseconds>50?50:milliseconds;dt/=1000.0;
 m->time+=dt*1000.0;m->distance+=SPEED*dt;m->score+=SPEED*dt/10.0;
 double surface=waves_model_surface(m);
 if(m->air){
  m->vy+=GRAVITY*dt*1000.0;m->y+=m->vy*dt*1000.0;
  if(left!=right)m->spin+=(right?1:-1)*540.0*dt;
  if(up!=down)m->flip+=(down?1:-1)*540.0*dt;
  if(m->y>=surface&&m->vy>0)land(m,surface);
 }else{
  double target=up?-55.0:down?35.0:0.0;m->ride+=(target-m->ride)*(dt*8.0<1?dt*8.0:1.0);m->y=surface+m->ride;m->spin=0;m->flip=0;
 }
 if(m->mode==WAVES_SURVIVAL){
  m->sharkX+=60.0*dt;if(m->sharkX<m->distance+30.0)wipeout(m);
 }
 if(m->time>=WAVE_MS)end_wave(m);
}
