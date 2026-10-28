#include "game_hydro_model.h"
#include "game_hydro_data.h"
#include <math.h>
#include <string.h>

typedef struct {unsigned numHazards,totalHazards,randomHazards;bool extraLife;} HydroLevel;
static const HydroLevel LEVELS[17]={{0,0,0,false},{2,20,10,false},{3,45,12,true},{4,60,14,true},{6,90,16,true},{8,120,18,true},{10,150,18,true},
 {12,180,18,true},{14,210,18,true},{16,240,18,true},{18,270,18,true},{20,300,18,true},{25,375,18,true},{30,450,18,true},{35,525,18,true},
 {40,600,18,true},{50,750,18,true}};

/** Math.round: halves round towards +infinity. */
static double js_round(double v){return floor(v+.5);}
static const HydroLevel *spec(const HydroModel *m){return &LEVELS[m->level<16?m->level:16];}
static const HydroPenguinData *penguin(const HydroModel *m){return &HYDRO_PENGUIN_DATA[m->variant<HYDRO_VARIANTS?m->variant:0];}
static void sound(HydroModel *m,HydroSound s){if(m->soundCount<HYDRO_MAX_EVENTS)m->sounds[m->soundCount++]=s;else m->overflow=true;}
static void boat(HydroModel *m,HydroBoat b){if(m->boatCount<HYDRO_MAX_EVENTS)m->boat[m->boatCount++]=b;else m->overflow=true;}

unsigned hydro_model_variant(unsigned hand){return hand==711?1:hand==712?2:0;}
unsigned hydro_model_total(const HydroModel *m){return spec(m)->totalHazards;}
unsigned hydro_model_concurrent(const HydroModel *m){return spec(m)->numHazards+1;}
double hydro_model_hazard_y(unsigned clip,unsigned frame){
 if(clip>=HYDRO_CLIPS||frame<1||frame>HYDRO_CLIP_DATA[clip].frames)return HYDRO_HAZARD_Y;
 return HYDRO_CLIP_DATA[clip].y[frame-1]+HYDRO_HAZARD_Y;
}
void hydro_model_player_art(unsigned frame,double *x,double *y){
 frame=frame<1?1:frame>HYDRO_PLAYER_FRAMES?HYDRO_PLAYER_FRAMES:frame;
 *x=HYDRO_PLAYER_POSITIONS[frame-1][2];*y=HYDRO_PLAYER_POSITIONS[frame-1][3];
}

void hydro_model_init(HydroModel *m,HydroRandom random,void *user){
 memset(m,0,sizeof *m);m->level=1;m->lives=4;m->jumpPoint=10;m->missPoint=1;m->playerFrame=2;m->penguinFrame=1;m->random=random;m->randomUser=user;
}

void hydro_model_start(HydroModel *m,unsigned hand){
 m->jumpPoint=hand==711||hand==712?20:10;m->variant=hydro_model_variant(hand);
 m->count=0;m->hazardCounter=0;m->frameCounter=0;m->levelDone=false;
 m->jumping=m->crashed=m->invincible=false;m->invincibleCounter=0;m->penguinFrame=1;m->penguinPlaying=false;
}

void hydro_model_mouse(HydroModel *m,double gameX){
 double step=js_round(gameX/10);if(!(step>0))step=0;if(step>40)step=40;
 m->playerFrame=(unsigned)step+2;
 unsigned frame=m->playerFrame>HYDRO_PLAYER_FRAMES?HYDRO_PLAYER_FRAMES:m->playerFrame;
 m->x=HYDRO_PLAYER_POSITIONS[frame-1][0];m->y=HYDRO_PLAYER_POSITIONS[frame-1][1];
 m->tilex1=(int)floor((m->x-20)/40);m->tilex2=(int)floor((m->x+20)/40);m->tiley=(int)floor(m->y/40);
}

void hydro_model_jump(HydroModel *m){
 if(m->crashed||m->jumping||m->over||m->levelDone)return;
 sound(m,HYDRO_SOUND_JUMP);m->jumping=true;m->penguinFrame=penguin(m)->jump;m->penguinPlaying=true;
}

static void crash_done(HydroModel *m){
 if(m->lives>0){m->penguinFrame=penguin(m)->invincible;m->penguinPlaying=false;m->invincibleCounter=24;m->crashed=true;m->jumping=false;}
 else m->over=true;
}

