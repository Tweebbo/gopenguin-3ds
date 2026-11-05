#include "game_controller.h"
#include "game_dj3k_model.h"
#include "game_dj3k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DJ3K_ROOM 926
enum {BUTTON_EXIT=41,BUTTON_PAGE=42};
enum {DJ3K_DRUM_BASE=1,DJ3K_BASS_BASE=7,DJ3K_MELODY_BASE=10,DJ3K_EFFECT_BASE=15,DJ3K_SPEAKER_BASE=23,DJ3K_NOTE_BASE=28};

typedef struct {
 GamePort *port;
 GameBank *bank;
 SWLScene *scene;
 Dj3kModel model;
 unsigned shownRevision;
 unsigned keyFrames[13];
 unsigned page;
 unsigned contact;
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
static void activate(Dj3k *d,unsigned clicked){
 if(!clicked)return;
 if(clicked==BUTTON_EXIT){game_port_leave(d->port);return;}
 if(clicked>=DJ3K_DRUM_BASE&&clicked<DJ3K_DRUM_BASE+6){dj3k_toggle_loop(&d->model,DJ3K_DRUM,clicked-DJ3K_DRUM_BASE);return;}
 if(clicked>=DJ3K_BASS_BASE&&clicked<DJ3K_BASS_BASE+3){dj3k_toggle_loop(&d->model,DJ3K_BASS,clicked-DJ3K_BASS_BASE);return;}
 if(clicked>=DJ3K_MELODY_BASE&&clicked<DJ3K_MELODY_BASE+5){dj3k_toggle_loop(&d->model,DJ3K_MELODY,clicked-DJ3K_MELODY_BASE);return;}
 if(clicked>=DJ3K_EFFECT_BASE&&clicked<DJ3K_EFFECT_BASE+8){dj3k_effect(&d->model,clicked-DJ3K_EFFECT_BASE);return;}
 if(clicked>=DJ3K_SPEAKER_BASE&&clicked<DJ3K_SPEAKER_BASE+5){dj3k_speaker(&d->model,clicked-DJ3K_SPEAKER_BASE);return;}
 if(clicked>=DJ3K_NOTE_BASE&&clicked<DJ3K_NOTE_BASE+13){int index=clicked-DJ3K_NOTE_BASE;d->keyFrames[index]=6;dj3k_note(&d->model,index);return;}
}
/* Touch screen: three zoomed control surfaces of the original deck (the upper screen shows the whole
   studio). Each page is the authored art at 1.5x to 1.7x the upper scale, so every control is a big target. */
#define HEADER 24
#define PAGE_COUNT 3
static const struct {const char *name;float scale,x,y;} PAGES[PAGE_COUNT]={
 {"Loops",.8f,0,0},      /* sampler knobs, bass lines, drum pads (and the dancing penguin) */
 {"Keys",.9f,0,238},     /* melody sliders and the keyboard */
 {"Sounds",.9f,405,238}, /* the speaker's sound buttons */
};
static bool lower_view(void *game,SWLMatrix *view){
 Dj3k *d=game;unsigned i=d->page%PAGE_COUNT;float s=PAGES[i].scale;
 *view=(SWLMatrix){s,0,0,s,-PAGES[i].x*s,HEADER-PAGES[i].y*s};
 return true;
}
static void set_page(Dj3k *d,unsigned page){d->page=page%PAGE_COUNT;d->contact=0;}
static void frame_update(void *game,float seconds,const GameInput *input){
 Dj3k *d=game;(void)seconds;if(!d->scene)return;
 unsigned clicked=game_port_clicked(d->port);
 if(clicked>=BUTTON_PAGE&&clicked<BUTTON_PAGE+PAGE_COUNT){set_page(d,clicked-BUTTON_PAGE);return;}
 if(input->down&(GAME_KEY_R|GAME_KEY_X)){set_page(d,d->page+1);return;}
 if(input->down&GAME_KEY_L){set_page(d,d->page+PAGE_COUNT-1);return;}
 activate(d,clicked);
}
static bool original_button(void *user,unsigned id,const char *path){
 Dj3k *d=user;unsigned index=0;(void)id;
 if(!strcmp(path,"root.closeBtn")){activate(d,BUTTON_EXIT);return true;}
 if(sscanf(path,"root.gameMC.button%u",&index)==1){
  if(index>=1&&index<=8){activate(d,DJ3K_EFFECT_BASE+index-1);return true;}
  if(index>=34&&index<=38){activate(d,DJ3K_SPEAKER_BASE+index-34);return true;}
 }
 return false;
}
static unsigned clip_at(Dj3k *d,float x,float y){
 /* These hit regions follow the authored deck; black keys win their overlap. */
 static const float notes[13][4]={{4,416,37,471},{28,412,52,446},{37,416,70,471},{63,412,87,446},{72,416,105,471},{106,416,139,471},{134,412,158,446},{139,416,172,471},{167,412,191,446},{173,416,206,471},{205,412,229,446},{207,416,240,471},{241,416,275,471}};
 static const unsigned keys[]={1,3,6,8,10,0,2,4,5,7,9,11,12};
 for(unsigned n=0;n<13;n++){unsigned i=keys[n];const float *r=notes[i];if(x>=r[0]&&x<r[2]&&y>=r[1]&&y<r[3])return DJ3K_NOTE_BASE+i;}
 char path[96];SWLInstance pose;
 for(unsigned i=0;i<6;i++){
  snprintf(path,sizeof path,"root.gameMC.rackmountedDrumLoops.squareButton%u",i+1);
  if(swl_pose(d->scene,path,&pose)&&x>=pose.matrix.x&&x<pose.matrix.x+31.5f*pose.matrix.a&&y>=pose.matrix.y&&y<pose.matrix.y+31.5f*pose.matrix.d)return DJ3K_DRUM_BASE+i;
 }
 static const float bass[3][4]={{37,105,96,164},{89,112,148,171},{139,118,198,177}};
 for(unsigned i=0;i<3;i++){const float *r=bass[i];if(x>=r[0]&&x<r[2]&&y>=r[1]&&y<r[3])return DJ3K_BASS_BASE+i;}
 static const float melodies[5][4]={{75,277,97,366},{96,275,118,358},{116,274,138,350},{134,272,156,342},{150,271,172,335}};
 for(unsigned i=0;i<5;i++){const float *r=melodies[i];if(x>=r[0]&&x<r[2]&&y>=r[1]&&y<r[3])return DJ3K_MELODY_BASE+i;}
 return 0;
}
static void touch(void *game,GameTouch phase,float x,float y){
 Dj3k *d=game;if(!d->scene)return;
 if(phase==GAME_TOUCH_UP){d->contact=0;return;}
 if(y<HEADER||y>=240)return;
 SWLMatrix view;lower_view(d,&view);x=(x-view.x)/view.a;y=(y-view.y)/view.d;
 unsigned control=clip_at(d,x,y);
 if(phase==GAME_TOUCH_DOWN){
  if(!control)movie_meta_click(game_bank_meta(d->bank),d->scene,x,y,&(MovieHooks){.custom=original_button},d);
  else activate(d,control);
 }else if(control>=DJ3K_NOTE_BASE&&control<DJ3K_NOTE_BASE+13&&control!=d->contact)activate(d,control);
 d->contact=control;sync_art(d);
}
static void close_game(void *game){Dj3k *d=game;game_port_leave(d->port);}
static void draw_bottom(void *game){
 Dj3k *d=game;GamePort *p=d->port;
 /* Thin header band: game name, page tabs (L/R or X switch them) and Exit, as Bean Counters and Roundup do. */
 game_port_rect(p,0,0,320,HEADER,0x102040);game_port_rect(p,0,HEADER-1,320,1,0x4fc0ff);
 game_port_text(p,6,6,11,0xffffff,GAME_ALIGN_LEFT,"DJ3K");
 game_port_text(p,52,7,9,0x8fb8e0,GAME_ALIGN_CENTER,"L");
 for(unsigned i=0;i<PAGE_COUNT;i++){
  float x=60+(float)i*64;
  if(i!=d->page){game_port_button(p,BUTTON_PAGE+i,x,2,60,HEADER-5,PAGES[i].name,true);continue;}
  /* Current page: a lit tab joined to the deck below. */
  game_port_rect(p,x,2,60,HEADER-2,0xffd23f);game_port_rect(p,x+1,3,58,HEADER-4,0xffe680);
  game_port_text(p,x+30,6,10,0x102040,GAME_ALIGN_CENTER,PAGES[i].name);
 }
 game_port_text(p,256,7,9,0x8fb8e0,GAME_ALIGN_CENTER,"R");
 game_port_button(p,BUTTON_EXIT,266,2,50,HEADER-5,"Exit",true);
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){(void)game;(void)command;(void)count;(void)args;return false;}

const GameController GAME_DJ3K_CONTROLLER={
 .key="dj3k",.kind=GAME_SOLO,.room=DJ3K_ROOM,.rate=24,
 .banks={"games/mixmaster.gpk"},.order={0,1,2},.nightPanel=true,.backgroundRGB=0x101020,
 .create=create,.destroy=destroy,.ready=ready,.frame=frame_update,.tick=tick,.touch=touch,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.lower_view=lower_view,
};
