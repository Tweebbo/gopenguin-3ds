#include "game_controller.h"
#include "game_aqua.h"
#include "game_aqua_model.h"
#include "game_rescue.h"
#include "game_rescue_model.h"
#include "game_waves.h"
#include "game_waves_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned fullPanels;static bool visibleClaw;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static void rect(void *user,float x,float y,float w,float h,uint32_t rgb){(void)user;(void)rgb;if(x<=0&&y<=0&&w>=320&&h>=240)fullPanels++;}
static void button(void *user,float x,float y,float w,float h,const char *label,bool pressed,bool enabled){(void)user;(void)pressed;(void)enabled;assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);if(!strcmp(label,"Claw (A)")){assert(y==220&&h==20);visibleClaw=true;}}
static void update(GamePort *p,GameInput input){game_port_update(p,0,&input);}
static void tick(GamePort *p,unsigned n){for(unsigned i=0;i<n;i++)game_port_update(p,1.0f/24,&(GameInput){0});}
static GamePort *open_game(const char *key){
 GameDescriptor d={.returnRoom=100,.colourRGB=0x0099ff};snprintf(d.key,sizeof d.key,"%s",key);
 GameHost h={.resource=resource,.rect=rect,.button=button};GamePort *p=game_port_open(&d,&h);assert(p);tick(p,3);
 assert(game_port_session(p)==GAME_SESSION_PLAYING);assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);return p;
}
static void tap(GamePort *p,float x,float y){game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);update(p,(GameInput){0});}
static SWLMatrix view(GamePort *p){SWLMatrix v;const GameController *c=game_port_controller(p);assert(c->lower_view&&c->lower_view(game_port_game(p),&v));float scale=!strcmp(c->key,"waves")?.5f:.8f;assert(v.a==scale&&v.d==scale);fullPanels=0;game_port_draw_bottom(p);assert(fullPanels==0);return v;}
static void zoom_clamps(GamePort *p,bool aqua){
 void *g=game_port_game(p);AquaModel *a=aqua?game_aqua_model(g):NULL;RescueModel *r=aqua?NULL:game_rescue_model(g);
 double x=aqua?a->x:r->x,y=aqua?a->y:r->y;
 if(aqua){a->x=0;a->y=0;}else{r->x=0;r->y=0;}
 SWLMatrix v=view(p);assert(v.x==0&&v.y==28);assert(fabsf(320/v.a-400)<.001f&&fabsf(192/v.d-240)<.001f);
 if(aqua){a->x=a->width*80;a->y=a->height*80;}else{const RescueLevelData *l=rescue_model_level(r);r->x=l->width-1;r->y=l->height-1;}
 v=view(p);assert(fabsf(v.x+288)<.001f&&fabsf(v.y+164)<.001f);
 if(aqua){a->x=x;a->y=y;}else{r->x=(unsigned)x;r->y=(unsigned)y;}
}
static void instance_center(GamePort *p,unsigned bank,const char *path,float *x,float *y){SWLInstance in;assert(swl_instance(game_bank_scene(game_port_bank(p,bank)),path,&in));*x=in.matrix.x;*y=in.matrix.y;}
static unsigned emits;
static bool emit(void *user,const SWLArt *a,const SWLMatrix *m,const SWLColor *c){(void)user;(void)a;(void)c;assert(isfinite(m->x)&&isfinite(m->y));emits++;return true;}
static bool mask(void *user,unsigned n){(void)user;(void)n;return true;}
static void art(GamePort *p){
 SWLMatrix v=view(p);unsigned banks=0;const GameController *c=game_port_controller(p);
 for(unsigned i=0;i<GAME_PORT_BANKS;i++){GameBank *b=game_port_bank(p,i);SWLMatrix local={1,0,0,1,0,0};if(!b||(c->bank_view&&!c->bank_view(game_port_game(p),i,&local)))continue;
  emits=0;assert(swl_render(game_bank_scene(b),swl_matrix_multiply(v,local),-1,1,&(SWLRenderer){.emit=emit,.mask=mask},NULL));assert(emits>0);banks++;
 }assert(banks>0);
}
static void aqua(void){
 GamePort *p=open_game("aqua");update(p,(GameInput){.down=GAME_KEY_A});tick(p,50);AquaModel *m=game_aqua_model(game_port_game(p));art(p);zoom_clamps(p,true);assert(visibleClaw);
 SWLMatrix v=view(p);float x,y;instance_center(p,1,"root.sub",&x,&y);float sx=v.a*x+v.x,sy=v.d*y+v.y;assert(sx>=0&&sx<320&&sy>=28&&sy<220);
 double before=m->x;game_port_touch(p,GAME_TOUCH_DOWN,sx+40,sy);tick(p,1);assert(m->x>before);
 before=m->x;game_port_touch(p,GAME_TOUCH_MOVE,-10,sy);tick(p,1);assert(m->x==before);
 game_port_touch(p,GAME_TOUCH_DOWN,270,230);update(p,(GameInput){0});assert(game_aqua_claw_held(game_port_game(p)));
 game_port_touch(p,GAME_TOUCH_UP,-20,-20);update(p,(GameInput){0});assert(!game_aqua_claw_held(game_port_game(p)));
 update(p,(GameInput){.held=GAME_KEY_A});assert(game_aqua_claw_held(game_port_game(p)));update(p,(GameInput){0});assert(!game_aqua_claw_held(game_port_game(p)));
 before=m->x;game_port_touch(p,GAME_TOUCH_DOWN,sx+40,sy);game_port_touch(p,GAME_TOUCH_UP,999,999);tick(p,1);assert(m->x==before);
 size_t bytes=game_port_texture_bytes(p);for(unsigned i=0;i<20;i++)art(p);assert(game_port_texture_bytes(p)==bytes);game_port_free(p);
}
static void rescue(void){
 GamePort *p=open_game("rescue");update(p,(GameInput){.down=GAME_KEY_A});tick(p,60);RescueModel *m=game_rescue_model(game_port_game(p));art(p);zoom_clamps(p,false);
 static const int directions[][2]={{1,0},{-1,0},{0,1},{0,-1}};
 for(unsigned i=0;i<4;i++){
  SWLMatrix v=view(p);float x,y;instance_center(p,0,"root.player",&x,&y);x+=40;y+=40;RescueModel expected=*m;rescue_model_step(&expected,directions[i][0],directions[i][1]);
  tap(p,v.a*(x+80*directions[i][0])+v.x,v.d*(y+80*directions[i][1])+v.y);
  assert(m->x==expected.x&&m->y==expected.y&&m->moves==expected.moves&&m->score==expected.score);
  unsigned moves=m->moves;game_port_touch(p,GAME_TOUCH_MOVE,300,150);game_port_touch(p,GAME_TOUCH_UP,999,999);update(p,(GameInput){0});assert(m->moves==moves);
 }
 unsigned moves=m->moves;tap(p,160,230);tap(p,-1,100);assert(m->moves==moves);
 RescueModel expected=*m;rescue_model_step(&expected,1,0);update(p,(GameInput){.down=GAME_KEY_RIGHT});assert(m->x==expected.x&&m->y==expected.y&&m->moves==expected.moves);
 game_port_free(p);
}
static void waves(void){
 GamePort *p=open_game("waves");tap(p,100,45);WavesModel *m=game_waves_model(game_port_game(p));art(p);
 tap(p,270,230);assert(m->air);SWLMatrix v=view(p);float y=v.d*(float)m->y+v.y;
 game_port_touch(p,GAME_TOUCH_DOWN,220,y);tick(p,1);assert(m->spin>0);double spin=m->spin;
 game_port_touch(p,GAME_TOUCH_MOVE,220,20);tick(p,1);assert(m->spin==spin);
 game_port_touch(p,GAME_TOUCH_DOWN,100,y);tick(p,1);assert(m->spin<spin);spin=m->spin;
 game_port_touch(p,GAME_TOUCH_UP,999,999);tick(p,1);assert(m->spin==spin);
 game_port_update(p,1.0f/24,&(GameInput){.held=GAME_KEY_RIGHT});assert(m->spin>spin);spin=m->spin;
 double flip=m->flip;game_port_touch(p,GAME_TOUCH_DOWN,160,60);tick(p,1);assert(m->flip<flip);game_port_touch(p,GAME_TOUCH_UP,-1,-1);flip=m->flip;tick(p,1);assert(m->flip==flip&&m->spin==spin);
 game_port_free(p);
}
int main(void){aqua();rescue();waves();puts("PASS Aqua/Rescue/Waves: original-bank lower art, inverse touch routing, cardinal movement, fast jump tap, off-area/release clearing, hardware controls, unchanged texture residency");return 0;}
