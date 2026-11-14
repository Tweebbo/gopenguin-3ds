// Treasure Hunt controller: TreasureHunt.swf at the Captain's Quarters tables, with Waddle's
// z-messages (gz, jz, uz, sz, zm, zo, cz, lz) as in the web TreasureHunt (src/games/treasure.ts).
// Player 1 digs rows, player 2 columns, 12 turns; a square shows its treasure once dug twice.
#include "game_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TH_SIZE 10
#define TH_TURNS 12
#define TH_TILE 21.0f
#define TH_COVER 48u
/* Lower screen: the board and its dig arrows at 0.8 under a 28 px status strip (placer at stage 59,111.55). */
#define TH_SCALE 0.8f
#define TH_PX 59.0f
#define TH_PY 111.55f
#define TH_OX 28.8f
#define TH_OY (-40.0f)
enum {BUTTON_CLOSE=1,BUTTON_YES,BUTTON_NO};

typedef struct {unsigned frame;int delay;bool active,shown;} Cover; /* shown: frame pushed to the scene */
typedef struct {
 GamePort *port;SWLScene *scene;GameBank *bank;
 char names[2][40];int seat,turn;bool started,over,leaving,asking,snapshot,joined,syncFailed,rareShown;
 unsigned map[TH_SIZE][TH_SIZE],digs[TH_SIZE][TH_SIZE],gems[3][2],gemCount,gemValue,coinValue;
 bool used[2][TH_SIZE];unsigned usedCount;
 Cover cover[2][TH_SIZE][TH_SIZE]; /* [0] st, [1] dst */
 unsigned sand[2][TH_SIZE];          /* sand animation frame per line (0 = idle) */
 int cursorDir,cursor;float syncAt;unsigned closeTicks;char notice[96];
 unsigned sandShown[2][TH_SIZE]; /* sand frames already applied by apply_animations() */
} Hunt;

static bool number(const char *text,unsigned max,unsigned *out){if(!text||!*text)return false;unsigned n=0;for(const unsigned char *p=(const unsigned char *)text;*p;p++){if(*p<'0'||*p>'9'||n>max/10||(n==max/10&&(unsigned)(*p-'0')>max%10))return false;n=n*10+(unsigned)(*p-'0');}*out=n;return true;}
static void target(Hunt *h,const char *path,unsigned frame,const char *label){if(h->scene)swl_target(h->scene,path,frame,label,false);}
/** Board clips whose frame draws nothing (empty slots, idle sand, a fully dug cover) are also hidden,
 *  so the scene preparation and the two screen draws skip them; Old 3DS cost grows with every instance. */
static void target_slot(Hunt *h,const char *path,unsigned frame,bool empty){if(!h->scene)return;swl_target(h->scene,path,frame,NULL,false);swl_visible(h->scene,path,!empty);}
static const char *myDir(const Hunt *h){return h->seat==0?"right":"down";}
static bool can_dig(const Hunt *h,int dir,int n){return h->snapshot&&h->started&&!h->over&&h->seat==h->turn&&h->usedCount<TH_TURNS&&dir==h->seat&&n>=0&&n<TH_SIZE&&!h->used[dir][n];}

/** The treasure frame of a square, or NULL: coins always, gems once part of them is dug out (updateGemStatus). */
static const char *map_label(const Hunt *h,unsigned r,unsigned c){
 unsigned v=h->map[r][c];if(v==1)return h->digs[r][c]==2?"coin":NULL;if(v!=2&&v!=4)return NULL;
 bool seen=h->digs[r][c]==2||h->digs[r+1][c]==2||h->digs[r][c+1]==2||h->digs[r+1][c+1]==2;
 return seen?(v==4?"gemrare":"gem"):NULL;
}

