#include "game_controller.h"
#include "game_dj3k_model.h"
#include "game_dj3k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DJ3K_ROOM 926
enum {BUTTON_EXIT=41};
enum {DJ3K_DRUM_BASE=1,DJ3K_BASS_BASE=7,DJ3K_MELODY_BASE=10,DJ3K_EFFECT_BASE=15,DJ3K_SPEAKER_BASE=23,DJ3K_NOTE_BASE=28};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 Dj3kModel model;
 unsigned shownRevision;
 unsigned keyFrames[13];
} Dj3k;

Dj3kModel *game_dj3k_model(void *game){return &((Dj3k *)game)->model;}

static void frame(Dj3k *d,const char *path,int value){swl_target(d->scene,path,value,NULL,false);}
static void label(Dj3k *d,const char *path,const char *value){swl_goto(d->scene,path,1,value,false);}

static void sync_art(Dj3k *d){
 char path[96];
 for(int i=0;i<6;i++){snprintf(path,sizeof path,"root.gameMC.rackmountedDrumLoops.squareButton%d",i+1);frame(d,path,d->model.drum==i?3:1);}
 for(int i=0;i<3;i++){snprintf(path,sizeof path,"root.gameMC.rackmountedBassLines.roundButton%d",i+1);frame(d,path,d->model.bass==i?3:1);}
 for(int i=0;i<5;i++){snprintf(path,sizeof path,"root.gameMC.rackmountedMelodyLoops.slider%d",i+1);frame(d,path,d->model.melody[i]?2:1);}
 int active=0;if(d->model.drum>=0)active++;if(d->model.bass>=0)active++;for(int i=0;i<5;i++)if(d->model.melody[i])active++;
 label(d,"root.gameMC.penguin",active==0?"off":active<3?"low":active<5?"on":"high");
 const char *equalizers[]={"root.gameMC.rackmountedDrumLoops.equaliser","root.gameMC.mixer.equaliser","root.gameMC.speakers","root.gameMC.leftTurntable","root.gameMC.rightTurntable"};
 for(unsigned i=0;i<sizeof equalizers/sizeof *equalizers;i++)label(d,equalizers[i],active?"on":"off");
 for(int i=0;i<13;i++){snprintf(path,sizeof path,"root.gameMC.noteKey%d",i+1);frame(d,path,d->keyFrames[i]?2:1);}
}
static void restart_loops(Dj3k *d){
 game_port_stop_sounds(d->port);
 if(d->model.drum>=0)game_port_sound(d->port,dj3k_drum_name(d->model.drum),55,true);
 if(d->model.bass>=0)game_port_sound(d->port,dj3k_bass_name(d->model.bass),55,true);
 for(int i=0;i<5;i++)if(d->model.melody[i])game_port_sound(d->port,dj3k_melody_name(i),50,true);
}
static void drain(Dj3k *d){
 if(d->shownRevision!=d->model.loopRevision){d->shownRevision=d->model.loopRevision;restart_loops(d);}
 for(unsigned i=0;i<d->model.soundCount;i++)game_port_sound(d->port,d->model.sounds[i],55,false);
 d->model.soundCount=0;
}
static void *create(GamePort *port){Dj3k *d=calloc(1,sizeof *d);if(!d)return NULL;d->port=port;dj3k_model_init(&d->model);return d;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Dj3k *d=game;d->bank=game_port_bank(d->port,0);d->scene=game_bank_scene(d->bank);if(!d->scene)return false;
 d->shownRevision=0;sync_art(d);return true;
}
static void tick(void *game){
 Dj3k *d=game;if(!d->scene)return;swl_tick_deferred(d->scene);drain(d);
 for(int i=0;i<13;i++)if(d->keyFrames[i])d->keyFrames[i]--;
 sync_art(d);
}
static void frame_update(void *game,float seconds,const GameInput *input){
 Dj3k *d=game;(void)seconds;(void)input;if(!d->scene)return;
 unsigned clicked=game_port_clicked(d->port);if(!clicked)return;
 if(clicked==BUTTON_EXIT){game_port_leave(d->port);return;}
 if(clicked>=DJ3K_DRUM_BASE&&clicked<DJ3K_DRUM_BASE+6){dj3k_toggle_loop(&d->model,DJ3K_DRUM,clicked-DJ3K_DRUM_BASE);return;}
 if(clicked>=DJ3K_BASS_BASE&&clicked<DJ3K_BASS_BASE+3){dj3k_toggle_loop(&d->model,DJ3K_BASS,clicked-DJ3K_BASS_BASE);return;}
 if(clicked>=DJ3K_MELODY_BASE&&clicked<DJ3K_MELODY_BASE+5){dj3k_toggle_loop(&d->model,DJ3K_MELODY,clicked-DJ3K_MELODY_BASE);return;}
 if(clicked>=DJ3K_EFFECT_BASE&&clicked<DJ3K_EFFECT_BASE+8){dj3k_effect(&d->model,clicked-DJ3K_EFFECT_BASE);return;}
 if(clicked>=DJ3K_SPEAKER_BASE&&clicked<DJ3K_SPEAKER_BASE+5){dj3k_speaker(&d->model,clicked-DJ3K_SPEAKER_BASE);return;}
 if(clicked>=DJ3K_NOTE_BASE&&clicked<DJ3K_NOTE_BASE+13){int index=clicked-DJ3K_NOTE_BASE;d->keyFrames[index]=6;dj3k_note(&d->model,index);return;}
}
static void touch(void *game,GameTouch phase,float x,float y){(void)game;(void)phase;(void)x;(void)y;}
static void close_game(void *game){Dj3k *d=game;game_port_leave(d->port);}
static void draw_bottom(void *game){
 Dj3k *d=game;GamePort *p=d->port;char line[64];
 game_port_rect(p,0,0,320,22,0x102040);
 snprintf(line,sizeof line,"DJ3K  Drums %d  Bass %d",d->model.drum+1,d->model.bass+1);
 game_port_text(p,6,6,9,0xffffff,GAME_ALIGN_LEFT,line);
 game_port_button(p,BUTTON_EXIT,270,2,44,18,"Exit",true);
 for(int i=0;i<6;i++)game_port_button(p,DJ3K_DRUM_BASE+i,4+i*52,26,48,26,i==d->model.drum?"on":"Drum",true);
 for(int i=0;i<3;i++)game_port_button(p,DJ3K_BASS_BASE+i,4+i*52,56,48,24,i==d->model.bass?"on":"Bass",true);
 for(int i=0;i<5;i++)game_port_button(p,DJ3K_MELODY_BASE+i,164+i*30,56,28,24,d->model.melody[i]?"on":"Mel",true);
 for(int i=0;i<8;i++)game_port_button(p,DJ3K_EFFECT_BASE+i,4+i*39,84,37,24,"FX",true);
 for(int i=0;i<5;i++)game_port_button(p,DJ3K_SPEAKER_BASE+i,4+i*62,112,60,24,"Spk",true);
 for(int i=0;i<13;i++)game_port_button(p,DJ3K_NOTE_BASE+i,4+i*23,140,22,48,"",true);
 game_port_text(p,6,196,8,0xffffff,GAME_ALIGN_LEFT,"Loops: drum/bass exclusive, melody layers");
 game_port_text(p,6,208,8,0xffffff,GAME_ALIGN_LEFT,"FX, speakers and 13 keys play one-shots");
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}

const GameController GAME_DJ3K_CONTROLLER={
 .key="dj3k",.kind=GAME_SOLO,.room=DJ3K_ROOM,.rate=24,
 .banks={"games/mixmaster.gpk"},.order={0,1,2},.backgroundRGB=0x101020,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame_update,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,
};
