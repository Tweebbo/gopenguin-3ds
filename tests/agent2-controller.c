#include "game_controller.h"
#include "font_layout.h"
#include "mission2_guide.h"
#include <ctype.h>
#include <assert.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned buttons,sends;static char lastAction[48];static int lastArg;static size_t peak;
static bool dialogueText,acceptEdit;static unsigned editCalls,readableCalls;static const char *editValue;
static bool continueText,previousText;static float previousTextY;
static bool guideText,captureGuide;static char guideBody[2048];
static GameResource resolve_bank(void *user,const char *name,char *path,size_t cap){(void)user;assert(strncmp(name,"missions/2/",11)==0);snprintf(path,cap,"romfs/%s",name);return GAME_RESOURCE_READY;}
static bool send_packet(void *user,char extension,const char *command,unsigned count,const char *const *args){(void)user;assert(extension=='s');sends++;if(!strcmp(command,"q#psa")){assert(count==2);snprintf(lastAction,sizeof lastAction,"%s",args[0]);lastArg=atoi(args[1]);}return true;}
static void leave_game(void *user,unsigned room,int x,int y){(void)user;(void)room;(void)x;(void)y;}
static void draw_button(void *user,float x,float y,float w,float h,const char *label,bool pressed,bool enabled){(void)user;(void)label;(void)pressed;(void)enabled;assert(x>=0&&y>=0&&x+w<=320.1f&&y+h<=240.1f);assert(w>=28&&h>=28);buttons++;}
static void draw_text(void *user,float x,float y,float size,uint32_t rgb,GameAlign align,const char *value){
 (void)user;(void)rgb;if(!dialogueText&&!guideText)return;
 if(guideText&&y>=170&&size<14)return;
 if(guideText&&y>=36&&y<166){assert(size==14);if(captureGuide){assert(strlen(guideBody)+strlen(value)+2<sizeof guideBody);if(*guideBody)strcat(guideBody," ");strcat(guideBody,value);}}
 if(!strcmp(value,"Continue reading"))continueText=true;
 if(!strcmp(value,"Previous text")){previousText=true;previousTextY=y+6;}
 if(align==GAME_ALIGN_RIGHT)return;
 assert(size>=14);for(const unsigned char *p=(const unsigned char*)value;*p;p++)assert(*p<128);
 FontBounds ink=font_ink((unsigned)size,value);assert(x+ink.left>=0&&x+ink.right<=320&&y+ink.top>=0&&y+ink.bottom<=240);readableCalls++;
}
static bool edit_text(void *user,char *value,size_t capacity,const char *hint){(void)user;assert(capacity==17&&!strcmp(hint,"The word:"));editCalls++;snprintf(value,capacity,"%s",editValue);return acceptEdit;}
static bool emit_art(void *user,const SWLArt *a,const SWLMatrix *m,const SWLColor *c){(void)user;(void)a;(void)m;(void)c;return true;}
static bool emit_mask(void *user,unsigned event){(void)user;(void)event;return true;}
static void tap(GamePort *p,int x,int y){game_port_draw_bottom(p);game_port_touch(p,GAME_TOUCH_DOWN,x,y);game_port_touch(p,GAME_TOUCH_UP,x,y);game_port_update(p,0,NULL);}
static void receive(GamePort *p,json_t *state){char *encoded=json_dumps(state,JSON_COMPACT);assert(encoded);assert(game_port_packet(p,"psa",1,&encoded));free(encoded);game_port_update(p,0,NULL);}
static void check_tap(GamePort *p,json_t *state,int x,int y,const char *action,int arg){lastAction[0]=0;tap(p,x,y);assert(game_port_active(p));assert(!strcmp(lastAction,action));assert(lastArg==arg);receive(p,state);}
static void normalize(const char *source,char *target,size_t capacity){
 size_t used=0;bool space=false;
 for(size_t i=0;source[i];i++){
  unsigned char c=(unsigned char)source[i];
  if(c==0xe2&&(unsigned char)source[i+1]==0x80&&((unsigned char)source[i+2]==0x99||(unsigned char)source[i+2]==0x98)){c='\'';i+=2;}
  if(isspace(c)){space=used>0;continue;}if(space){assert(used+1<capacity);target[used++]=' ';space=false;}
  assert(used+1<capacity);target[used++]=(char)c;
 }target[used]=0;
}
static void capture_guide(GamePort *p){guideBody[0]=0;captureGuide=true;game_port_draw_bottom(p);captureGuide=false;}
int main(int argc,char **argv){
 assert(argc==2);json_error_t error;json_t *trace=json_load_file(argv[1],JSON_REJECT_DUPLICATES,&error);assert(json_is_array(trace));
 GameDescriptor d={.online=true};snprintf(d.key,sizeof d.key,"mission2");GameHost host={.resource=resolve_bank,.send=send_packet,.leave=leave_game,.button=draw_button,.text=draw_text,.edit_text=edit_text};
 GamePort *p=game_port_open(&d,&host);assert(p);game_port_update(p,0,NULL);char *joined[]={"908"};assert(game_port_packet(p,"jg",1,joined));assert(!strcmp(lastAction,"select"));
 unsigned answerChecks=0,guideChecks=0,itemChecks=0,moreChecks=0,mapChecks=0,dialogChecks=0,useChecks=0,roomChecks=0,rewardChecks=0,rideChecks=0,textPageChecks=0;
 for(size_t i=0;i<json_array_size(trace);i++){
  json_t *state=json_object_get(json_array_get(trace,i),"snapshot");char *encoded=json_dumps(state,JSON_COMPACT);assert(encoded);
  assert(game_port_packet(p,"psa",1,&encoded));free(encoded);game_port_update(p,0,NULL);
  const char *mode=json_string_value(json_object_get(state,"phase"));dialogueText=!strcmp(mode,"dialog")||!strcmp(mode,"confirm");guideText=!strcmp(mode,"guide");buttons=0;continueText=previousText=false;game_port_draw_bottom(p);assert(buttons||!strcmp(mode,"ride"));
  if(dialogueText&&!textPageChecks){
   json_t *longState=json_deep_copy(state);const char *original=json_string_value(json_object_get(state,"text"));char expanded[2048];
   snprintf(expanded,sizeof expanded,"%s %s %s %s",original,original,original,original);json_object_set_new(longState,"text",json_string(expanded));receive(p,longState);continueText=false;game_port_draw_bottom(p);json_decref(longState);
  }
  if(dialogueText&&continueText&&!textPageChecks){
   unsigned before=sends;tap(p,160,142);previousText=false;game_port_draw_bottom(p);assert(previousText&&sends==before);
   tap(p,75,previousTextY);continueText=false;game_port_draw_bottom(p);assert(continueText&&sends==before);textPageChecks++;receive(p,state);
  }
  size_t bytes=game_port_texture_bytes(p);if(bytes>peak)peak=bytes;assert(bytes<=6u*1024u*1024u);
  GameBank *bank=game_port_bank(p,0);assert(bank);SWLScene *scene=game_bank_scene(bank);
  if(!strcmp(mode,"ride")){SWLInstance instance;assert(swl_pose(scene,"root#1.sled",&instance));assert(swl_pose(scene,"root#1.landscape",&instance));for(unsigned seg=1;seg<=21;seg++){char path[48];snprintf(path,sizeof path,"root#1.seg%u",seg);assert(swl_pose(scene,path,&instance));}}
  assert(swl_render(scene,(SWLMatrix){.5f,0,0,.5f,0,0},-1,1,&(SWLRenderer){.emit=emit_art,.mask=emit_mask},NULL));assert(!swl_error(scene));
  if(!strcmp(mode,"answer")&&!answerChecks){
   unsigned before=sends;lastAction[0]=0;tap(p,160,110);assert(sends==before&&!lastAction[0]);
   acceptEdit=false;editValue="WRONG";tap(p,210,38);assert(editCalls==1);tap(p,160,110);assert(sends==before);
   acceptEdit=true;editValue="MOGUL";tap(p,210,38);assert(editCalls==2);tap(p,160,110);assert(!strcmp(lastAction,"answer:MOGUL"));receive(p,state);
   acceptEdit=false;editValue="WRONG";tap(p,210,38);tap(p,160,110);assert(!strcmp(lastAction,"answer:MOGUL"));receive(p,state);
   acceptEdit=true;editValue=" m0o_g!u-l ";game_port_update(p,0,&(GameInput){.down=GAME_KEY_A});tap(p,160,110);assert(!strcmp(lastAction,"answer:mogul"));receive(p,state);
   editValue="ABCDEFGHIJKLMNOPQRST";tap(p,210,38);tap(p,160,110);assert(!strcmp(lastAction,"answer:ABCDEFGHIJKLMNOP"));receive(p,state);
   check_tap(p,state,160,174,"riddle",0);answerChecks++;
   receive(p,state);
  }
  if(!strcmp(mode,"guide")&&!guideChecks){check_tap(p,state,264,214,"cancel",0);guideChecks++;}
  if(!strcmp(mode,"map")&&!mapChecks){check_tap(p,state,160,85,"room:sportShop",0);check_tap(p,state,160,135,"room:mountain",0);check_tap(p,state,160,205,"cancel",0);mapChecks++;}
  json_t *items=json_object_get(state,"items");size_t itemCount=json_array_size(items);
  if(!strcmp(mode,"bag")&&itemCount&&!itemChecks){check_tap(p,state,160,90,"item",(int)json_integer_value(json_array_get(items,0)));check_tap(p,state,170,210,"cancel",0);itemChecks++;}
  if(!strcmp(mode,"bag")&&itemCount>3&&!moreChecks){tap(p,270,210);check_tap(p,state,160,90,"item",(int)json_integer_value(json_array_get(items,3)));moreChecks++;}
  if(!strcmp(mode,"bag")&&json_integer_value(json_object_get(state,"held"))>0&&!useChecks){check_tap(p,state,60,210,"use",0);useChecks++;}
  json_t *choices=json_object_get(state,"choices");
  if(!strcmp(mode,"dialog")&&json_array_size(choices)&&!dialogChecks){const char *label=json_string_value(json_array_get(choices,0));check_tap(p,state,160,134,!strcmp(label,"Continue")?"next":"choice",0);dialogChecks++;}
  if(!strcmp(mode,"room")&&!roomChecks){check_tap(p,state,60,210,"bag",0);check_tap(p,state,170,210,"map",0);GameInput input={.down=GAME_KEY_L};game_port_update(p,0,&input);assert(!strcmp(lastAction,"pan")&&lastArg==120);receive(p,state);input.down=GAME_KEY_R;game_port_update(p,0,&input);assert(!strcmp(lastAction,"pan")&&lastArg==-120);receive(p,state);roomChecks++;}
  if(!strcmp(mode,"complete")&&!rewardChecks){check_tap(p,state,80,130,"medal",0);check_tap(p,state,240,130,"letter",0);rewardChecks++;}
  if(!strcmp(mode,"ride")&&!rideChecks){game_port_touch(p,GAME_TOUCH_DOWN,80,80);assert(!strcmp(lastAction,"steer")&&lastArg==190);receive(p,state);game_port_update(p,.11f,NULL);assert(!strcmp(lastAction,"poll"));receive(p,state);rideChecks++;}
 }
 dialogueText=false;guideText=true;json_t *guideState=json_deep_copy(json_object_get(json_array_get(trace,72),"snapshot")),*pages=json_array();
 for(unsigned page=0;page<MISSION2_GUIDE_PAGE_COUNT;page++){
  json_object_set_new(guideState,"guidePage",json_integer(page+1));receive(p,guideState);
  SWLScene *s=game_bank_scene(game_port_bank(p,0));assert(swl_prepare(s));assert(swl_frame(s,"root")==MISSION2_GUIDE_PAGES[page].frame);
  assert(game_port_texture_bytes(p)<=6u*1024u*1024u);
  unsigned before=sends;tap(p,55,214);if(page){assert(sends==before+1&&!strcmp(lastAction,"page")&&lastArg==-1);receive(p,guideState);}else{game_port_update(p,0,&(GameInput){.down=GAME_KEY_L});assert(sends==before);}
  char expected[2048],observed[2048]={0};normalize(MISSION2_GUIDE_PAGES[page].text,expected,sizeof expected);unsigned parts=0;
  while(parts<16){
   capture_guide(p);char part[2048];normalize(guideBody,part,sizeof part);assert(*part);assert(strlen(observed)+strlen(part)+2<sizeof observed);
   if(*observed){strcat(observed," ");}strcat(observed,part);parts++;
   assert(!strncmp(expected,observed,strlen(observed)));
   if(!strcmp(expected,observed))break;
   before=sends;game_port_update(p,0,&(GameInput){.down=GAME_KEY_R});assert(sends==before);
   if(parts==1){game_port_update(p,0,&(GameInput){.down=GAME_KEY_L});capture_guide(p);char previous[2048];normalize(guideBody,previous,sizeof previous);assert(!strcmp(previous,part));game_port_update(p,0,&(GameInput){.down=GAME_KEY_R});assert(sends==before);}
  }
  assert(!strcmp(expected,observed));json_array_append_new(pages,json_pack("{s:i,s:i,s:i,s:b}","page",page+1,"frame",MISSION2_GUIDE_PAGES[page].frame,"parts",parts,"allOriginalTextMatched",1));
  before=sends;tap(p,160,214);if(page+1<MISSION2_GUIDE_PAGE_COUNT){assert(sends==before+1&&!strcmp(lastAction,"page")&&lastArg==1);receive(p,guideState);}else{game_port_update(p,0,&(GameInput){.down=GAME_KEY_R});assert(sends==before);}
 }
 assert(json_dump_file(pages,"build/agent-tests/guide-readability.json",JSON_INDENT(2))==0);json_decref(pages);check_tap(p,guideState,264,214,"cancel",0);lastAction[0]=0;game_port_update(p,0,&(GameInput){.down=GAME_KEY_B});assert(!strcmp(lastAction,"cancel"));json_decref(guideState);
 assert(answerChecks&&guideChecks&&itemChecks&&moreChecks&&mapChecks&&dialogChecks&&useChecks&&roomChecks&&rewardChecks&&rideChecks&&sends&&readableCalls&&textPageChecks);printf("PASS: %zu snapshots; all 8 guide pages match their complete original text at 14px, with local text paging, correct spread frames, navigation bounds and <=6 MiB textures; dialogue, keyboard, original word form, camera, steering and rewards.\n",json_array_size(trace));
 game_port_free(p);json_decref(trace);return 0;
}
