#define main legacy_main
#ifdef TEST_MISSION3
#include "game-mission3-port.c"
#else
#include "game-mission-port.c"
#endif
#undef main

static bool upper,foundSelected,foundChosen,foundNumbered;
static unsigned upperTexts,lowerTexts,fullPanels;
static void text_capture(void *user,float x,float y,float size,uint32_t colour,GameAlign align,const char *text){
 (void)user;(void)size;(void)colour;(void)align;assert(x>=0&&x<=(upper?400:320));assert(y>=0&&y+size<=240);
 assert(size>=10);
 if(upper)upperTexts++;
 else{lowerTexts++;if(strstr(text,"1/5"))foundSelected=true;if(!strcmp(text,"Left path"))foundChosen=true;}
}
static void rect_capture(void *user,float x,float y,float w,float h,uint32_t colour){(void)user;(void)colour;assert(x>=0&&y>=0&&x+w<=(upper?400:320)&&y+h<=240);if(!upper&&w>=320&&h>=240)fullPanels++;}
static void button_capture(void *user,float x,float y,float w,float h,const char *label,bool pressed,bool enabled){
 (void)user;(void)pressed;(void)enabled;assert(!upper);assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);
 if(!strcmp(label,"Left path")&&x<10){assert(x==8&&y==94&&h>=28);foundNumbered=true;}
}
static void tap_real(GamePort *p,float x,float y){game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);game_port_update(p,0,&(GameInput){0});}
static void check_layout(GamePort *p){
 const GameController *c=game_port_controller(p);void *g=game_port_game(p);SWLMatrix v,local={1,0,0,1,0,0};
 assert(c->draw_top&&c->lower_view&&c->lower_bank_view);assert(c->bank_view(g,0,&local));assert(c->lower_view(g,&v));assert(c->lower_bank_view(g,0,&local));assert(!memcmp(&v,&local,sizeof v));assert(!c->lower_bank_view(g,1,&local));
 upper=true;upperTexts=0;c->draw_top(g);assert(upperTexts>=4);upper=false;lowerTexts=0;fullPanels=0;game_port_draw_bottom(p);assert(!fullPanels&&lowerTexts>=1);
 unsigned draws=0;assert(swl_render(game_bank_scene(game_port_bank(p,0)),v,-1,1,&(SWLRenderer){.emit=emit,.mask=mask},&draws));assert(draws>0);
 assert(game_port_texture_bytes(p)<=GAME_PORT_TEXTURE_LIMIT);
}
static void focused(void){
 Fixture f={0};
#ifdef TEST_MISSION3
 GameDescriptor d={.key="mission3",.online=true,.returnRoom=121,.returnX=380,.returnY=300};char *join[]={"911"};
#else
 GameDescriptor d={.key="mission4",.online=true,.returnRoom=121,.returnX=380,.returnY=300};char *join[]={"913"};
#endif
 GameHost h={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins,.text=text_capture,.rect=rect_capture,.button=button_capture};
 GamePort *p=game_port_open(&d,&h);assert(p);tick(p,2);assert(game_port_packet(p,"jg",1,join));check_layout(p);SWLMatrix plain;assert(game_port_controller(p)->lower_view(game_port_game(p),&plain));
#ifdef TEST_MISSION3
 for(unsigned i=0;i<9;i++){tap_real(p,160,218);tick(p,2);check_layout(p);}
 assert(game_mission3_step(game_port_game(p))==9);SWLMatrix puzzle;assert(game_port_controller(p)->lower_view(game_port_game(p),&puzzle));assert(!memcmp(&plain,&puzzle,sizeof plain));
 tap_real(p,160,218);assert(game_mission3_step(game_port_game(p))==9);
 unsigned presses[]={0,0,1,2,3};for(unsigned i=0;i<5;i++)tap_real(p,53+presses[i]*72,125);
 assert(game_mission3_fuses(game_port_game(p))==5);tap_real(p,160,218);tick(p,2);assert(game_mission3_step(game_port_game(p))==10);check_layout(p);
#else
 for(unsigned i=0;i<5;i++){tap_real(p,160,218);tick(p,2);check_layout(p);}
 assert(game_mission_step(game_port_game(p))==5);SWLMatrix puzzle;assert(game_port_controller(p)->lower_view(game_port_game(p),&puzzle));assert(!memcmp(&plain,&puzzle,sizeof plain));assert(foundNumbered);
 tap_real(p,160,218);assert(game_mission_step(game_port_game(p))==5);
 tap_real(p,80,108);assert(game_mission_order_progress(game_port_game(p))==1);check_layout(p);assert(foundChosen);
 tap_real(p,238,108);tap_real(p,80,141);tap_real(p,238,141);tap_real(p,160,174);assert(game_mission_order_progress(game_port_game(p))==5);check_layout(p);assert(foundSelected);
 tap_real(p,80,108);assert(game_mission_order_progress(game_port_game(p))==5);
 tap_real(p,160,218);tick(p,2);assert(game_mission_step(game_port_game(p))==6);check_layout(p);
#endif
 game_port_free(p);
}
#ifndef TEST_MISSION3
static void all_briefings(void){
 for(unsigned number=4;number<=11;number++){
  Fixture f={0};GameDescriptor d={.online=true,.returnRoom=121,.returnX=380,.returnY=300};snprintf(d.key,sizeof d.key,"mission%u",number);
  GameHost h={.user=&f,.resource=resource,.send=sendp,.leave=leave,.coins=coins,.text=text_capture,.rect=rect_capture};
  GamePort *p=game_port_open(&d,&h);assert(p);tick(p,2);void *g=game_port_game(p);const MissionData *data=game_mission_data(g);char room[8];snprintf(room,sizeof room,"%u",data->room);char *join[]={room};assert(game_port_packet(p,"jg",1,join));
  for(unsigned step=0;step<data->stepCount;step++){
   check_layout(p);const MissionPuzzle *q=&data->steps[step].puzzle;
   if(q->type==3)for(unsigned i=0;i<q->count;i++)game_mission_press_order(g,q->order[i]);
   if(q->type==2){
    char keys[12]={0};unsigned n=0;
    for(unsigned i=0;q->code[i];i++)if(!strchr(keys,q->code[i]))keys[n++]=q->code[i];
    for(unsigned i=0;q->code[i];i++){const char *at=strchr(keys,q->code[i]);assert(at);float total=n*52+(n-1)*8;tap_real(p,160-total/2+(at-keys)*60+26,126);}
    assert(!strcmp(game_mission_code(g),q->code));check_layout(p);
   }
   tap_real(p,160,218);tick(p,2);
  }
  check_layout(p);game_port_free(p);
 }
}
#endif
int main(void){int result=legacy_main();assert(result==0);focused();
#ifndef TEST_MISSION3
 all_briefings();
#endif
 puts("PASS mission dual-screen: upper room scene with dialogue box, original lower scene, state-specific framing, real puzzle touch coordinates, visible order feedback, existing progression and authoritative payout");return 0;}
