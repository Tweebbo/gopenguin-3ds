#include "game_controller.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef EAGER_REFERENCE
#define swl_tick_deferred swl_tick
#endif
#if defined(TEST_CART)
#include "../source/game_cart.c"
#define TEST_KEY "cart"
#define TEST_SEED(game) (((Cart*)(game))->seed=42)
#define TEST_MODEL(game) (&((Cart*)(game))->model)
#elif defined(TEST_HYDRO)
#include "../source/game_hydro.c"
#define TEST_KEY "hydro"
#define TEST_SEED(game) (((Hydro*)(game))->seed=42)
#elif defined(TEST_ROUNDUP)
#include "../source/game_roundup.c"
#define TEST_KEY "roundup"
#define TEST_SEED(game) (((Roundup*)(game))->seed=42)
#elif defined(TEST_THINICE)
#include "../source/game_thinice.c"
#define TEST_KEY "thinice"
#define TEST_SEED(game) (((ThinIce*)(game))->seed=42)
#elif defined(TEST_SLED)
#include "../source/game_sled.c"
#define TEST_KEY "sled"
#define TEST_SEED(game) ((void)(game))
#elif defined(TEST_FISH)
#include "../source/game_fish.c"
#define TEST_KEY "fish"
#define TEST_SEED(game) (((Fishing*)(game))->seed=42)
#define TEST_MODEL(game) (&((Fishing*)(game))->model)
#else
#error Select a controller
#endif

static uint64_t trace=14695981039346656037ull;
static uint64_t logic=14695981039346656037ull;
static void trace_bytes(const void *data,size_t count){const unsigned char *p=data;for(size_t i=0;i<count;i++)trace=(trace^p[i])*1099511628211ull;}
static bool trace_art(void *user,const SWLArt *art,const SWLMatrix *matrix,const SWLColor *colour){(void)user;trace_bytes(art,sizeof *art);trace_bytes(matrix,sizeof *matrix);trace_bytes(colour,sizeof *colour);return true;}
static bool trace_mask(void *user,unsigned event){(void)user;trace_bytes(&event,sizeof event);return true;}
static GameResource test_resource(void *u,const char *name,char *path,size_t cap){(void)u;const char *root=getenv("TEST_ROMFS");snprintf(path,cap,"%s/%s",root?root:"romfs",name);return GAME_RESOURCE_READY;}
int main(void){
 GameHost host={.resource=test_resource};GameDescriptor d={.colour=1};snprintf(d.key,sizeof d.key,"%s",TEST_KEY);
 GamePort *port=game_port_open(&d,&host);assert(port);TEST_SEED(game_port_game(port));
 uint64_t preparations=0,seen[GAME_PORT_BANKS]={0};unsigned frames=0;
 for(unsigned i=0;i<480;i++){
  GameInput input={.down=i==2||i==4||i==6?GAME_KEY_A:0,.held=(i/24)%2?GAME_KEY_LEFT:GAME_KEY_RIGHT,.stickX=(i/48)%2?-.5f:.5f};
  game_port_update(port,1.0f/24,&input);assert(game_port_session(port)==GAME_SESSION_PLAYING);
#ifdef TEST_MODEL
  const unsigned char *bytes=(const unsigned char *)TEST_MODEL(game_port_game(port));
  for(size_t j=0;j<sizeof *TEST_MODEL(game_port_game(port));j++)logic=(logic^bytes[j])*1099511628211ull;
#endif
#ifdef TEST_SLED
  if(i==1){Sled *sled=game_port_game(port);const char *players[]={"Test|1|0|Test","Other|2|0|Other"};sled_model_players(&sled->model,2,players,"Test");sled->model.started=true;}
#endif
  if(i>6&&i%12==0){game_port_touch(port,GAME_TOUCH_DOWN,80+(i%120),100);game_port_touch(port,GAME_TOUCH_UP,80+(i%120),100);}
  for(unsigned b=0;b<GAME_PORT_BANKS;b++){
   GameBank *bank=game_port_bank(port,b);if(!bank){seen[b]=0;continue;}SWLScene *s=game_bank_scene(bank);
   assert(swl_render(s,(SWLMatrix){.5f,0,0,.5f,0,0},-1,1,&(SWLRenderer){.emit=trace_art,.mask=trace_mask},NULL));
   assert(swl_render(s,(SWLMatrix){.4f,0,0,.4f,8,4},-1,1,&(SWLRenderer){.emit=trace_art,.mask=trace_mask},NULL));
   SWLStats stats=swl_stats(s);assert(!swl_error(s));
   if(stats.preparations>=seen[b])preparations+=stats.preparations-seen[b];seen[b]=stats.preparations;
  }
  frames++;
 }
 printf("%s %016llx %llu %u %016llx\n",TEST_KEY,(unsigned long long)trace,(unsigned long long)preparations,frames,(unsigned long long)logic);
 game_port_free(port);return 0;
}
