// Regression (2026-11-05): the lower-screen panel must not cover a game's lower scene.
// The 2026-11-04 restyle painted a full-screen panel in game_port_draw_bottom after main.c had drawn the lower scene
// through game_port_draw_lower, which erased the playfield of the published dual-screen games (Hydro, Roundup,
// Ice Fishing, Pizzatron, Cart Surfer, Bean Counters, Thin Ice...). Both Ice Fishing
// and the adapted DJ3K deck must retain their lower scene without a full-screen rect.
#include "game_controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {unsigned full;} Fixture;
static GameResource resource(void *user,const char *name,char *path,size_t cap){(void)user;snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static void rect(void *user,float x,float y,float w,float h,uint32_t rgb){(void)rgb;Fixture *f=user;if(x<=0&&y<=0&&w>=320&&h>=240)f->full++;}
static void text(void *user,float x,float y,float size,uint32_t rgb,GameAlign align,const char *value){(void)user;(void)x;(void)y;(void)size;(void)rgb;(void)align;(void)value;}
static void button(void *user,float x,float y,float w,float h,const char *label,bool pressed,bool enabled){(void)user;(void)x;(void)y;(void)w;(void)h;(void)label;(void)pressed;(void)enabled;}
static void leave(void *user,unsigned room,int x,int y){(void)user;(void)room;(void)x;(void)y;}

static unsigned full_rects(const char *key){
 Fixture f={0};GameDescriptor d={.returnRoom=100,.colourRGB=0x0099ff};snprintf(d.key,sizeof d.key,"%s",key);
 GameHost host={.user=&f,.resource=resource,.leave=leave,.rect=rect,.text=text,.button=button};
 GamePort *p=game_port_open(&d,&host);assert(p);
 for(unsigned i=0;i<120&&game_port_session(p)!=GAME_SESSION_PLAYING;i++)game_port_update(p,1.0f/24,&(GameInput){0});
 assert(game_port_session(p)==GAME_SESSION_PLAYING);
 f.full=0;game_port_draw_bottom(p);unsigned n=f.full;
 game_port_close(p);game_port_free(p);
 return n;
}

int main(void){
 assert(full_rects("fish")==0);
 assert(full_rects("dj3k")==0);
 puts("PASS lower-screen panel: Ice Fishing and DJ3K keep their lower scenes uncovered");
 return 0;
}