static void totals(const Hunt *h,unsigned *coins,unsigned *gems,bool *rare,unsigned *score){
 *coins=*gems=0;*rare=false;
 for(unsigned r=0;r<TH_SIZE;r++)for(unsigned c=0;c<TH_SIZE;c++)if(h->map[r][c]==1&&h->digs[r][c]==2)(*coins)++;
 for(unsigned g=0;g<h->gemCount;g++){unsigned r=h->gems[g][0],c=h->gems[g][1];if(h->digs[r][c]==2&&h->digs[r+1][c]==2&&h->digs[r][c+1]==2&&h->digs[r+1][c+1]==2){(*gems)++;if(h->map[r][c]==4)*rare=true;}}
 *score=*gems*h->gemValue+*coins*h->coinValue+(*rare?h->gemValue*3:0);
}

static void update_text(Hunt *h){
 if(!h->scene)return;
 unsigned coins,gems,score;bool rare;totals(h,&coins,&gems,&rare,&score);char text[40];
 snprintf(text,sizeof text,"Coins: %u",coins);game_bank_text(h->bank,"root.gameMC.coinsFound",text);
 snprintf(text,sizeof text,"Fragments: %u",gems);game_bank_text(h->bank,"root.gameMC.gemsFound",text);
 snprintf(text,sizeof text,"Total: %u",score);game_bank_text(h->bank,"root.gameMC.totalFound",text);
 if(rare&&!h->rareShown){h->rareShown=true;if(h->scene)swl_target(h->scene,"root.gameMC.rareGemFound",2,NULL,true);}
 for(unsigned s=0;s<2;s++){
  char path[40],field[64],upper[40];snprintf(path,sizeof path,"root.gameMC.player%u_mc",s);
  target(h,path,1,h->names[s][0]?(h->started&&!h->over&&h->turn==(int)s?"Turn":"Wait"):NULL);
  if(h->names[s][0]){size_t i=0;for(;h->names[s][i]&&i<sizeof upper-1;i++)upper[i]=(char)(h->names[s][i]>='a'&&h->names[s][i]<='z'?h->names[s][i]-32:h->names[s][i]);upper[i]=0;}
  else snprintf(upper,sizeof upper,"Waiting for Player");
  snprintf(field,sizeof field,"%s.anim.name_txt",path);game_bank_text(h->bank,field,upper);
  snprintf(field,sizeof field,"%s.name1_txt",path);game_bank_text(h->bank,field,upper);
 }
}

/** Board slots: treasure under the sand, the shine over a half-dug square, and the dig buttons. */
static void update_board(Hunt *h){
 if(!h->scene)return;
 char path[48],child[52];
 for(unsigned r=0;r<TH_SIZE;r++)for(unsigned c=0;c<TH_SIZE;c++){
  const char *label=map_label(h,r,c);snprintf(path,sizeof path,"root.gameMC.placer.m%u_%u",r,c);snprintf(child,sizeof child,"%s.m",path);
  target_slot(h,path,label?2:1,!label);
  if(label)target(h,child,0,label);
  bool shine=h->digs[r][c]==1&&h->map[r][c]!=0;snprintf(path,sizeof path,"root.gameMC.placer.d%u_%u",r,c);snprintf(child,sizeof child,"%s.d",path);
  target_slot(h,path,shine?2:1,!shine);
  if(shine)target(h,child,2,NULL);
 }
 for(unsigned n=0;n<TH_SIZE;n++){
  snprintf(path,sizeof path,"root.gameMC.placer.bd%u",n);target(h,path,can_dig(h,1,(int)n)?2:1,NULL);
  snprintf(path,sizeof path,"root.gameMC.placer.br%u",n);target(h,path,can_dig(h,0,(int)n)?2:1,NULL);
 }
}

static void set_cover(Hunt *h,unsigned layer,unsigned r,unsigned c,unsigned frame){
 char path[40];snprintf(path,sizeof path,"root.gameMC.placer.%s%u%02u",layer?"dst":"st",(r+1)%10,c+1);target_slot(h,path,frame,frame>=TH_COVER);
}

