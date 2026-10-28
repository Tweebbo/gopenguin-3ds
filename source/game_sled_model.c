#include "game_sled_model.h"
#include "game_sled_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLED_TICK (1000.0/24)
static double js_round(double v){return floor(v+.5);}
unsigned sled_model_sled(unsigned hand){return hand==5021?2:hand==5046?3:hand==5047?4:1;}

void sled_model_init(SledModel *m,unsigned waddle){
 memset(m,0,sizeof *m);m->me=-1;
 char key[16];snprintf(key,sizeof key,"%u",waddle);unsigned hill=SLED_HILLS-1;
 for(unsigned i=0;i<SLED_HILLS;i++)if(!strcmp(SLED_HILL_DATA[i].waddle,key))hill=i;
 if(strcmp(SLED_HILL_DATA[hill].waddle,key))for(unsigned i=0;i<SLED_HILLS;i++)if(!strcmp(SLED_HILL_DATA[i].waddle,"default"))hill=i;
 m->hill=SLED_HILL_DATA[hill].tiles;m->hillLength=SLED_HILL_DATA[hill].length;
 for(unsigned t=0;t<m->hillLength&&m->mapRows+SLED_TILE_ROWS<=SLED_MAP_ROWS;t++){
  unsigned type=0;for(unsigned k=0;k<SLED_TILE_TYPES;k++)if(SLED_TILE_IDS[k]==m->hill[t])type=k;
  for(unsigned r=0;r<SLED_TILE_ROWS;r++)memcpy(m->rows[m->mapRows++],SLED_TILES[type][r],SLED_TILE_COLS);
 }
 m->racerCount=4;for(unsigned i=0;i<SLED_PLAYER_COUNTS;i++)if(SLED_PLAYERS[i].waddle==waddle)m->racerCount=SLED_PLAYERS[i].players;
 for(unsigned i=0;i<SLED_RACERS;i++)m->racers[i]=(SledRacer){.sled=1,.art=1,.x=SLED_START_X[i],.x2=SLED_START_X[i]};
 double speed=SLED_BOOST_SPEED;for(unsigned i=0;i<(unsigned)SLED_BOOST_SPEED&&i<64;i++){speed*=SLED_DECAY;m->speeds[m->speedCount++]=speed;}
}

void sled_model_players(SledModel *m,unsigned count,const char *const *players,const char *username){
 for(unsigned i=0;i<count&&i<m->racerCount;i++){
  char text[160];snprintf(text,sizeof text,"%s",players[i]);char *field[4]={text,NULL,NULL,NULL};
  for(unsigned f=1;f<4;f++){char *bar=field[f-1]?strchr(field[f-1],'|'):NULL;if(!bar)break;*bar=0;field[f]=bar+1;}
  SledRacer *r=&m->racers[i];size_t n=strlen(field[0]);if(n>=sizeof r->name)n=sizeof r->name-1;memcpy(r->name,field[0],n);r->name[n]=0;r->named=true;
  r->colour=field[1]?(unsigned)strtoul(field[1],NULL,10):0;r->sled=sled_model_sled(field[2]?(unsigned)strtoul(field[2],NULL,10):0);
  const char *user=field[3]?field[3]:field[0];
  bool same=strlen(user)==strlen(username);for(size_t k=0;same&&user[k];k++){char a=user[k],b=username[k];if(a>='a'&&a<='z')a-=32;if(b>='a'&&b<='z')b-=32;same=a==b;}
  if(same)m->me=(int)i;
 }
}

static void send(SledModel *m,double code){if(m->sendCount<SLED_MAX_SENDS)m->sends[m->sendCount++]=(SledSend){code,m->racers[m->me].y,m->gameTime};}

void sled_model_turn(SledModel *m,int direction){
 if(!m->started||m->me<0)return;
 SledRacer *me=&m->racers[m->me];
 if(me->crashed||me->jumping)return;
 me->x=fmax(0,fmin(180,me->x+direction*SLED_TURN_SPEED));
 if(m->currentX!=me->x){m->currentX=me->x;send(m,me->x+100);}
}

static void game_over(SledModel *m){
 m->over=true;m->place=0;
 for(unsigned i=0;i<m->finished;i++)if(!strcmp(m->finish[i],m->racers[m->me].name)){m->place=i<4?i+1:0;break;}
}
static void finish(SledModel *m,SledRacer *r){if(m->finished<SLED_RACERS){memcpy(m->finish[m->finished],r->name,sizeof r->name);m->finished++;}r->finished=true;}

static void move(SledModel *m,SledRacer *r,bool mine){
 double s=r->s;
 if(r->finished){s*=SLED_FINISH_DECAY;if(s<1){if(mine&&!r->stopped)game_over(m);r->stopped=true;s=0;}}
 else if(r->crashed){s*=SLED_CRASH_DECAY;if(s<1){r->crashed=false;r->art=1;s=0;}}
 else if(s<SLED_MAX_SPEED)s=fmin(SLED_MAX_SPEED,s+SLED_GRAVITY);
 else if(s>SLED_MAX_SPEED)s=fmax(SLED_MAX_SPEED,s*SLED_DECAY);
 else s=SLED_MAX_SPEED;
 if(r->stopped)return;
 r->s=s;r->y=js_round(r->y+SLED_TICK/40*s);
 int col=(int)js_round(r->x/20),row=(int)js_round(r->y/20);
 double ground=(row>=0&&row<(int)m->mapRows&&col>=0&&col<SLED_TILE_COLS?m->rows[row][col]:0)*10,h=r->h,mm=r->m;
 if(r->y>SLED_HILL_MAX)ground=100;
 if(ground>90){
  if(ground==100){if(mine&&!r->finished)send(m,0);if(!r->finished)finish(m,r);}
  else if(ground==110){if(mine&&!r->crashed){r->s=SLED_MAX_SPEED;r->crashed=true;r->art=5;send(m,1);}}
  else if(ground==120){if(mine){r->s=SLED_BOOST_SPEED;send(m,r->s+400);}}
  else if(ground==130){if(mine){r->s=10;send(m,r->s+400);h=10;}}
  ground=h;
 }else if(h>ground){if(mine&&!r->jumping)send(m,2);r->jumping=true;mm+=SLED_GRAVITY;h-=mm;}
 else{r->jumping=false;mm=0;h=ground;}
 r->h=h;r->m=mm;r->ground=ground;r->row=row;
 if(!r->crashed)r->art=r->x<r->x2?3:r->x>r->x2?2:r->jumping?4:1;
 r->x2=r->x;
}

void sled_model_step(SledModel *m){
 if(!m->started)return;
 m->gameTime+=SLED_TICK;
 for(unsigned i=0;i<m->racerCount;i++)if(m->racers[i].named)move(m,&m->racers[i],(int)i==m->me);
}

void sled_model_remote(SledModel *m,int id,double code,double y,double time){
 if(id<0||id>=(int)m->racerCount||id==m->me)return;
 SledRacer *r=&m->racers[id];double lag=(m->gameTime-time)*0.024;
 y+=lag*SLED_MAX_SPEED;
 if(code==0){if(!r->finished){finish(m,r);r->y=y;}}
 else if(code==1){r->s=SLED_MAX_SPEED;r->y=y;r->crashed=true;r->art=5;}
 else if(code==2)r->y=y;
 else if(code<400){r->x=code-100;r->y=y;}
 // mySpeed[lag]: a fractional age is no index in Flash, so the racer resumes maxSpeed.
 else{r->s=lag>=0&&lag==floor(lag)&&lag<m->speedCount?m->speeds[(unsigned)lag]:SLED_MAX_SPEED;r->y=y;}
}
