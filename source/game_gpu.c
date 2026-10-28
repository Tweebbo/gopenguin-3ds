// Console side of the game adapter: game banks are MovieBanks and the top screen
// draws them through SWL2 with the controller's overlay callbacks.
#include "game_controller.h"
#include "movie_bank.h"
#include "viewport.h"
#include <citro2d.h>
#include <stdlib.h>

struct GameBank {MovieBank *bank;};

GameBank *game_bank_load(const char *path,size_t limit,const char **error){
 GameBank *b=calloc(1,sizeof *b);if(!b){if(error)*error="Out of memory.";return NULL;}
 b->bank=movie_bank_load(path,limit,error);if(!b->bank){free(b);return NULL;}
 return b;
}
void game_bank_free(GameBank *b){if(!b)return;movie_bank_free(b->bank);free(b);}
SWLScene *game_bank_scene(GameBank *b){return b?movie_bank_scene(b->bank):NULL;}
MovieMeta *game_bank_meta(GameBank *b){return b?movie_bank_meta(b->bank):NULL;}
size_t game_bank_texture_bytes(const GameBank *b){return b?movie_bank_bytes(b->bank):0;}
bool game_bank_text(GameBank *b,const char *path,const char *value){return b&&movie_bank_text(b->bank,path,value);}

typedef struct {GamePort *port;unsigned bank;SWLMatrix view;} Layer;
static void line(void *user,float x0,float y0,float x1,float y1,float width,uint32_t rgb){
 const Layer *l=user;const SWLMatrix *v=&l->view;u32 colour=C2D_Color32(rgb>>16&255,rgb>>8&255,rgb&255,255);
 float scale=sqrtf(fabsf(v->a*v->d-v->b*v->c));
 C2D_ViewReset();
 C2D_DrawLine(v->a*x0+v->c*y0+v->x,v->b*x0+v->d*y0+v->y,colour,v->a*x1+v->c*y1+v->x,v->b*x1+v->d*y1+v->y,colour,width*scale<1?1:width*scale,.5f);
}
static bool depth(void *user,float value){
 Layer *l=user;const GameController *c=game_port_controller(l->port);
 if(c->overlay)c->overlay(game_port_game(l->port),l->bank,value,&(GameCanvas){.user=l,.line=line});
 return true;
}

static bool draw(GamePort *port,C3D_RenderTarget *target,unsigned width,unsigned height,SWLMatrix view,bool lower){
 const GameController *c=game_port_controller(port);bool ok=true;
 for(unsigned n=0;n<GAME_PORT_BANKS;n++){
  unsigned i=c->order[n];if(!c->banks[i])continue;
  GameBank *bank=game_port_bank(port,i);SWLMatrix local={1,0,0,1,0,0};
  unsigned frame=lower&&c->lower_frame?c->lower_frame(game_port_game(port),i):0;
  if(lower&&c->lower_frame&&!frame)continue;
  if(!bank||(c->bank_view&&!c->bank_view(game_port_game(port),i,&local)))continue;
  bool alternate=frame&&frame!=swl_frame(game_bank_scene(bank),"root");
  if(alternate)swl_view_frame(game_bank_scene(bank),"root",frame);
  Layer layer={port,i,swl_matrix_multiply(view,local)};
  ok=movie_bank_draw_depth(bank->bank,target,width,height,layer.view,alternate?1:game_port_interpolation(port),c->overlay?depth:NULL,NULL,&layer,lower||c->top_view)&&ok;
  if(alternate)swl_view_frame(game_bank_scene(bank),"root",0);
 }
 C2D_ViewReset();C2D_SetTintMode(C2D_TintMult);
 return ok;
}

bool game_port_draw_top(GamePort *port,C3D_RenderTarget *target){
 if(!game_port_drawable(port))return false;
 const GameController *c=game_port_controller(port);SWLMatrix custom;
 if(c->backgroundRGB){uint32_t rgb=c->backgroundRGB;C2D_TargetClear(target,C2D_Color32(rgb>>16&255,rgb>>8&255,rgb&255,255));C2D_SceneBegin(target);}
 if(c->top_view&&c->top_view(game_port_game(port),&custom))return draw(port,target,400,240,custom,false);
 float w,h;game_port_stage(port,&w,&h);Viewport view=viewport_cover(400,240,w,h,w/2,0,1);
 return draw(port,target,400,240,(SWLMatrix){view.scale,0,0,view.scale,view.x,view.y},false);
}

bool game_port_draw_lower(GamePort *port,C3D_RenderTarget *target){
 const GameController *c=game_port_controller(port);SWLMatrix view;
 if(!game_port_drawable(port)||!c->lower_view||!c->lower_view(game_port_game(port),&view))return false;
 if(c->backgroundRGB){uint32_t rgb=c->backgroundRGB;C2D_TargetClear(target,C2D_Color32(rgb>>16&255,rgb>>8&255,rgb&255,255));C2D_SceneBegin(target);}
 return draw(port,target,320,240,view,true);
}