static void dig(Hunt *h,int dir,unsigned n,bool animate){
 if(dir<0||dir>1||n>=TH_SIZE||h->used[dir][n])return;
 h->used[dir][n]=true;unsigned turn=h->usedCount++;
 for(unsigned i=0;i<TH_SIZE;i++){
  unsigned r=dir==0?n:i,c=dir==0?i:n,layer=h->digs[r][c]++?1:0;
  if(!layer)set_cover(h,1,r,c,1); /* the lower sand layer only shows once the upper one is dug */
  if(animate)h->cover[layer][r][c]=(Cover){1,(int)i,true,false};else set_cover(h,layer,r,c,TH_COVER);
 }
 char path[48];snprintf(path,sizeof path,"root.gameMC.player%uSpades.spade%u",turn%2+1,turn/2+1);target(h,path,2,NULL);
 if(animate){h->sand[dir][n]=1;game_port_sound(h->port,"sound_dig",60,false);}
 h->turn=(int)(h->usedCount%2);
 update_text(h);update_board(h);
}

static void leave(Hunt *h,bool sendLeave){
 if(h->leaving)return;
 h->leaving=true;if(sendLeave)game_port_send(h->port,'z',"lz",0,NULL);
 game_port_leave(h->port);
}

/** MAP_WIDTH, MAP_HEIGHT, coins, gems, turns, gem value, coin value, gem corners, map (row by row). */
static bool load(Hunt *h,unsigned count,char *const *args){
 if(count<9)return false;
 unsigned w,ht,gems,v;if(!number(args[0],99,&w)||!number(args[1],99,&ht)||w!=TH_SIZE||ht!=TH_SIZE||!number(args[3],3,&gems))return false;
 h->gemValue=number(args[5],1000,&v)?v:25;h->coinValue=number(args[6],1000,&v)?v:1;
 char corners[64];snprintf(corners,sizeof corners,"%s",args[7]);h->gemCount=0;
 for(char *save=NULL,*t=strtok_r(corners,",",&save);t&&h->gemCount<gems;){unsigned r,c;char *u=strtok_r(NULL,",",&save);if(!u||!number(t,8,&r)||!number(u,8,&c))return false;h->gems[h->gemCount][0]=r;h->gems[h->gemCount][1]=c;h->gemCount++;t=strtok_r(NULL,",",&save);}
 const char *p=args[8];
 for(unsigned i=0;i<TH_SIZE*TH_SIZE;i++){if(*p<'0'||*p>'4')return false;h->map[i/TH_SIZE][i%TH_SIZE]=(unsigned)(*p-'0');p++;if(i+1<TH_SIZE*TH_SIZE){if(*p!=',')return false;p++;}}
 return *p==0;
}

static int dir_of(const char *dir){return !strcmp(dir,"right")?0:!strcmp(dir,"down")?1:-1;}

