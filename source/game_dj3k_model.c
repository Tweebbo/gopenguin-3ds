#include "game_dj3k_model.h"
#include <string.h>

static const char *DRUMS[]={"drumLoopHihatMinimal","drumLoopBongos","drumLoopHihats","drumLoopFlatBeats","drumLoopFunkyHouse","drumLoopDeepHouse"};
static const char *BASS[]={"bassLineLive","bassLineFilter","bassLineSynth"};
static const char *MELODY[]={"melodyTrumpet","melodyGuitarWahwah","melodyGuitar","melodyRhodesLowpitch","melodyRhodesHighpitch"};
static const char *EFFECTS[]={"effectChopDmShort","effectWahDmShort","effectChopAmShort","effectRideReverse","effectJungleBird","effectWahDm7Long","effectWahAm7Long","effectRide"};
static const char *SPEAKERS[]={"effectCowbell","effectKlaxon","effectCops","effectEep","effectWhistle"};
static const char *NOTES[]={"keyCLow","keyCSharp","keyD","keyDSharp","keyE","keyF","keyFSharp","keyG","keyGSharp","keyA","keyASharp","keyB","keyCHigh"};

void dj3k_model_init(Dj3kModel *m){memset(m,0,sizeof *m);m->drum=-1;m->bass=-1;}
void dj3k_toggle_loop(Dj3kModel *m,int group,int index){
 if(group==DJ3K_MELODY){if(index>=0&&index<5)m->melody[index]=!m->melody[index];}
 else if(group==DJ3K_DRUM){m->drum=m->drum==index?-1:index;}
 else if(group==DJ3K_BASS){m->bass=m->bass==index?-1:index;}
 m->loopRevision++;
}
static void queue(Dj3kModel *m,const char *name){if(m->soundCount<DJ3K_MAX_SOUNDS)m->sounds[m->soundCount++]=name;}
void dj3k_effect(Dj3kModel *m,int index){if(index>=0&&index<8)queue(m,EFFECTS[index]);}
void dj3k_speaker(Dj3kModel *m,int index){if(index>=0&&index<5)queue(m,SPEAKERS[index]);}
void dj3k_note(Dj3kModel *m,int index){if(index>=0&&index<13){m->key[index]=true;queue(m,NOTES[index]);}}
const char *dj3k_drum_name(int i){return i>=0&&i<6?DRUMS[i]:0;}
const char *dj3k_bass_name(int i){return i>=0&&i<3?BASS[i]:0;}
const char *dj3k_melody_name(int i){return i>=0&&i<5?MELODY[i]:0;}
const char *dj3k_effect_name(int i){return i>=0&&i<8?EFFECTS[i]:0;}
const char *dj3k_speaker_name(int i){return i>=0&&i<5?SPEAKERS[i]:0;}
const char *dj3k_note_name(int i){return i>=0&&i<13?NOTES[i]:0;}
