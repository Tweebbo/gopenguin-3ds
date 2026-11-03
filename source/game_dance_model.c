#include "game_dance_model.h"
#include <math.h>
#include <string.h>

static const double WINDOW=200.0;
void dance_model_init(DanceModel *m){memset(m,0,sizeof *m);m->phase=DANCE_PLAYING;}
const DanceChartData *dance_model_chart(const DanceModel *m){return &DANCE_CHART_DATA[m->song*3+m->difficulty];}
void dance_model_start(DanceModel *m,unsigned song,unsigned difficulty){
 memset(m,0,sizeof *m);m->song=song;m->difficulty=difficulty;const DanceSongData *s=&DANCE_SONGS_DATA[song];m->duration=s->lengthBeats*s->millisPerBeat+2000;m->phase=DANCE_PLAYING;
}
static int judgement_at(double distance){if(distance<=45)return 0;if(distance<=90)return 1;if(distance<=140)return 2;if(distance<=200)return 3;return -1;}
int dance_model_hit(DanceModel *m,unsigned direction){
 if(m->phase!=DANCE_PLAYING)return -1;
 const DanceChartData *c=dance_model_chart(m);int best=-1;double distance=1e9;
 for(unsigned i=0;i<c->count;i++)if(m->noteState[i]==0&&c->notes[i].dir==direction){double d=fabs((double)c->notes[i].time-m->position);if(d<distance&&d<=WINDOW){best=(int)i;distance=d;}}
 if(best<0)return -1;
 int grade=judgement_at(distance);m->noteState[best]=(uint8_t)(grade+1);m->judged++;m->counts[grade<0?4:(unsigned)grade]++;
 if(grade<=2){m->combo++;if(m->combo>m->maxCombo)m->maxCombo=m->combo;}else m->combo=0;
 unsigned value=grade==0?300:grade==1?200:grade==2?100:50;unsigned multiplier=1+(m->combo/10>3?3:m->combo/10);m->score+=value*multiplier;m->moves++;return grade;
}
void dance_model_update(DanceModel *m,double milliseconds){
 if(m->phase!=DANCE_PLAYING)return;
 m->position=milliseconds>0?milliseconds:0;
 const DanceChartData *c=dance_model_chart(m);
 for(unsigned i=0;i<c->count;i++)if(m->noteState[i]==0&&c->notes[i].time+WINDOW<m->position){m->noteState[i]=5;m->judged++;m->counts[4]++;m->combo=0;}
 if(m->position>=m->duration)m->phase=DANCE_RESULT;
}
double dance_model_accuracy(const DanceModel *m){const DanceChartData *c=dance_model_chart(m);if(!c->count)return 0;unsigned hits=0;for(unsigned i=0;i<c->count;i++)if(m->noteState[i]>=1&&m->noteState[i]<=4)hits++;return (double)hits/c->count;}
const char *dance_model_grade(const DanceModel *m){double a=dance_model_accuracy(m);return a>=.95?"AAA":a>=.9?"AA":a>=.8?"A":a>=.7?"B":a>=.6?"C":"D";}
unsigned dance_model_coins(const DanceModel *m){double a=dance_model_accuracy(m);return 10+(unsigned)lround(a*90)+(m->maxCombo==dance_model_chart(m)->count?20:0);}