static bool packet(void *game,const char *command,unsigned count,char *const *args){
 Hunt *h=game;
 if(!strcmp(command,"gz")){
  for(unsigned s=0;s<2;s++)snprintf(h->names[s],sizeof h->names[s],"%s",count>s?args[s]:"");
  if(count>=17){
   if(!load(h,count-2,args+2))return false;
   h->started=true;
   char names[512],dirs[256],nums[128];snprintf(names,sizeof names,"%s",args[14]);snprintf(dirs,sizeof dirs,"%s",args[15]);snprintf(nums,sizeof nums,"%s",args[16]);
   char *a=NULL,*b=NULL,*c=NULL,*name=strtok_r(names,",",&a),*dir=strtok_r(dirs,",",&b),*num=strtok_r(nums,",",&c);
   while(name&&dir&&num){unsigned n;if(number(num,TH_SIZE-1,&n))dig(h,dir_of(dir),n,false);name=strtok_r(NULL,",",&a);dir=strtok_r(NULL,",",&b);num=strtok_r(NULL,",",&c);}
  }else if(h->seat>=0&&!h->joined){h->joined=true;if(!game_port_send(h->port,'z',"jz",0,NULL))h->syncFailed=true;}
  h->snapshot=true;update_text(h);update_board(h);return true;
 }
 if(!strcmp(command,"jz")){unsigned seat;if(count!=1||!number(args[0],1,&seat))return false;if(h->seat>=0)h->seat=(int)seat;update_board(h);return true;}
 if(!strcmp(command,"uz")){unsigned seat;if(count<1||count>2||!number(args[0],1,&seat))return false;snprintf(h->names[seat],sizeof h->names[seat],"%s",count>1?args[1]:"");update_text(h);return true;}
 if(!strcmp(command,"sz")){
  if(count<11)return false;
  for(unsigned s=0;s<2;s++)snprintf(h->names[s],sizeof h->names[s],"%s",args[s]);
  if(!load(h,count-2,args+2))return false;
  h->started=true;h->turn=0;h->snapshot=true;update_text(h);update_board(h);return true;
 }
 if(!strcmp(command,"zm")){unsigned n;if(count!=3||!number(args[2],TH_SIZE-1,&n))return false;if(!h->over)dig(h,dir_of(args[1]),n,true);return true;}
 if(!strcmp(command,"zo")){
  if(count<3)return false;
  unsigned score=0,balance=0;number(args[2],0x7fffffffu,&score);
  if(h->over)return true;
  h->over=true;update_text(h);update_board(h);
  const GameHost *host=game_port_host(h->port);
  if(count>=4&&number(args[3],0x7fffffffu,&balance)&&host->coins)host->coins(host->user,balance);
  snprintf(h->notice,sizeof h->notice,h->seat<0?"The treasure hunt is over.":score?"You found %u coins of treasure!":"No treasure this time.",score);
  h->closeTicks=72; /* the original shows the result after 3 seconds */
  return true;
 }
 if(!strcmp(command,"cz")){h->over=true;snprintf(h->notice,sizeof h->notice,"%.63s has quit the game.",count?args[0]:"Your partner");const GameHost *host=game_port_host(h->port);if(host->notice)host->notice(host->user,h->notice);leave(h,false);return true;}
 return false;
}

static void *create(GamePort *port){
 Hunt *h=calloc(1,sizeof *h);if(!h)return NULL;h->port=port;
 int seat=game_port_descriptor(port)->seat;h->seat=seat==99||seat<1?-1:seat-1;h->cursor=0;h->gemValue=25;h->coinValue=1;return h;
}
static void destroy(void *game){free(game);}
static bool ready(void *game){
 Hunt *h=game;h->bank=game_port_bank(h->port,0);h->scene=game_bank_scene(h->bank);if(!h->scene)return false;
 /* Frame 48 is empty: the lower layer stays hidden under the upper one until its first dig (halves the board's quads). */
 for(unsigned r=0;r<TH_SIZE;r++)for(unsigned c=0;c<TH_SIZE;c++){set_cover(h,0,r,c,1);set_cover(h,1,r,c,TH_COVER);}
 for(unsigned n=0;n<TH_SIZE;n++){char path[40];snprintf(path,sizeof path,"root.gameMC.placer.sd%u",n);target_slot(h,path,1,true);snprintf(path,sizeof path,"root.gameMC.placer.sr%u",n);target_slot(h,path,1,true);}
 for(unsigned k=1;k<=6;k++)for(unsigned s=1;s<=2;s++){char path[48];snprintf(path,sizeof path,"root.gameMC.player%uSpades.spade%u",s,k);target(h,path,1,NULL);}
 update_text(h);update_board(h);
 h->syncAt=game_port_clock(h->port);h->syncFailed=!game_port_send(h->port,'z',"gz",1,(const char *[]){""});
 return true;
}

