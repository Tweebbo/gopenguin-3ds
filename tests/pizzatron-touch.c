#ifndef PIZZATRON_SOURCE
#define PIZZATRON_SOURCE "../source/game_pizzatron.c"
#endif
#include PIZZATRON_SOURCE
#include <assert.h>
#include <time.h>
static GameResource resource(void *u,const char *name,char *path,size_t cap){(void)u;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool emit(void *u,const SWLArt *a,const SWLMatrix *m,const SWLColor *c){(void)u;(void)a;(void)m;(void)c;return true;}
static bool mask(void *u,unsigned e){(void)u;(void)e;return true;}
static void update(GamePort *p,float dt){game_port_update(p,dt,&(GameInput){0});}
static void drag(GamePort *p,float trayX,float x,float y){game_port_touch(p,GAME_TOUCH_DOWN,trayX,60);game_port_touch(p,GAME_TOUCH_MOVE,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);}
static void check_mode(bool candy){
 GamePort *port=game_port_open(&(GameDescriptor){.key="pizzatron"},&(GameHost){.resource=resource});assert(port);update(port,1.f/24);if(candy)game_port_update(port,1.f/24,&(GameInput){.down=GAME_KEY_X});game_port_update(port,1.f/24,&(GameInput){.down=GAME_KEY_A});Pizzatron *p=game_port_game(port);assert(p->started);
 while(p->model.x<100)update(port,1.f/24);
 drag(port,157,160,164);drag(port,157,180,164);update(port,1.f/24);printf("Two rapid drops: %u toppings\n",p->model.counts[0]);
#ifndef BASELINE
 assert(p->model.counts[0]==2);
#endif
 game_port_touch(port,GAME_TOUCH_DOWN,18,60);game_port_touch(port,GAME_TOUCH_MOVE,160,164);game_port_touch(port,GAME_TOUCH_UP,160,10);printf("Header release: held=%d stylus=%d\n",p->model.held,p->stylus);
#ifndef BASELINE
 assert(!p->model.held&&!p->stylus);
 drag(port,157,160,164);assert(p->model.counts[0]==3);
 game_port_touch(port,GAME_TOUCH_DOWN,200,164);game_port_touch(port,GAME_TOUCH_UP,200,164);assert(p->model.counts[0]==4);
#endif
 #ifndef BASELINE
 game_port_touch(port,GAME_TOUCH_DOWN,18,60);game_port_touch(port,GAME_TOUCH_MOVE,160,164);
 assert(p->model.splatCount==1);
 for(unsigned i=0;i<20;i++)game_port_touch(port,GAME_TOUCH_MOVE,160+i,164);
 assert(p->model.splatCount==1);
 game_port_touch(port,GAME_TOUCH_UP,180,164);assert(!p->model.ingredient);
 update(port,1.f/24);assert(p->model.splatCount==1);
#endif
 pizz_model_init(&p->model,candy,random01,p);p->model.x=100;p->model.phase=PIZZ_PLAY;p->stylus=false;p->keyHeld=false;
 game_port_touch(port,GAME_TOUCH_DOWN,18,60);
 clock_t begin=clock();uint64_t before=swl_stats(p->scene).preparations;
 for(unsigned i=0;i<240;i++){
  game_port_touch(port,GAME_TOUCH_MOVE,160+(int)(i%5)*15,140+(int)(i%6)*10);update(port,1.f/24);
  assert(swl_render(p->scene,(SWLMatrix){.5,0,0,.5,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},NULL));
  assert(swl_view_frame(p->scene,"root",8));assert(swl_render(p->scene,(SWLMatrix){1,0,0,1,0,0},-1,1,&(SWLRenderer){.emit=emit,.mask=mask},NULL));assert(swl_view_frame(p->scene,"root",0));
 }
 printf("240 dual-screen frames: %.3f CPU seconds, %llu scene preparations\n",(double)(clock()-begin)/CLOCKS_PER_SEC,(unsigned long long)(swl_stats(p->scene).preparations-before));
 game_port_free(port);
}
int main(void){check_mode(false);check_mode(true);return 0;}
