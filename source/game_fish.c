#include "game_controller.h"
#include "game_fish_model.h"
#include "game_fish_data.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {BUTTON_PLAY=1,BUTTON_HOW,BUTTON_EXIT,BUTTON_STORE,BUTTON_WORM,BUTTON_PAY,BUTTON_LEAVE};
enum {STAMP_SNACK=372,STAMP_SHOCK=374,STAMP_FISHTASTIC=376,STAMP_WORM=378,STAMP_CRAB=380,STAMP_AFISHIONADO=382,STAMP_GRAY=384,STAMP_MULLET=386,STAMP_FLY=388,STAMP_ACE=390};
#define ROOTS (sizeof FISH_ROOTS/sizeof *FISH_ROOTS)
typedef struct {
 GamePort *port;GameBank *bank;SWLScene *scene;FishModel model;
 SWLMatrix positions[ROOTS];char paths[ROOTS][40];
 unsigned watched[ROOTS],seed,stamps,screen;bool started,finished;
} Fishing;

static void game_over(Fishing *f);
static void empty_hook(Fishing *f);
static void crab_hit(Fishing *f);
static void mullet(Fishing *f);
static int root_index(const char *name){for(unsigned i=0;i<ROOTS;i++)if(!strcmp(name,FISH_ROOTS[i].name))return (int)i;return -1;}
static unsigned frame(Fishing *f,const char *name){int i=root_index(name);unsigned n=i>=0?swl_frame(f->scene,f->paths[i]):0;return n?n:1;}
static void target_path(Fishing *f,const char *path,unsigned n,bool playing){swl_target(f->scene,path,n,NULL,playing);}
static void target(Fishing *f,const char *name,unsigned n,bool playing){int i=root_index(name);if(i>=0)target_path(f,f->paths[i],n,playing);}
static void play(Fishing *f,const char *name){target(f,name,frame(f,name),true);}
static SWLMatrix *position(Fishing *f,const char *name){int i=root_index(name);return i>=0?&f->positions[i]:NULL;}
static void move_to(Fishing *f,const char *name,double x,double y){int i=root_index(name);if(i<0)return;f->positions[i].x=(float)x;f->positions[i].y=(float)y;swl_transform(f->scene,f->paths[i],f->positions[i]);}
static void move_y(Fishing *f,const char *name,double y){SWLMatrix *p=position(f,name);if(p)move_to(f,name,p->x,y);}
static double random01(Fishing *f){f->seed^=f->seed<<13;f->seed^=f->seed>>17;f->seed^=f->seed<<5;return (double)f->seed/4294967296.0;}
static double now(const Fishing *f){return f->model.ticks*(1000.0/24);}
static void sound(Fishing *f,const char *name){game_port_sound(f->port,name,100,false);}
static void stamp(Fishing *f,unsigned id){unsigned bit=1u<<((id-372)/2);if(f->stamps&bit)return;f->stamps|=bit;char value[12];snprintf(value,sizeof value,"%u",id);game_port_send(f->port,'s',"st#sse",1,(const char *[]){value});}
static const char *text(const char *key){for(unsigned i=0;i<sizeof FISH_TEXT/sizeof *FISH_TEXT;i++)if(!strcmp(FISH_TEXT[i].key,key))return FISH_TEXT[i].value;return "";}
static void label(Fishing *f,const char *path,const char *key){game_bank_text(f->bank,path,text(key));}
static void number(Fishing *f,const char *path,unsigned n){char value[20];snprintf(value,sizeof value,"%u",n);game_bank_text(f->bank,path,value);}
static void replace(const char *source,char *out,size_t cap,unsigned a,unsigned b){
 size_t used=0;for(const char *s=source;*s&&used+1<cap;s++){
  if(*s=='%'&&(s[1]=='0'||s[1]=='1')){char value[24];int n=snprintf(value,sizeof value,"%u",s[1]=='0'?a:b);s++;for(int i=0;i<n&&used+1<cap;i++)out[used++]=value[i];}
  else out[used++]=*s;
 }out[used]=0;
}
static bool box(Fishing *f,const char *path,float out[4]){
 SWLInstance pose;if(!swl_pose(f->scene,path,&pose))return false;
 unsigned source=movie_meta_source(game_bank_meta(f->bank),pose.symbol);const float *bounds=source==55?FISH_HITBOX:source==152?FISH_LINE_BOX[pose.frame==2?1:0]:NULL;if(!bounds)return false;
 out[0]=out[1]=INFINITY;out[2]=out[3]=-INFINITY;
 for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++){float px=bounds[x?2:0],py=bounds[y?3:1],sx=pose.matrix.a*px+pose.matrix.c*py+pose.matrix.x,sy=pose.matrix.b*px+pose.matrix.d*py+pose.matrix.y;out[0]=fminf(out[0],sx);out[1]=fminf(out[1],sy);out[2]=fmaxf(out[2],sx);out[3]=fmaxf(out[3],sy);}
 return true;
}
static bool hit(Fishing *f,const char *a,const char *b){float x[4],y[4];return box(f,a,x)&&box(f,b,y)&&x[0]<=y[2]&&x[2]>=y[0]&&x[1]<=y[3]&&x[3]>=y[1];}
static void level_up(Fishing *f){f->model.level++;f->model.speed-=500;f->model.levelBlock=false;swl_visible(f->scene,"root.puffle",false);}
static void watch(Fishing *f){
 for(unsigned i=0;i<ROOTS;i++){
  SWLInstance pose;if(!swl_pose(f->scene,f->paths[i],&pose))continue;
  unsigned n=pose.frame,last=f->watched[i];if(n==last)continue;f->watched[i]=n;if(last==UINT32_MAX)continue;const char *name=FISH_ROOTS[i].name;
  if(!strcmp(name,"hook")&&n==50)empty_hook(f);
  else if((!strcmp(name,"jfish1")||!strcmp(name,"jfish2"))&&n==2)move_y(f,name,250+random01(f)*200);
  else if(!strcmp(name,"finLeft")||!strcmp(name,"finRight")){unsigned side=!strcmp(name,"finLeft");if(n==2)f->model.sharkActive=true;if(n==101){move_y(f,side?"sharkRight":"sharkLeft",300+random01(f)*100);f->model.sharkMoving[side]=true;}}
  else if(!strcmp(name,"oneupText")&&n==11)move_to(f,name,1000,100);
  else if((!strcmp(name,"fishflee")||!strcmp(name,"grayfishflee"))&&n==23){target(f,name,1,false);move_to(f,name,900,position(f,name)->y);}
  else if(!strcmp(name,"crab1")||!strcmp(name,"crab2")){if(n==148)sound(f,"snip");if(n==149)crab_hit(f);}
  else if(!strcmp(name,"puffle")&&n==237){level_up(f);target(f,name,1,false);}
  else if(!strcmp(name,"bigFishApproach")){if(n==249)play(f,FISH_OBJECTS[1]);if(n==311)play(f,FISH_OBJECTS[6]);if(n==489)play(f,FISH_OBJECTS[3]);if(n==769)play(f,"bigFish");}
  else if(!strcmp(name,"bigFish")&&n==227)game_over(f);
 }
}
static void begin(Fishing *f){
 unsigned ticks=f->model.ticks;fish_model_init(&f->model,game_port_descriptor(f->port)->hand==321,ticks);f->started=true;
 for(unsigned i=0;i<ROOTS;i++)f->positions[i]=FISH_ROOTS[i].matrix;
 swl_visible(f->scene,"root.puffle",false);swl_visible(f->scene,"root.gameOver_mc",false);
 number(f,"root.lives_mc.lives",3);number(f,"root.icebox.fishNum",0);label(f,"root.icebox.fish","fish_txt");label(f,"root.prompt.newWorm","ingame_newworm");label(f,"root.gameOver_mc.gameOver","ui_gameover");label(f,"root.gameOver_mc.collectCoins.label","ingame_collectcoins");
}
static void end_level(Fishing *f,unsigned level,unsigned count,const char *tip){
 FishModel *m=&f->model;if(m->fishCaught!=count||m->level!=level)return;if(frame(f,"puffle")==1)play(f,"puffle");swl_visible(f->scene,"root.puffle",true);label(f,"root.puffle.obj.levelTip",tip);m->levelBlock=true;
}
static void choose(Fishing *f,unsigned stopwatch,unsigned item){
 FishModel *m=&f->model;double elapsed=now(f)-m->timer;if(elapsed<stopwatch||elapsed>stopwatch+200)return;
 const char *name=FISH_OBJECTS[item];
 if(item<=7||item==9||item==10||(item==8&&m->lives<3))play(f,name);
 else if(item==11||item==12){if(!m->sharkActive)play(f,name);}
 else if(item==13||item==14){if(!m->crabActive)play(f,name);}
 else if(item==15||item==16){unsigned side=item-15;if(m->kickerMoving[side])return;target_path(f,side?"root.kickerRight.obj":"root.kickerLeft.obj",(unsigned)floor(random01(f)+1.5),false);move_y(f,name,random01(f)*200+235);m->kickerMoving[side]=true;}
 else if(item>=17&&m->fishCaught<56&&m->gray)play(f,name);
}
static void chooser(Fishing *f){
 FishModel *m=&f->model;
 if(m->fishCaught<60&&!m->levelBlock&&m->running){
  if(now(f)-m->timer>=m->speed){m->timer=now(f);unsigned level=m->level>4?4:m->level;m->group=(unsigned)floor(random01(f)*(level==1?10:level==2?20:30))+(level==4?11:1);}
  for(unsigned i=0;i<FISH_GROUPS[m->group][1];i++){unsigned event=FISH_GROUPS[m->group][0]+i;choose(f,FISH_EVENTS[event][0],FISH_EVENTS[event][1]);}
 }
 end_level(f,1,5,"ingame_jellyfish");end_level(f,2,19,"ingame_sharks");end_level(f,3,37,"ingame_crabs");
 if(m->fishCaught==60&&frame(f,"bigFish")==1&&frame(f,"bigFishApproach")==1)play(f,"bigFishApproach");
}
static void line_scale(Fishing *f){
 FishModel *m=&f->model;SWLMatrix *line=position(f,"line");double y=line->y,my=m->mouseY,scale=line->d;
 if(y<my&&my<440){if(!m->lineCut||my<m->cutPoint+y)scale=(my-y)/100;}
 else if(my>=440&&!m->lineCut)scale=(440-y)/100;else if(y>my&&!m->lineCut)scale=0;
 line->d=(float)scale;swl_transform(f->scene,"root.line",*line);
}
static void hook_move(Fishing *f){
 FishModel *m=&f->model;double top=position(f,"line")->y+m->lineExtra,my=m->mouseY;move_y(f,"hook",my>=top&&my<440?my:my>=440?440:top);
 if(m->hookEmpty&&!m->isHit){if(my<=122)target(f,"hook",2,false);else{target(f,"hook",my<m->hookPrevious?4:my>m->hookPrevious?3:1,false);m->hookPrevious=my;}}
}
static void flee(Fishing *f,bool gray){f->model.consecutive=0;sound(f,"fishfleesound");SWLMatrix *hook=position(f,"hook");const char *name=gray?"grayfishflee":"fishflee";move_to(f,name,hook->x,hook->y);target(f,name,2,true);}
static void fish_hit(Fishing *f,bool gray){
 if((gray&&!f->model.gray)||frame(f,"hook")>=5)return;
 for(unsigned i=1;i<=(gray?4u:8u);i++){char name[20],path[48];snprintf(name,sizeof name,"%s%u",gray?"grayfish":"fish",i);snprintf(path,sizeof path,"root.%s.obj.hotspot",name);if(frame(f,name)>1&&hit(f,path,"root.hook.hotspot")){target(f,name,1,false);target(f,"hook",gray?7:5,false);f->model.hookEmpty=false;sound(f,"chomp");}}
}
static void fish_drop(Fishing *f){
 FishModel *m=&f->model;unsigned hook=frame(f,"hook");if(m->isHit||m->hookEmpty||(hook!=5&&hook!=7))return;bool gray=hook==7;
 if(m->mouseY<=122){
  unsigned block=m->iceboxBlock;fish_model_store(m,gray);if(gray&&m->grayCaught==15)stamp(f,STAMP_GRAY);else if(!gray&&m->consecutive==15)stamp(f,STAMP_FISHTASTIC);if(m->consecutive==45)stamp(f,STAMP_AFISHIONADO);
  target(f,"hook",2,false);if(block!=m->iceboxBlock)target(f,"icebox",frame(f,"icebox")+1,false);
  char path[72];snprintf(path,sizeof path,"root.icebox.fishblock%u",m->iceboxBlock);SWLInstance pose;unsigned n=swl_pose(f->scene,path,&pose)?pose.frame:1;target_path(f,path,n+1,false);
  if(gray){snprintf(path,sizeof path,"root.icebox.fishblock%u.fish%u",m->iceboxBlock,m->currentFish);target_path(f,path,2,false);}
  if(m->ticks-m->startTicks<=7200&&m->fishCaught>=63)stamp(f,STAMP_FLY);
  number(f,"root.icebox.fishNum",m->fishCaught);label(f,"root.icebox.fish",m->fishCaught==1?"ingame_fish":"ingame_fishies");sound(f,"fishicebox");
 }else{target(f,"hook",1,false);flee(f,gray);}m->hookEmpty=true;
}
static void line_cut(Fishing *f,const char *collider){
 FishModel *m=&f->model;m->lineCuts++;unsigned hook=frame(f,"hook");if(!strncmp(collider,"crab",4)&&(hook==5||hook==7))flee(f,hook==7);if(strncmp(collider,"shark",5))m->hookFalling=true;
 double cy=position(f,collider)->y;
 if(m->hookFalling){move_to(f,"hookFall",position(f,"hook")->x,position(f,"hookFall")->y);SWLInstance mask;if(swl_pose(f->scene,"root.hookFall.mask",&mask)){SWLMatrix transform={1,0,0,1,0,(float)(cy-m->mouseY)};swl_transform(f->scene,"root.hookFall.mask",transform);}}
 m->lineCut=m->isHit=true;swl_visible(f->scene,"root.hook",false);target(f,"hook",50,false);int index=root_index("hook");f->watched[index]=50;empty_hook(f);
 SWLMatrix *line=position(f,"line");line->d=(float)((cy-line->y)/100);swl_transform(f->scene,"root.line",*line);m->cutPoint=cy-line->y;
}
static void jelly_hit(Fishing *f){
 FishModel *m=&f->model;if(m->isHit)return;
 for(unsigned i=1;i<=2;i++){char name[12],path[40];snprintf(name,sizeof name,"jfish%u",i);snprintf(path,sizeof path,"root.%s.obj.hotspot",name);
  if(frame(f,name)>1&&(hit(f,path,"root.hook.hotspot")||hit(f,path,"root.line"))){m->isHit=true;unsigned hook=frame(f,"hook");if(hook==5||hook==7)flee(f,hook==7);m->jellyHits++;target(f,"hook",6,true);target(f,"line",2,false);swl_target(f->scene,"root.penguin",0,"electro",true);}}
}
static void shark_hit(Fishing *f){
 FishModel *m=&f->model;if(m->isHit)return;
 if(hit(f,"root.sharkLeft.hotspot","root.hook.hotspot")){line_cut(f,"sharkLeft");sound(f,"shark");}
 if(hit(f,"root.sharkRight.hotspot","root.hook.hotspot")){if(!m->hookEmpty)stamp(f,STAMP_SNACK);line_cut(f,"sharkRight");sound(f,"shark");}
}
static void crab_hit(Fishing *f){if(f->model.isHit)return;for(unsigned i=1;i<=2;i++){char name[12],path[32];snprintf(name,sizeof name,"crab%u",i);snprintf(path,sizeof path,"root.%s.hotspot",name);if(hit(f,path,"root.line"))line_cut(f,name);}}
static void kicker_hit(Fishing *f){
 for(unsigned side=0;side<2;side++)if(hit(f,side?"root.kickerRight.obj.hotspot":"root.kickerLeft.obj.hotspot","root.hook.hotspot")){unsigned hook=frame(f,"hook");if(hook==5||hook==7){target(f,"hook",1,false);flee(f,hook==7);hook_move(f);f->model.hookEmpty=true;}}
}
static void sharks(Fishing *f){
 FishModel *m=&f->model;SWLMatrix *hook=position(f,"hook");
 for(unsigned side=0;side<2;side++){const char *name=side?"sharkRight":"sharkLeft";SWLMatrix *p=position(f,name);double x=p->x,y=p->y;
  if(m->sharkMoving[side]){x+=side?-12:12;move_to(f,name,x,y);target(f,name,hypot(x-hook->x,y-hook->y)<150&&!m->lineCut?2:1,false);}
  if(side?x<-700:x>1500){move_to(f,name,side?1000:-100,y);m->sharkMoving[side]=false;m->sharkActive=false;}}
}
static void kickers(Fishing *f){
 for(unsigned side=0;side<2;side++){const char *name=side?"kickerRight":"kickerLeft";SWLMatrix *p=position(f,name);double x=p->x,y=p->y;if(f->model.kickerMoving[side]){x+=side?-4:4;move_to(f,name,x,y);}if(side?x<-300:x>1000){move_to(f,name,side?1000:-300,y);f->model.kickerMoving[side]=false;}}
}
static void line_mask(Fishing *f){
 FishModel *m=&f->model;SWLMatrix *fall=position(f,"hookFall");
 if(!m->lineCut){if(!m->hookFalling)move_y(f,"hookFall",m->mouseY);else{target(f,"hookFall",1,false);move_to(f,"hookFall",900,fall->y);m->hookFalling=false;}return;}
 if(fall->y<800){target(f,"hookFall",2,false);move_y(f,"hookFall",fall->y+20);}else{target(f,"hookFall",1,false);move_to(f,"hookFall",900,fall->y);m->hookFalling=false;}
}
static void oneup(Fishing *f){
 FishModel *m=&f->model;
 if(m->oneupAlpha==100&&frame(f,"hook")<50&&frame(f,"oneup")>1&&hit(f,"root.oneup.obj.hotspot","root.hook.hotspot")){m->oneupAlpha-=10;m->lives++;number(f,"root.lives_mc.lives",(unsigned)m->lives);label(f,"root.oneupText.mc.label","ingame_1UP");move_to(f,"oneupText",367,330);target(f,"oneupText",2,true);sound(f,"ding");}
 else if(m->oneupAlpha<100)m->oneupAlpha-=10;
 if(m->oneupAlpha<10){target(f,"oneup",1,false);m->oneupAlpha=100;}
 swl_color(f->scene,"root.oneup",(SWLColor){{1,1,1,(float)m->oneupAlpha/100},{0,0,0,0}});
}
static void big_fish(Fishing *f){
 if(frame(f,"bigFish")<=1||frame(f,"hook")!=5)return;
 if(frame(f,"bigFish")<165)target_path(f,"root.bigFish.obj",hit(f,"root.bigFish.obj.disthotspot","root.hook.hotspot")?2:1,false);
 if(!f->model.won&&hit(f,"root.bigFish.obj.hotspot","root.hook.hotspot")){target_path(f,"root",39,false);mullet(f);}
}
static void empty_hook(Fishing *f){if(f->model.lives==0)game_over(f);else{target(f,"prompt",2,false);label(f,"root.prompt.newWorm","ingame_newworm");}}
static void new_worm(Fishing *f,double extra){
 FishModel *m=&f->model;if(m->lives<=0||frame(f,"prompt")!=2||m->mouseY>122+extra)return;
 target(f,"hook",2,false);target(f,"prompt",1,false);swl_visible(f->scene,"root.hook",true);m->isHit=false;m->hookEmpty=true;m->hasLostLife=true;m->lives--;m->lineCut=false;number(f,"root.lives_mc.lives",(unsigned)m->lives);
}
static void score_text(Fishing *f,const char *base){
 char path[80],value[256];FishModel *m=&f->model;
 if(m->gray){char orange[120],gray[120];replace(text("ingame_yellowfishscore"),orange,sizeof orange,m->orangeFish,m->orangeFish*4);replace(text("ingame_grayfishscore"),gray,sizeof gray,m->grayFish,m->grayFish*8);snprintf(value,sizeof value,"%s\n%s",orange,gray);}
 else replace(text("ui_fishcaught"),value,sizeof value,m->fishCaught,0);
 snprintf(path,sizeof path,"%s.fishNum",base);game_bank_text(f->bank,path,value);snprintf(path,sizeof path,"%s.gameOver",base);label(f,path,m->won?"ingame_bigfish":"ui_gameover");snprintf(path,sizeof path,"%s.collectCoins.label",base);label(f,path,"ingame_collectcoins");
}
static void game_over(Fishing *f){
 FishModel *m=&f->model;if(m->over)return;if(!m->hasLostLife)stamp(f,STAMP_WORM);if(m->lineCuts==3)stamp(f,STAMP_CRAB);m->running=false;m->over=true;swl_visible(f->scene,"root.hook",false);swl_visible(f->scene,"root.gameOver_mc",true);if(m->gray)target(f,"gameOver_mc",2,false);score_text(f,"root.gameOver_mc");
}
static void mullet(Fishing *f){
 FishModel *m=&f->model;if(m->won)return;m->running=false;m->over=m->won=true;stamp(f,STAMP_MULLET);if(m->jellyHits==3)stamp(f,STAMP_SHOCK);if(m->lineCuts==3)stamp(f,STAMP_CRAB);if(m->grayCaught>=15&&!m->hasLostLife)stamp(f,STAMP_ACE);if(!m->hasLostLife)stamp(f,STAMP_WORM);if(m->ticks-m->startTicks<=7200&&m->fishCaught>=63)stamp(f,STAMP_FLY);score_text(f,"root#2#32");
}
static void *create(GamePort *port){Fishing *f=calloc(1,sizeof *f);if(f){f->port=port;f->seed=0x714f3e25u^game_port_descriptor(port)->player;f->model.gray=game_port_descriptor(port)->hand==321;f->model.mouseY=240;for(unsigned i=0;i<ROOTS;i++){snprintf(f->paths[i],sizeof f->paths[i],"root.%s",FISH_ROOTS[i].name);f->positions[i]=FISH_ROOTS[i].matrix;f->watched[i]=UINT32_MAX;}}return f;}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Fishing *f=game;f->bank=game_port_bank(f->port,0);f->scene=game_bank_scene(f->bank);if(!f->scene)return false;target_path(f,"root",2,false);
 uint32_t rgb=game_port_descriptor(f->port)->colourRGB;SWLColor colour={{0,0,0,1},{(float)(rgb>>16&255),(float)(rgb>>8&255),(float)(rgb&255),0}};
 for(unsigned i=0;i<sizeof FISH_BODY_PATHS/sizeof *FISH_BODY_PATHS;i++)swl_color(f->scene,FISH_BODY_PATHS[i],colour);
 for(unsigned i=0;i<3;i++){char path[32];snprintf(path,sizeof path,"root.hook.lure%u",i==0?155:i==1?159:162);swl_visible(f->scene,path,f->model.gray);}
 label(f,"root.start_screen.play_txt","title_play");label(f,"root.start_screen.instructions_txt","title_instructions");return true;
}
static void finish(Fishing *f){if(f->finished)return;f->finished=true;game_port_finish(f->port,f->started?fish_model_coins(&f->model):0);}
#ifdef GP_LAN_ONLY
static void input_trace(Fishing *f){
 if(f->model.ticks%6)return;
 FILE *log=fopen("sdmc:/3ds/gopenguin/fish-input.log","a");if(!log)return;
 fprintf(log,"tick=%u hook=%u caught=%u hit=%u",f->model.ticks,frame(f,"hook"),f->model.fishCaught,f->model.isHit);
 for(unsigned n=1;n<=8;n++){char name[16],path[40];float b[4];snprintf(name,sizeof name,"fish%u",n);snprintf(path,sizeof path,"root.%s.obj.hotspot",name);if(frame(f,name)>1&&box(f,path,b))fprintf(log," f%u=%.1f,%.1f",n,(b[0]+b[2])/2,(b[1]+b[3])/2);}
 fputc('\n',log);fclose(log);
}
#endif
static void tick(void *game){
 Fishing *f=game;if(f->finished||swl_frame(f->scene,"root")<3)return;
 unsigned before=swl_frame(f->scene,"root");f->model.ticks++;swl_tick(f->scene);unsigned current=swl_frame(f->scene,"root");
 if(current!=before){if(current==36)play(f,"fish2");if(current==38)begin(f);if(current==39)mullet(f);}
 if(current<38||!f->model.running){watch(f);if(f->model.won)score_text(f,"root#2#32");return;}
 FishModel *m=&f->model;chooser(f);line_scale(f);hook_move(f);
 if(position(f,"hook")->y>120){if(m->level>0){fish_hit(f,false);kicker_hit(f);fish_hit(f,true);}if(m->level>1)jelly_hit(f);if(m->level>2)shark_hit(f);}
 if(frame(f,"penguin")<10)target(f,"penguin",m->isHit?3:m->mouseY<=122?1:2,false);
 target_path(f,"root.penguin.wormCan",m->lives<1?2:1,false);
 number(f,"root.icebox.fishNum",m->fishCaught);big_fish(f);sharks(f);kickers(f);line_mask(f);oneup(f);watch(f);
#ifdef GP_LAN_ONLY
 input_trace(f);
#endif
}
static void store(Fishing *f){f->model.mouseY=80;fish_drop(f);new_worm(f,30);}
static void frame_input(void *game,float seconds,const GameInput *input){
 Fishing *f=game;unsigned clicked=game_port_clicked(f->port);if(clicked==BUTTON_EXIT||clicked==BUTTON_PAY){finish(f);return;}if(clicked==BUTTON_LEAVE){game_port_leave(f->port);return;}
 if(!f->started){if(swl_frame(f->scene,"root")!=2)return;if(clicked==BUTTON_PLAY||(input->down&GAME_KEY_A))target_path(f,"root",3,true);if(clicked==BUTTON_HOW){f->screen=1;target_path(f,"root.start_screen",2,false);}return;}
 if(!f->model.running)return;
 if(clicked==BUTTON_STORE||clicked==BUTTON_WORM||(input->down&GAME_KEY_B)){store(f);return;}
 double direction=input->stickY+(input->held&GAME_KEY_UP?1:0)-(input->held&GAME_KEY_DOWN?1:0);f->model.mouseY=fmax(0,fmin(480,f->model.mouseY-direction*200*seconds));
 if(input->down&GAME_KEY_A){fish_drop(f);new_worm(f,30);}
}
static void touch(void *game,GameTouch phase,float x,float y){
 (void)x;Fishing *f=game;if(!f->model.running||phase==GAME_TOUCH_UP)return;f->model.mouseY=fmax(0,fmin(480,(y+68)/.7));if(phase==GAME_TOUCH_DOWN){fish_drop(f);new_worm(f,0);}
}
static void close_game(void *game){finish(game);}
static bool lower_view(void *game,SWLMatrix *view){Fishing *f=game;*view=f->model.running?(SWLMatrix){.7f,0,0,.7f,160-.7f*366.95f,-68}:(SWLMatrix){1,0,0,1,0,0};return true;}
static unsigned lower_frame(void *game,unsigned bank){Fishing *f=game;return bank?0:f->model.running?38:40;}
static void draw_bottom(void *game){
 Fishing *f=game;GamePort *p=f->port;FishModel *m=&f->model;
 if(m->running){
  game_port_rect(p,0,0,320,30,0x003366);char status[64];snprintf(status,sizeof status,"Fish %u  |  Worms %d",m->fishCaught,m->lives);game_port_text(p,97,9,10,0xffffff,GAME_ALIGN_LEFT,status);
  bool worm=frame(f,"prompt")==2;game_port_button(p,worm?BUTTON_WORM:BUTTON_STORE,3,3,89,25,worm?"New worm":"Store fish",true);game_port_button(p,BUTTON_EXIT,264,3,53,25,"Exit",true);
  return;
 }
 game_port_rect(p,0,0,320,30,0x003366);game_port_text(p,12,9,14,0xffffff,GAME_ALIGN_LEFT,"Ice Fishing");game_port_button(p,BUTTON_EXIT,264,3,53,25,"Exit",true);
 if(!f->started&&swl_frame(f->scene,"root")>=3){game_port_text(p,160,92,12,0x003366,GAME_ALIGN_CENTER,"Getting ready...");return;}
 if(!f->started){
  if(f->screen){game_port_rect(p,7,37,306,99,0xffffff);game_port_text(p,160,46,11,0x003366,GAME_ALIGN_CENTER,"Drag the hook to catch a fish.");game_port_text(p,160,68,11,0x003366,GAME_ALIGN_CENTER,"Store fish above the ice (B).");game_port_text(p,160,90,11,0x003366,GAME_ALIGN_CENTER,"Keep a yellow fish for the Mullet.");game_port_text(p,160,112,10,0x003366,GAME_ALIGN_CENTER,"Circle Pad: depth. A: drop. B: store.");}
  if(m->gray){game_port_rect(p,7,141,306,17,0xffffff);game_port_text(p,160,143,10,0x003366,GAME_ALIGN_CENTER,"Special rod: gray fish enabled");}
  game_port_button(p,BUTTON_PLAY,191,163,122,33,f->screen?"Start game":"Play",true);game_port_button(p,BUTTON_HOW,191,203,122,30,"Instructions",true);return;
 }
 if(!m->over){game_port_text(p,160,92,12,0x003366,GAME_ALIGN_CENTER,"Getting ready...");return;}
 game_port_rect(p,7,45,306,121,0xffffff);game_port_text(p,160,65,15,0x003366,GAME_ALIGN_CENTER,m->won?"You caught the Mullet!":"Game over");char score[96];snprintf(score,sizeof score,"Yellow %u  |  Gray %u",m->orangeFish,m->grayFish);game_port_text(p,160,99,12,0x003366,GAME_ALIGN_CENTER,score);snprintf(score,sizeof score,"%u coins%s",fish_model_coins(m),m->won?" (includes Mullet bonus)":"");game_port_text(p,160,126,12,0x003366,GAME_ALIGN_CENTER,score);
 if(!game_port_descriptor(p)->online)game_port_text(p,160,154,10,0x003366,GAME_ALIGN_CENTER,"Offline preview: no coins are paid.");
 if(!f->finished)game_port_button(p,BUTTON_PAY,85,185,150,35,"Collect coins",true);
 else if(!game_port_paid(p)&&*game_port_status(p))game_port_button(p,BUTTON_LEAVE,45,165,230,28,"Return without waiting",true);
}
const FishModel *fish_game_model(const GamePort *port){if(!port||strcmp(game_port_descriptor(port)->key,"fish"))return NULL;const Fishing *f=game_port_game((GamePort *)port);return &f->model;}
const GameController GAME_FISH_CONTROLLER={.key="fish",.kind=GAME_SOLO,.room=904,.rate=24,.banks={"games/fish.gpk"},.order={0,1,2},.create=create,.destroy=destroy,.ready=ready,.frame=frame_input,.tick=tick,.touch=touch,.close=close_game,.draw_bottom=draw_bottom,.lower_view=lower_view,.lower_frame=lower_frame};