static void tick(void *game){
 Hunt *h=game;if(!h->scene)return;
 /* Deferred: several catch-up ticks in one frame share a single scene preparation at draw time
  * (681 timelines; the two name animations alone would otherwise re-prepare the scene each tick). */
 swl_tick_deferred(h->scene);
 if(h->syncFailed||(!h->snapshot&&game_port_clock(h->port)-h->syncAt>10)){const GameHost *host=game_port_host(h->port);if(host->notice)host->notice(host->user,"Could not synchronize the table. Please join again.");leave(h,true);return;}
 /* pcN_right/pcN_down: one square per frame, each sand layer playing out to frame 48. */
 /* Animation state only; apply_animations() pushes it to the scene once per frame, so catch-up
  * ticks never prepare the board twice in one frame. */
 for(unsigned layer=0;layer<2;layer++)for(unsigned r=0;r<TH_SIZE;r++)for(unsigned c=0;c<TH_SIZE;c++){
  Cover *k=&h->cover[layer][r][c];if(!k->active)continue;
  if(k->delay>0){k->delay--;continue;}
  if(++k->frame>=TH_COVER){k->frame=TH_COVER;k->active=false;}
  k->shown=false;
 }
 for(unsigned d=0;d<2;d++)for(unsigned n=0;n<TH_SIZE;n++)if(h->sand[d][n]&&++h->sand[d][n]>11)h->sand[d][n]=0;
 if(h->closeTicks&&!--h->closeTicks){const GameHost *host=game_port_host(h->port);if(host->notice)host->notice(host->user,h->notice);leave(h,false);}
}


/** Pushes the cover and sand frames computed by tick() to the scene, once per frame before drawing. */
static void apply_animations(Hunt *h){
 if(!h->scene)return;
 for(unsigned layer=0;layer<2;layer++)for(unsigned r=0;r<TH_SIZE;r++)for(unsigned c=0;c<TH_SIZE;c++){
  Cover *k=&h->cover[layer][r][c];if(k->shown||(!k->active&&!k->frame))continue;
  if(!k->active||k->delay<=0)set_cover(h,layer,r,c,k->frame?k->frame:1);
  k->shown=true;
 }
 for(unsigned d=0;d<2;d++)for(unsigned n=0;n<TH_SIZE;n++){
  unsigned want=h->sand[d][n];if(want==h->sandShown[d][n])continue;h->sandShown[d][n]=want;
  char path[40],child[44];snprintf(path,sizeof path,"root.gameMC.placer.%s%u",d?"sd":"sr",n);snprintf(child,sizeof child,"%s.a",path);
  /* Frames 1-10 of the sand line, then hidden (as the original sand animation). */
  if(!want||want>10){target_slot(h,path,1,true);continue;}
  target_slot(h,path,2,false);target(h,child,want,NULL);
 }
}

static void send_dig(Hunt *h,int n){
 if(!can_dig(h,h->seat,n))return;
 char name[24],num[8];snprintf(name,sizeof name,"%sbutton%d_mc",myDir(h),n);snprintf(num,sizeof num,"%d",n);
 game_port_send(h->port,'z',"zm",3,(const char *[]){name,myDir(h),num});
}

static void close_game(void *game){
 Hunt *h=game;if(h->leaving)return;
 if(!h->started||h->over||h->seat<0){leave(h,!h->over);return;}
 h->asking=true;
}

static void frame(void *game,float seconds,const GameInput *input){
 Hunt *h=game;(void)seconds;unsigned clicked=game_port_clicked(h->port);
 if(h->asking){if(clicked==BUTTON_YES||(input->down&GAME_KEY_A))leave(h,true);else if(clicked==BUTTON_NO||(input->down&GAME_KEY_B))h->asking=false;return;}
 if(clicked==BUTTON_CLOSE){close_game(h);return;}
 /* D-Pad picks the next free line of your direction; A digs it. */
 bool back=h->seat==0?(input->down&GAME_KEY_UP):(input->down&GAME_KEY_LEFT),fwd=h->seat==0?(input->down&GAME_KEY_DOWN):(input->down&GAME_KEY_RIGHT);
 if(back&&h->cursor>0)h->cursor--;
 if(fwd&&h->cursor<TH_SIZE-1)h->cursor++;
 if(input->down&GAME_KEY_A)send_dig(h,h->cursor);
}

