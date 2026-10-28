#include "game_beans_model.h"
#include "game_beans_data.h"
#include <math.h>
#include <string.h>

static double js_round(double v){return floor(v+.5);}
static void sound(BeansModel *m,const char *name,unsigned volume){if(m->soundCount<BEANS_MAX_SOUNDS)m->sounds[m->soundCount++]=(BeansSound){name,volume};else m->overflow=true;}
static const BeansLevelData *level(const BeansModel *m){return &BEANS_LEVEL_DATA[m->level>=1&&m->level<=BEANS_LEVELS?m->level-1:0];}

void beans_model_init(BeansModel *m,BeansRandom random,void *user){
 memset(m,0,sizeof *m);m->x=m->oldX=391.5;m->lives=3;m->level=1;m->pose=1;m->phase=BEANS_PLAYING;m->oneUp=true;m->random=random;m->user=user;
}
unsigned beans_model_coins(const BeansModel *m){return m->score/10+(m->phase==BEANS_WON?60:0);}
unsigned beans_model_payout(const BeansModel *m){return m->score+(m->phase==BEANS_WON?600:0);}

static void clamp_x(BeansModel *m,double x){m->x=x<190?190:x>555?555:x;}

void beans_model_step(BeansModel *m,double x){
 m->oldX=m->x;
 // Objects already thrown keep flying and land (airborne = 1000 only stops new throws).
 for(unsigned i=0;i<m->count;i++)m->flights[i].age++;
 if(m->phase==BEANS_WON||m->phase==BEANS_LOST)return;
 if(m->phase==BEANS_TRUCK)clamp_x(m,x);
 if(m->phase!=BEANS_PLAYING){
  m->phaseTick++;
  // TryAgain frame 93 -> continueLevel; levelUp frame 89 -> nextLevel.
  if(m->phaseTick>=(m->phase==BEANS_RETRY?92u:88u)){
   if(m->phase==BEANS_TRUCK){m->level++;m->placed=0;m->oneUp=true;m->fish=0;}
   m->held=0;m->pose=1;m->phase=BEANS_PLAYING;m->phaseTick=0;
  }
  return;
 }
 clamp_x(m,x);
 static const unsigned capacities[5]={1,2,2,3,4};
 int activity=10-((int)m->level-1)*3;if(activity<1)activity=1;
 unsigned capacity=capacities[m->level>=1&&m->level<=5?m->level-1:4];
 if(m->count>=capacity||js_round(m->random(m->user)*activity)>2)return;
 unsigned kind=(unsigned)js_round(m->random(m->user)*7);
 if(kind<4){
  if(!m->bagCount){
   for(unsigned i=0;i<20;i++)m->bags[i]=i%4;
   for(unsigned i=19;i>0;i--){unsigned j=(unsigned)floor(m->random(m->user)*(i+1));unsigned t=m->bags[i];m->bags[i]=m->bags[j];m->bags[j]=t;}
   m->bagCount=20;
  }
  kind=m->bags[--m->bagCount];
 }else if(kind==4){if(!m->oneUp||m->level<3||m->level%2==0)return;m->oneUp=false;}
 else{
  if(m->level<kind-3)return;
  for(unsigned i=0;i<m->count;i++)if(m->flights[i].kind==kind)return;
  if(kind==6&&m->fish>2+(m->level-1)*4)return;
  if(kind==6)m->fish++;
 }
 if(m->count>=BEANS_MAX_FLIGHTS){m->overflow=true;return;}
 m->flights[m->count++]=(BeansFlight){kind,2,++m->serial};
}

static int find(const BeansModel *m,unsigned id){for(unsigned i=0;i<m->count;i++)if(m->flights[i].id==id)return (int)i;return -1;}
static void remove_flight(BeansModel *m,unsigned id){int i=find(m,id);if(i<0)return;memmove(&m->flights[i],&m->flights[i+1],(m->count-(unsigned)i-1)*sizeof *m->flights);m->count--;}

static void fail(BeansModel *m,unsigned pose){
 // pose 7: crushed by the sixth bag; 8-10: anvil, fish, vase.
 sound(m,pose==7?"sound_toonhit":pose==8?"sound_anvil":pose==9?"sound_fish":"sound_vase",100);
 // Three spare lives: the fourth hit is Game Over.
 m->lives--;m->pose=pose;m->phase=m->lives<0?BEANS_LOST:BEANS_RETRY;m->phaseTick=0;
 if(m->lives<0)m->lives=0;
}

void beans_model_catch(BeansModel *m,unsigned id){
 int i=find(m,id);if(m->phase!=BEANS_PLAYING||i<0)return;
 unsigned kind=m->flights[i].kind;remove_flight(m,id);
 if(kind==4){m->lives++;sound(m,"sound_boing",100);return;}
 if(kind>=5){fail(m,kind+3);return;}
 sound(m,"sound_beanbagLand",30);
 m->score+=level(m)->caught;m->held++;
 if(m->held>5)fail(m,7);else m->pose=m->held+1;
}

void beans_model_drop(BeansModel *m){
 if((m->phase!=BEANS_PLAYING&&m->phase!=BEANS_TRUCK)||m->x>230||!m->held)return;
 m->held--;m->pose=m->held+1;sound(m,"sound_beanbagPlace",25);m->score+=level(m)->placed;
 if(m->phase==BEANS_TRUCK){m->score+=m->level*25;return;}
 m->placed++;
 if(m->placed>=level(m)->bags){m->phase=m->level==BEANS_LEVELS?BEANS_WON:BEANS_TRUCK;m->phaseTick=0;}
}

void beans_model_tick(BeansModel *m,double x){
 beans_model_step(m,x);
 unsigned kept=0;
 for(unsigned i=0;i<m->spillCount;i++)if(++m->spills[i].age<=m->spills[i].length)m->spills[kept++]=m->spills[i];
 m->spillCount=kept;
 if(m->impact&&++m->impact>6)m->impact=0;
 if(m->notify&&++m->notify>11)m->notify=0;
 const double *pose=BEANS_POSE_BOXES[m->pose>=1&&m->pose<=BEANS_POSES?m->pose-1:0],shift=m->x-391.5;
 double player[4]={pose[0]+shift,pose[1],pose[2]+shift,pose[3]};
 BeansFlight snapshot[BEANS_MAX_FLIGHTS];unsigned count=m->count;memcpy(snapshot,m->flights,sizeof snapshot);
 for(unsigned i=0;i<count;i++){
  const BeansFlight f=snapshot[i];const BeansKindData *k=&BEANS_KIND_DATA[f.kind];
  if(f.age>=k->length){
   // The final flight frame replaces a flying bag with its spill.
   if(f.kind<4){if(m->spillCount<BEANS_MAX_SPILLS)m->spills[m->spillCount++]=(BeansSpill){f.kind,f.id,1,k->spill};else m->overflow=true;}
   remove_flight(m,f.id);continue;
  }
  if(f.age<=6||f.age>=k->end)continue;
  const double *box=k->boxes[f.age-1];
  if(box[0]<player[2]&&box[2]>player[0]&&box[1]<player[3]&&box[3]>player[1]){
   int lives=m->lives;beans_model_catch(m,f.id);
   if(f.kind>=5&&m->lives<lives){m->impact=2;m->impactX=m->x;}
   // oneUpNotify._x = penguin._x; oneUpNotify.play()
   if(f.kind==4&&m->lives>lives){m->notify=1;m->notifyX=m->x;}
  }
 }
}
