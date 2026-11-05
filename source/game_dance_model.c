#include "game_dance_model.h"
#include <math.h>
#include <string.h>

static const double TRAVEL=344.0,FADE=119.45;
static const unsigned VALUES[]={10,5,2,1,0,0};
static unsigned multiplier(const DanceModel *m){static const unsigned values[]={1,2,3,4,5,6,8,10};unsigned step=m->combo/10;return values[step>7?7:step];}
static void release_note(DanceModel *m,unsigned i,double position){
 const DanceNote *note=&dance_model_chart(m)->notes[i];
 if(!m->noteState[i]||m->noteState[i]==5||m->released[i]||note->hold<=0)return;
 m->released[i]=1;double held=fmax(0,fmin(note->hold,position-m->pressed[i]));
 m->score+=(unsigned)lround(VALUES[m->noteState[i]-1]*multiplier(m)*held/500)*(m->difficulty==3?2:1);
}
void dance_model_release(DanceModel *m,unsigned direction){if(m->phase!=DANCE_PLAYING)return;const DanceChartData *c=dance_model_chart(m);for(unsigned i=0;i<c->count;i++)if(c->notes[i].dir==direction)release_note(m,i,m->position);}
void dance_model_init(DanceModel *m){memset(m,0,sizeof *m);m->rating=15;m->phase=DANCE_PLAYING;}
const DanceChartData *dance_model_chart(const DanceModel *m){return m->difficulty==3?&m->expertChart:&DANCE_CHART_DATA[m->song*3+m->difficulty];}
static double random_unit(uint32_t *seed){*seed=*seed*1664525u+1013904223u;return *seed/4294967296.0;}
static void expert_note(DanceModel *m,double beat,unsigned maxLength,double last[4],uint32_t *seed){
 unsigned hold=(unsigned)(random_unit(seed)*maxLength),lane=(unsigned)(random_unit(seed)*4);static const uint8_t dirs[]={0,3,2,1};
 if(beat<=last[lane]||m->expertChart.count>=DANCE_MAX_NOTES)return;
 double tempo=DANCE_SONGS_DATA[m->song].millisPerBeat;
 m->expertNotes[m->expertChart.count++]=(DanceNote){(float)(beat*tempo),(float)(hold*tempo),dirs[lane]};last[lane]=beat+hold;
}
void dance_model_start_seed(DanceModel *m,unsigned song,unsigned difficulty,uint32_t seed){
 memset(m,0,sizeof *m);m->rating=15;m->song=song;m->difficulty=difficulty;const DanceSongData *s=&DANCE_SONGS_DATA[song];m->duration=s->lengthBeats*s->millisPerBeat+2000;m->phase=DANCE_PLAYING;
 if(difficulty==3){
  m->expertChart=(DanceChartData){song,3,0,1500,m->expertNotes};double last[4]={0};
  for(unsigned beat=4;beat<s->lengthBeats;beat++){
   if(beat%8==0){expert_note(m,beat,4,last,&seed);if((unsigned)(random_unit(&seed)*2)==0)expert_note(m,beat,0,last,&seed);}
   else if(beat%4==0){expert_note(m,beat,4,last,&seed);if((unsigned)(random_unit(&seed)*4)==0)expert_note(m,beat,0,last,&seed);}
   else if(beat%2==0)expert_note(m,beat,2,last,&seed);
   else if((unsigned)(random_unit(&seed)*4)>0){expert_note(m,beat,0,last,&seed);if((unsigned)(random_unit(&seed)*4)==0)expert_note(m,beat+.5,0,last,&seed);}
  }
  expert_note(m,s->lengthBeats,0,last,&seed);
 }
}
void dance_model_start(DanceModel *m,unsigned song,unsigned difficulty){dance_model_start_seed(m,song,difficulty,1);}
static void judge(DanceModel *m,unsigned grade){
 m->counts[grade]++;
 if(grade<=2){m->combo++;if(m->combo>m->maxCombo)m->maxCombo=m->combo;}else m->combo=0;
 int rating=(int)m->rating+(grade<2?1:grade==2?0:grade==3?-2:-5);m->rating=rating<0?0:rating>29?29:(unsigned)rating;
 m->score+=VALUES[grade]*multiplier(m)*(m->difficulty==3?2:1);
}
static int judgement_at(double pixels){if(pixels<5)return 0;if(pixels<10)return 1;if(pixels<25)return 2;if(pixels<50)return 3;return 5;}
int dance_model_hit(DanceModel *m,unsigned direction){
 if(m->phase!=DANCE_PLAYING)return -1;
 const DanceChartData *c=dance_model_chart(m);int best=-1;double distance=1e9;
 for(unsigned i=0;i<c->count;i++)if(m->noteState[i]==0&&c->notes[i].dir==direction&&fabs(floor(c->notes[i].time/10000)-floor(m->position/10000))<=1){double d=fabs((double)c->notes[i].time-m->position);if(d<distance){best=(int)i;distance=d;}}
 if(best<0)return -1;
 double pixels=distance*TRAVEL/c->approach;if(pixels>=100){judge(m,4);return 4;}
 int grade=judgement_at(pixels);m->noteState[best]=(uint8_t)(grade+1);m->pressed[best]=m->position;m->judged++;judge(m,(unsigned)grade);m->moves++;return grade;
}
void dance_model_update(DanceModel *m,double milliseconds){
 if(m->phase!=DANCE_PLAYING)return;
 m->position=milliseconds>0?milliseconds:0;
 const DanceChartData *c=dance_model_chart(m);
 for(unsigned i=0;i<c->count;i++){
  if(c->notes[i].hold>0&&m->position>=m->pressed[i]+c->notes[i].hold)release_note(m,i,m->pressed[i]+c->notes[i].hold);
  if(m->noteState[i]==0&&c->notes[i].time+c->notes[i].hold+c->approach*FADE/TRAVEL<=m->position){m->noteState[i]=5;m->judged++;judge(m,4);}
 }
 if(m->position>=m->duration){for(unsigned i=0;i<c->count;i++)release_note(m,i,m->position);m->phase=DANCE_RESULT;}
}
double dance_model_accuracy(const DanceModel *m){unsigned total=0;for(unsigned i=0;i<6;i++)total+=m->counts[i];return total?(double)(m->counts[0]+m->counts[1]+m->counts[2])/total:0;}
const char *dance_model_grade(const DanceModel *m){double percent=round(dance_model_accuracy(m)*1000)/10;unsigned total=dance_model_chart(m)->count;return percent<50?"D":percent<75?"C":percent<100?"B":m->counts[0]==total?"AAA":m->counts[0]+m->counts[1]==total?"AA":"A";}
unsigned dance_model_coins(const DanceModel *m){return m->score<=10000?(unsigned)lround(m->score/25.0):400+(unsigned)lround((m->score-10000)/100.0);}
double dance_model_audio_offset(const DanceModel *m){return m->song==5?DANCE_SONGS_DATA[m->song].millisPerBeat/24:m->song==3||m->song==4?DANCE_SONGS_DATA[m->song].millisPerBeat/16:0;}