static void animate_penguin(HydroModel *m){
 if(!m->penguinPlaying)return;
 m->penguinFrame++;
 if(m->penguinFrame==penguin(m)->jumpDone){sound(m,HYDRO_SOUND_SPLASH);m->jumping=false;m->penguinFrame=1;m->penguinPlaying=false;}
 else if(m->penguinFrame==penguin(m)->crashDone){m->penguinPlaying=false;crash_done(m);}
}

void hydro_model_new_hazard(HydroModel *m){
 if(m->hazardCounter>=spec(m)->totalHazards)return;
 if(m->count>=HYDRO_MAX_HAZARDS){m->overflow=true;return;}
 unsigned n=m->hazardCounter,x=((unsigned)floor(m->random(m->randomUser)*8)+1)*40+20;
 if(x>160&&x<240)boat(m,HYDRO_BOAT_BUMP);
 bool extraLife=n==10&&spec(m)->extraLife&&m->lives<4;
 unsigned kind=extraLife?25:(unsigned)floor(m->random(m->randomUser)*spec(m)->randomHazards);
 if(kind>=HYDRO_KINDS)kind=HYDRO_KINDS-1;
 m->hazards[m->count++]=(HydroHazard){.n=n,.x=x,.kind=kind,.clip=HYDRO_KIND_DATA[kind].clip,.flip=HYDRO_KIND_DATA[kind].flip,
  .extraLife=extraLife,.buoy=kind==10||kind==11,.y=0};
 m->hazardCounter++;
}

static int find(const HydroModel *m,unsigned n){for(unsigned i=0;i<m->count;i++)if(m->hazards[i].n==n)return (int)i;return -1;}
static void remove_hazard(HydroModel *m,unsigned n){
 int i=find(m,n);if(i<0)return;
 memmove(&m->hazards[i],&m->hazards[i+1],(m->count-(unsigned)i-1)*sizeof m->hazards[0]);m->count--;
 if(m->hazardCounter==spec(m)->totalHazards){if(!m->count)m->levelDone=true;}
 else hydro_model_new_hazard(m);
}

static void crash(HydroModel *m){
 if(m->crashed)return;
 sound(m,HYDRO_SOUND_CRASH);m->crashed=true;m->jumping=false;
 m->penguinFrame=penguin(m)->crash;m->penguinPlaying=true;m->crashedOnce=true;m->crashX=m->x;m->crashY=m->y;m->crashes++;m->lives--;
}

static void update_hazards(HydroModel *m){
 // The original iterates over a copy: hazards spawned by remove() wait for the next tick.
 unsigned order[HYDRO_MAX_HAZARDS],count=m->count;
 for(unsigned i=0;i<count;i++)order[i]=m->hazards[i].n;
 for(unsigned k=0;k<count;k++){
  int index=find(m,order[k]);if(index<0)continue;
  HydroHazard *h=&m->hazards[index];const HydroClipData *clip=&HYDRO_CLIP_DATA[h->clip];
  h->frame++;
  if(h->frame==clip->update)h->play=true;
  h->y=hydro_model_hazard_y(h->clip,h->frame);
  if(h->play){
   int col=(int)floor((double)h->x/40),row=(int)floor(h->y/40);
   if(row==m->tiley&&(col==m->tilex1||col==m->tilex2)&&!h->scored){
    if(h->extraLife){
     if(m->lives<4){sound(m,HYDRO_SOUND_BONUS);m->lives++;}
     h->scored=true;remove_hazard(m,h->n);continue;
    }else if(m->jumping&&!h->buoy){sound(m,HYDRO_SOUND_SCORE);boat(m,HYDRO_BOAT_SCORE);m->score+=m->jumpPoint;h->scored=true;}
    else if(!m->invincible&&!m->crashed)crash(m);
   }
   if(row>8&&!h->scored){m->score+=m->missPoint;h->scored=true;}
  }
  if(h->frame>=clip->done)remove_hazard(m,h->n);
 }
}

void hydro_model_step(HydroModel *m){
 if(m->over||m->levelDone)return;
 animate_penguin(m);
 update_hazards(m);
 if(m->frameCounter>12){if(m->hazardCounter<spec(m)->numHazards+1)hydro_model_new_hazard(m);m->frameCounter=0;}else m->frameCounter++;
 if(m->invincibleCounter>0){m->invincible=true;m->invincibleCounter--;}
 else if(m->invincible){m->penguinFrame=1;m->penguinPlaying=false;m->invincible=false;m->crashed=false;}
}

void hydro_model_next_level(HydroModel *m){m->level++;if(m->level>=100)m->over=true;}