static void draw_bottom(void *game){
 Hunt *h=game;GamePort *p=h->port;
 if(h->asking){
  game_port_rect(p,0,0,320,240,0x003366);
  game_port_text(p,160,70,13,0xffffff,GAME_ALIGN_CENTER,"Are you sure you want to quit this game?");
  game_port_button(p,BUTTON_YES,50,120,100,40,"Yes",true);game_port_button(p,BUTTON_NO,170,120,100,40,"No",true);return;
 }
 bool mine=h->snapshot&&h->started&&!h->over&&h->seat==h->turn;
 const char *state=!h->started?(h->seat<0?"Watching":"Waiting for a partner..."):h->over?"Game over":mine?(h->seat==0?"Your turn: dig a row":"Your turn: dig a column"):h->seat<0?"Watching":"Your partner is digging";
 game_port_rect(p,0,0,320,28,0x003366);game_port_text(p,8,7,12,0xffdd33,GAME_ALIGN_LEFT,state);
 game_port_button(p,BUTTON_CLOSE,248,3,68,22,"Close",true);
 if(mine){
  /* The D-Pad cursor: a yellow bar beside the row or above the column. */
  float s=TH_SCALE,x0=TH_OX+TH_PX*s,y0=TH_OY+TH_PY*s,t=TH_TILE*s;
  if(h->seat==0)game_port_rect(p,x0-t-4,y0+h->cursor*t+2,2,t-4,0xffdd33);
  else game_port_rect(p,x0+h->cursor*t+2,y0-t-4,t-4,2,0xffdd33);
 }
}

static void touch(void *game,GameTouch phase,float x,float y){
 Hunt *h=game;if(phase!=GAME_TOUCH_DOWN||h->asking||y<30)return;
 float sx=(x-TH_OX)/TH_SCALE-TH_PX,sy=(y-TH_OY)/TH_SCALE-TH_PY;int col=(int)(sx/TH_TILE),row=(int)(sy/TH_TILE);
 if(sx<0)col=-1;
 if(sy<0)row=-1;
 /* Tap an arrow, or anywhere in your row/column of the board. */
 if(h->seat==0&&row>=0&&row<TH_SIZE&&sx>=-TH_TILE){h->cursor=row;send_dig(h,row);}
 else if(h->seat==1&&col>=0&&col<TH_SIZE&&sy>=-TH_TILE){h->cursor=col;send_dig(h,col);}
}
/* Top screen: the whole 300x365 window; lower screen: the board under the status strip. */
/* Called just before each screen draws the bank: the animation frames computed by the ticks are
 * applied here, after every catch-up tick, so the scene is prepared once per frame. */
static bool bank_view(void *game,unsigned bank,SWLMatrix *local){(void)bank;(void)local;apply_animations(game);return true;}
static bool top_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){0.6575f,0,0,0.6575f,101.4f,0};return true;}
static bool lower_view(void *game,SWLMatrix *view){(void)game;*view=(SWLMatrix){TH_SCALE,0,0,TH_SCALE,TH_OX,TH_OY};return true;}

const GameController GAME_TREASURE_CONTROLLER={
 .key="treasurehunt",.kind=GAME_TABLE,.stageWidth=300,.stageHeight=365,.backgroundRGB=0x00609c,.banks={"games/treasurehunt.gpk"},.order={0,1,2},
 .create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.packet=packet,
 .close=close_game,.draw_bottom=draw_bottom,.touch=touch,.top_view=top_view,.lower_view=lower_view,.bank_view=bank_view,
};
