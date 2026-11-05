#include "game_dance_model.h"
#include <stdio.h>

int main(void){
 for(unsigned chart=0;chart<DANCE_CHARTS+DANCE_SONGS;chart++){
  unsigned song=chart<DANCE_CHARTS?chart/3:chart-DANCE_CHARTS,difficulty=chart<DANCE_CHARTS?chart%3:3;
  DanceModel generated;dance_model_start(&generated,song,difficulty);const DanceChartData *c=dance_model_chart(&generated);
  printf("C %u %u\n",chart,c->count);
  for(unsigned i=0;i<c->count;i++)printf("N %.6f %u %.6f\n",c->notes[i].time,c->notes[i].dir,c->notes[i].hold);
  for(unsigned hold=0;hold<2;hold++){
   DanceModel m;dance_model_start(&m,song,difficulty);
   for(unsigned i=0;i<c->count;i++){
    dance_model_update(&m,c->notes[i].time);dance_model_hit(&m,c->notes[i].dir);
    if(!hold)dance_model_release(&m,c->notes[i].dir);
   }
   dance_model_update(&m,m.duration);
   printf("R %u %u %u %u\n",hold,m.score,dance_model_coins(&m),m.maxCombo);
  }
 }
 static const unsigned approaches[]={4000,2500,1750,1500};
 static const unsigned offsets[4][10]={{58,59,116,117,290,291,581,582,1162,1163},{36,37,72,73,181,182,363,364,726,727},{25,26,50,51,127,128,254,255,508,509},{21,22,43,44,109,110,218,219,436,437}};
 for(unsigned a=0;a<4;a++)for(unsigned i=0;i<10;i++)for(int sign=-1;sign<=1;sign+=2){
  DanceModel m;dance_model_start(&m,0,3);m.expertChart.count=1;m.expertChart.approach=approaches[a];m.expertNotes[0]=(DanceNote){5000,0,0};
  dance_model_update(&m,5000+sign*(int)offsets[a][i]);int grade=dance_model_hit(&m,0);
  printf("W %u %d %d %u %u %u\n",approaches[a],sign*(int)offsets[a][i],grade,m.rating,m.score,m.noteState[0]);
 }
 static const unsigned counts[][6]={{100,0,0,0,0,0},{50,50,0,0,0,0},{90,0,10,0,0,0},{95,0,0,0,5,0},{50,0,0,0,50,0},{49,0,0,0,51,0},{99,0,0,0,0,1},{99,0,0,1,0,0}};
 for(unsigned i=0;i<sizeof counts/sizeof counts[0];i++){
  DanceModel m;dance_model_start(&m,0,3);m.expertChart.count=100;for(unsigned j=0;j<6;j++)m.counts[j]=counts[i][j];
  printf("G %s\n",dance_model_grade(&m));
 }
 for(unsigned song=0;song<DANCE_SONGS;song++){DanceModel m;dance_model_start(&m,song,0);printf("O %.9f\n",dance_model_audio_offset(&m));}
 return 0;
}
