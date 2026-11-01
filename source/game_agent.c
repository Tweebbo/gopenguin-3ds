#include "game_controller.h"
#include "mission_ui.h"
#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {GamePort *port;json_t *state;char room[24],resource[80],notice[96],queued[48];bool waiting,polling,joined,loaded,uiLoaded;float sent,poll;unsigned page,answer;int aim,queuedArg;} Agent;
static const char *rooms[]={"auntarctic","icerink","petshop","sportshop","iceberg","mountain","tallmountain","tallmountaintop","telescope","minigame"};
static const char *titles[]={"Aunt Arctic's Igloo","Ice Rink","Pet Shop","Sport Shop","Iceberg","Ski Mountain","Tallest Mountain","Mountain Summit","Telescope","Life Preserver Rescue"};
static const unsigned tools[]={6,4,5,11,12,13};
static const char *toolNames[]={"Wrench","Scissors","Comb","Grappling hook","Life preserver","Puffle photos"};
static const char *string(json_t *o,const char *key){const char *s=json_string_value(json_object_get(o,key));return s?s:"";}
static int number(json_t *o,const char *key){return (int)json_number_value(json_object_get(o,key));}
static bool flag(Agent *a,const char *name){json_t *list=json_object_get(a->state,"flags");for(size_t i=0;i<json_array_size(list);i++){const char *s=json_string_value(json_array_get(list,i));if(s&&!strcmp(s,name))return true;}return false;}
static unsigned room_index(const char *name){for(unsigned i=0;i<10;i++)if(!strcmp(name,rooms[i]))return i;return 10;}
static const char *phase(Agent *a){return string(a->state,"phase");}
static void send_action(Agent *a,const char *action,int arg){
 if(!a->joined||!game_port_connected(a->port))return;
 if(a->waiting){if(a->polling&&strcmp(action,"poll")&&strcmp(action,"state")){snprintf(a->queued,sizeof a->queued,"%s",action);a->queuedArg=arg;}return;}
 char value[24];snprintf(value,sizeof value,"%d",arg);const char *args[]={action,value};
 if(game_port_send(a->port,'s',"q#psa",2,args)){a->waiting=true;a->polling=!strcmp(action,"poll");a->sent=game_port_clock(a->port);a->notice[0]=0;}else snprintf(a->notice,sizeof a->notice,"Could not reach the server. Retry.");
}
static const char *resource(void *game,unsigned bank){Agent *a=game;return bank==0?a->resource:bank==1?"missions/1/ui.gpk":NULL;}
static SWLScene *scene(Agent *a){return game_bank_scene(game_port_bank(a->port,0));}
static void target(Agent *a,const char *path,unsigned frame,const char *label){if(scene(a))swl_target(scene(a),path,frame,label,false);}
static bool has_tool(Agent *a,unsigned id){if(id==4||id==5||id==6)return true;json_t *list=json_object_get(a->state,"items");for(size_t i=0;i<json_array_size(list);i++)if(json_integer_value(json_array_get(list,i))==id)return true;return false;}
static void ui_art(Agent *a,const char *name,float x,float y,float scale){SWLScene *s=game_bank_scene(game_port_bank(a->port,1));if(s){swl_visible(s,name,true);swl_transform(s,name,(SWLMatrix){scale,0,0,scale,x,y});}}
static void present_ui(Agent *a){
 SWLScene *s=game_bank_scene(game_port_bank(a->port,1));if(!s||!a->state)return;
 const char *names[]={"mapTouch","code","mapIcon","phoneIcon","left","right","item4","item5","item6","item11","item12","item13"};
 for(unsigned i=0;i<sizeof names/sizeof *names;i++)swl_visible(s,names[i],false);
 const char *p=phase(a);
 if(!strcmp(p,"map"))ui_art(a,"mapTouch",0,0,1);
 else if(!strcmp(p,"cipher"))ui_art(a,"code",96,1,1);
 else if(!strcmp(p,"bag")){for(unsigned i=0;i<6;i++)if(has_tool(a,tools[i])){char name[20];snprintf(name,sizeof name,"item%u",tools[i]);ui_art(a,name,28+i%3*101,30+i/3*81,1);}}
 else if(!strcmp(p,"room")&&!json_is_true(json_object_get(a->state,"complete"))){
  ui_art(a,"mapIcon",10,180,1);ui_art(a,"phoneIcon",70,180,1);
  if(strcmp(a->room,"minigame")){ui_art(a,"left",1,63,1);ui_art(a,"right",291,63,1);}
  unsigned held=number(a->state,"held");if(held){char name[20];snprintf(name,sizeof name,"item%u",held);ui_art(a,name,137,180,.7f);}
 }
}
static void present(Agent *a){
 present_ui(a);
 SWLScene *s=scene(a);if(!s||!a->state)return;
 if(!strcmp(phase(a),"note")||!strcmp(phase(a),"cipher")){
  swl_transform(s,"root",(SWLMatrix){1,0,0,1,80,310});target(a,"root",2,NULL);
  const char *fields[]={"root.socksNotePart1Txt","root.socksWordsAmountTxt","root.socksNotePart2Txt"};
  char note[128];snprintf(note,sizeof note,"%s",string(a->state,"note"));for(char *p=note;*p;p++)if(*p>='A'&&*p<='Z')*p+=32;char *end=strstr(note," pairs of socks");if(end)*end=0;game_bank_text(game_port_bank(a->port,0),fields[0],"g has");game_bank_text(game_port_bank(a->port,0),fields[1],strlen(note)>6?note+6:"");game_bank_text(game_port_bank(a->port,0),fields[2],"pairs of socks");
  game_port_snap(a->port);return;
 }
 swl_transform(s,"root",(SWLMatrix){1,0,0,1,(!strcmp(a->room,"telescope")||!strcmp(a->room,"tallmountain"))?0:number(a->state,"pan"),!strcmp(a->room,"tallmountain")?number(a->state,"pan"):0});
 if(!strcmp(a->room,"telescope"))swl_transform(s,"root.telescope",(SWLMatrix){.999969f,0,0,1,(float)json_number_value(json_object_get(a->state,"pan")),208.7f});
 if(!strcmp(a->room,"icerink"))swl_visible(s,"root.scene_mc.room1.pictures_mc",!flag(a,"photos"));
 if(!strcmp(a->room,"sportshop")&&flag(a,"gadgets")){target(a,"root.scene_mc.room1",2,NULL);json_t *items=json_object_get(a->state,"items");bool life=false,hook=false;for(size_t i=0;i<json_array_size(items);i++){int id=(int)json_integer_value(json_array_get(items,i));if(id==12)life=true;if(id==11)hook=true;}swl_visible(s,"root.scene_mc.room1.lpshooter_mc",!life);swl_visible(s,"root.scene_mc.room1.graphook_mc",!hook);}
 if(!strcmp(a->room,"iceberg")&&flag(a,"rescued"))target(a,"root.scene_mc.room1",0,"saved");
 if(!strcmp(a->room,"mountain"))target(a,"root.scene_mc.room1.tboy",0,flag(a,"telescopeFixed")?"start4":flag(a,"rescued")?"start2":"start1");
 const char *speaker=string(a->state,"speaker"),*frame=string(a->state,"frame");
 if(*frame){if(!strcmp(speaker,"aunt"))target(a,"root.scene_mc.room1.aunt",0,frame);else if(!strcmp(speaker,"g"))target(a,"root.scene_mc.room1.g",0,frame);else if(!strcmp(speaker,"tboy"))target(a,"root.scene_mc.room1.tboy",0,frame);}
 json_t *r=json_object_get(a->state,"rescue");
 if(json_is_object(r)){
  swl_visible(s,"root.minigame.perserver",json_is_true(json_object_get(r,"shooting")));
  swl_transform(s,"root.minigame.perserver",(SWLMatrix){1,0,0,1,(float)json_number_value(json_object_get(r,"x")),(float)json_number_value(json_object_get(r,"y"))});
  const char *hidden[]={"root.minigame.gameHint","root.minigame.endSpeech","root.minigame.crosshair","root.minigame.bergHit"};for(unsigned i=0;i<4;i++)swl_visible(s,hidden[i],false);
  static const unsigned depths[]={25,29,34,38,43,47,51};json_t *saved=json_object_get(r,"saved");for(unsigned i=0;i<7;i++){bool found=false;for(size_t j=0;j<json_array_size(saved);j++)if(json_integer_value(json_array_get(saved,j))==i)found=true;char path[64];snprintf(path,sizeof path,"root.minigame#%u",depths[i]);swl_visible(s,path,!found);}
  char count[16];snprintf(count,sizeof count,"%zu",json_array_size(json_object_get(r,"saved")));game_bank_text(game_port_bank(a->port,0),"root.minigame.savedNum",count);
  int wind=number(r,"wind");target(a,"root.minigame.wind_mc",wind>0?2:wind<0?1:3,NULL);target(a,"root.minigame.flag",wind==0?5:wind>=50?2:wind>0?4:wind<=-50?1:3,NULL);snprintf(count,sizeof count,"%u",(unsigned)abs(wind));game_bank_text(game_port_bank(a->port,0),"root.minigame.wind_mc.windStr",count);
 }
 game_port_snap(a->port);
}
static void *create(GamePort *port){Agent *a=calloc(1,sizeof *a);if(a){a->port=port;snprintf(a->room,sizeof a->room,"auntarctic");snprintf(a->resource,sizeof a->resource,"missions/1/auntarctic.gpk");}return a;}
static void destroy(void *game){Agent *a=game;json_decref(a->state);free(a);}
static bool ready(void *game){Agent *a=game;const char *args[]={"907","0","0"};if(!game_port_descriptor(a->port)->online){snprintf(a->notice,sizeof a->notice,"Connect to save agent missions.");return true;}a->waiting=game_port_send(a->port,'s',"j#jr",3,args);a->sent=game_port_clock(a->port);return true;}
static bool packet(void *game,const char *command,unsigned count,char *const *args){
 Agent *a=game;
 if(!strcmp(command,"jg")&&count&&atoi(args[0])==907){a->waiting=false;a->joined=true;send_action(a,"state",0);return true;}
 if(strcmp(command,"psa"))return false;
 if(count!=1||strlen(args[0])>16384)return true;
 json_t *state=json_loads(args[0],JSON_REJECT_DUPLICATES,NULL);const char *room=string(state,"room");
 if(!json_is_object(state)||number(state,"mission")!=1||room_index(room)>=10||!json_is_array(json_object_get(state,"flags"))){json_decref(state);return true;}
 const char *statePhase=string(state,"phase"),*art=(!strcmp(statePhase,"note")||!strcmp(statePhase,"cipher"))?"note":room;char resourceName[80];snprintf(resourceName,sizeof resourceName,"missions/1/%s.gpk",art);
 if(strcmp(resourceName,a->resource)){game_port_release_bank(a->port,0);snprintf(a->resource,sizeof a->resource,"%s",resourceName);a->loaded=false;}
 snprintf(a->room,sizeof a->room,"%s",room);
 json_decref(a->state);a->state=state;a->waiting=a->polling=false;a->page=0;a->notice[0]=0;present(a);
 if(*a->queued){char action[48];snprintf(action,sizeof action,"%s",a->queued);a->queued[0]=0;send_action(a,action,a->queuedArg);}return true;
}
static bool custom(void *user,unsigned id,const char *path){
 Agent *a=user;(void)path;MovieMeta *m=game_bank_meta(game_port_bank(a->port,0));id=movie_meta_source(m,id);
 static const unsigned ids[][8]={{63,69},{53},{106,114,118,122},{24,41,45,48},{50,61},{63,71,73,75,77},{33,34},{42},{70,71},{0}};
 unsigned index=room_index(a->room);if(index>=10)return true;
 for(unsigned i=0;i<8;i++)if(ids[index][i]&&id==ids[index][i]){char action[32];snprintf(action,sizeof action,"button:%u",id);send_action(a,action,0);return true;}
 return true;
}
static void frame(void *game,float seconds,const GameInput *input){
 Agent *a=game;
 if(!a->uiLoaded&&a->state&&game_port_require_bank(a->port,1)==GAME_RESOURCE_READY){a->uiLoaded=true;present_ui(a);}
 if(!a->loaded&&a->state&&game_port_require_bank(a->port,0)==GAME_RESOURCE_READY){a->loaded=true;present(a);}
 if(a->waiting&&game_port_clock(a->port)-a->sent>10){a->waiting=a->polling=false;a->queued[0]=0;snprintf(a->notice,sizeof a->notice,"No reply. Retry to recover progress.");}
 if(json_is_object(json_object_get(a->state,"rescue"))){a->poll+=seconds;if(a->poll>.1f){a->poll=0;send_action(a,"poll",0);}}
 if(input->down&GAME_KEY_START){game_port_leave(a->port);return;}
 unsigned id=game_port_clicked(a->port);
 if(id==1){if(!a->joined)ready(a);else send_action(a,"state",0);}
 else if(id==2)send_action(a,"map",0);
 else if(id==3)send_action(a,"bag",0);
 else if(id==4)send_action(a,"pan",120);
 else if(id==5)send_action(a,"pan",-120);
 else if(id==6)send_action(a,"cancel",0);
 else if(id==7)send_action(a,"back",0);
 else if(id==8)send_action(a,"fire",a->aim);
 else if(id==9)send_action(a,"answer",(int)a->answer);
 else if(id==10)send_action(a,"puffleChoice",0);
 else if(id==11)send_action(a,"medal",0);
 else if(id==12)send_action(a,"letter",0);
 else if(id==15)send_action(a,!strcmp(phase(a),"cipher")?"note":"cipher",0);
 else if(id==16)a->page++;
 else if(id>=20&&id<27){char action[48];snprintf(action,sizeof action,"room:%s",rooms[id-20]);send_action(a,action,0);}
 else if(id>=30&&id<36)send_action(a,"item",(int)tools[id-30]);
 else if(id>=40&&id<48){json_t *choices=json_object_get(a->state,"choices");const char *label=json_string_value(json_array_get(choices,id-40));send_action(a,label&&!strcmp(label,"Continue")?"next":"choice",(int)id-40);}
 else if(id>=50&&id<=53){static const unsigned change[]={90,99,1,10};a->answer=(a->answer+change[id-50])%100;}
 if(!strcmp(phase(a),"answer")){if(input->down&GAME_KEY_UP)a->answer=(a->answer+1)%100;if(input->down&GAME_KEY_DOWN)a->answer=(a->answer+99)%100;}
 if(input->down&GAME_KEY_L)send_action(a,"pan",120);
 if(input->down&GAME_KEY_R)send_action(a,"pan",-120);
 if(input->down&GAME_KEY_B)send_action(a,"map",0);
 if(input->down&GAME_KEY_X)send_action(a,"bag",0);
 if(input->down&GAME_KEY_A){if(!strcmp(phase(a),"answer"))send_action(a,"answer",(int)a->answer);else if(!strcmp(a->room,"minigame"))send_action(a,"fire",a->aim);}
}
static void tick(void *game){Agent *a=game;if(scene(a))swl_tick(scene(a));}
static void touch(void *game,GameTouch phase_,float x,float y){
 Agent *a=game;if(y>=170||strcmp(phase(a),"room"))return;
 float sx=(x-25.5f)/.3541667f,sy=y/.3541667f;if(sx<0||sx>760||sy<0||sy>480)return;
 a->aim=(int)lroundf(sx-377.55f);
 if(phase_!=GAME_TOUCH_UP||(a->waiting&&!a->polling))return;
 if(!strcmp(a->room,"minigame"))send_action(a,"fire",a->aim);
 else if(!strcmp(phase(a),"room")&&scene(a)){MovieHooks hooks={.custom=custom};movie_meta_click(game_bank_meta(game_port_bank(a->port,0)),scene(a),sx,sy,&hooks,a);}
}
static unsigned wrap_colour(Agent *a,const char *value,float x,float y,unsigned columns,unsigned max,unsigned skip,unsigned colour){
 unsigned line=0;const char *p=value;
 while(*p){char text[96];size_t n=strlen(p);if(n>columns){n=columns;while(n&&p[n]!=' ')n--;if(!n)n=columns;}if(n>95)n=95;memcpy(text,p,n);text[n]=0;if(line>=skip&&line<skip+max)game_port_text(a->port,x,y+(line-skip)*13,10,colour,GAME_ALIGN_LEFT,text);line++;p+=n;while(*p==' ')p++;}return line;
}
static unsigned wrap(Agent *a,const char *value,float x,float y,unsigned columns,unsigned max,unsigned skip){return wrap_colour(a,value,x,y,columns,max,skip,0x003366);}
static void button(Agent *a,unsigned id,float x,float y,float w,float h,const char *label){game_port_button(a->port,id,x,y,w,h,label,!a->waiting||a->polling);}
static void bottom(void *game){
 Agent *a=game;const char *p=phase(a);
 if(*a->notice){game_port_rect(a->port,0,0,320,240,0xeaf6ff);wrap(a,a->notice,7,20,48,8,0);button(a,1,8,195,304,34,"Retry");return;}
 if(!a->state){game_port_text(a->port,160,180,12,0x003366,GAME_ALIGN_CENTER,"Loading mission...");return;}
 bool enabled=!a->waiting||a->polling;unsigned index=room_index(a->room);
 if(!strcmp(p,"map")){
  for(unsigned i=0;i<sizeof missionMapPlaces/sizeof *missionMapPlaces;i++)game_port_hotspot(a->port,20+room_index(missionMapPlaces[i].room),missionMapPlaces[i].x-missionMapPlaces[i].w/2,missionMapPlaces[i].y-missionMapPlaces[i].h/2,missionMapPlaces[i].w,missionMapPlaces[i].h,enabled);
  game_port_hotspot(a->port,6,285,0,34,33,enabled);game_port_rect(a->port,0,211,320,29,0xe6f3fb);game_port_text(a->port,12,220,11,0x003366,GAME_ALIGN_LEFT,"Choose a place");button(a,6,234,211,80,28,"Close");return;
 }
 if(!strcmp(p,"bag")){
  game_port_rect(a->port,0,0,320,24,0xffffff);game_port_text(a->port,12,6,13,0x003366,GAME_ALIGN_LEFT,"Inventory");
  for(unsigned i=0;i<6;i++)if(has_tool(a,tools[i])){float x=7+i%3*101,y=27+i/3*81;game_port_hotspot(a->port,30+i,x,y,98,79,enabled);game_port_text(a->port,x+49,y+62,9,0x003366,GAME_ALIGN_CENTER,toolNames[i]);}
  button(a,6,8,203,304,30,"Close");return;
 }
 if(!strcmp(p,"dialog")){
  game_port_rect(a->port,0,0,320,240,0xe6f3fb);const char *speaker=string(a->state,"speaker");
  game_port_text(a->port,12,5,11,0x003366,GAME_ALIGN_LEFT,!strcmp(speaker,"aunt")?"Aunt Arctic":!strcmp(speaker,"g")?"G":"Penguin");
  game_port_rect(a->port,6,23,308,110,0xffffff);unsigned lines=wrap(a,string(a->state,"text"),14,31,46,7,a->page*7);
  if(lines>(a->page+1)*7)button(a,16,7,185,306,43,"Read more");
  else{json_t *choices=json_object_get(a->state,"choices");for(size_t i=0;i<json_array_size(choices)&&i<2;i++){const char *text=json_string_value(json_array_get(choices,i));button(a,40+(unsigned)i,7,140+i*48,306,44,"");wrap_colour(a,text?text:"Continue",14,144+i*48,46,3,0,0x003366);}}return;
 }
 if(!strcmp(p,"answer")){game_port_rect(a->port,0,0,320,240,0xe6f3fb);game_port_rect(a->port,12,16,296,74,0xffffff);game_port_text(a->port,160,24,12,0x003366,GAME_ALIGN_CENTER,"How many pairs of socks?");char text[32];snprintf(text,sizeof text,"%u",a->answer);game_port_text(a->port,160,52,24,0x003366,GAME_ALIGN_CENTER,text);button(a,50,8,103,72,47,"-10");button(a,51,85,103,72,47,"-1");button(a,52,162,103,72,47,"+1");button(a,53,239,103,73,47,"+10");button(a,9,8,179,304,48,"Answer (A)");return;}
 if(!strcmp(p,"puffles")||!strcmp(p,"puffles2")){game_port_rect(a->port,0,0,320,240,0xe6f3fb);game_port_rect(a->port,8,32,304,122,0xffffff);wrap(a,string(a->state,"text"),16,45,45,7,0);button(a,10,8,194,304,38,"Continue");return;}
 if(json_is_true(json_object_get(a->state,"complete"))){game_port_rect(a->port,0,0,320,240,0xe6f3fb);game_port_rect(a->port,12,27,296,97,0xffffff);game_port_text(a->port,160,47,16,0x003366,GAME_ALIGN_CENTER,"Mission accomplished!");game_port_text(a->port,160,82,10,0x003366,GAME_ALIGN_CENTER,"Case of the Missing Puffles");game_port_text(a->port,160,108,9,0x003366,GAME_ALIGN_CENTER,"More missions coming soon, Agent!");button(a,11,8,157,148,48,json_is_true(json_object_get(a->state,"medal"))?"Medal collected":"Get Medal");if(flag(a,"photosGiven"))button(a,12,164,157,148,48,json_is_true(json_object_get(a->state,"letter"))?"Letter collected":"Get Letter");game_port_text(a->port,160,224,9,0x003366,GAME_ALIGN_CENTER,"START: return to the island");return;}
 if(!strcmp(p,"note")||!strcmp(p,"cipher")){if(!strcmp(p,"cipher")){button(a,15,4,204,87,30,"Read note");button(a,6,229,204,87,30,"Close");}else{game_port_hotspot(a->port,6,207,22,29,29,enabled);button(a,15,7,203,149,30,"Code key");button(a,6,164,203,149,30,"Close");}return;}
 if(!strcmp(a->room,"minigame")){json_t *r=json_object_get(a->state,"rescue");char text[64];snprintf(text,sizeof text,"Rescued %zu/7  Wind %d",json_array_size(json_object_get(r,"saved")),number(r,"wind"));game_port_rect(a->port,0,170,320,70,0xe6f3fb);game_port_text(a->port,160,173,11,0x003366,GAME_ALIGN_CENTER,text);button(a,8,8,192,220,38,"Fire (A)");button(a,7,236,192,76,38,"Back");return;}
 game_port_rect(a->port,0,170,320,9,0xffffff);game_port_text(a->port,160,171,8,0x003366,GAME_ALIGN_CENTER,index<10?titles[index]:"Agent mission");
 game_port_hotspot(a->port,2,4,180,54,57,enabled);game_port_hotspot(a->port,3,63,180,60,57,enabled);game_port_text(a->port,31,226,9,0x003366,GAME_ALIGN_CENTER,"Map");game_port_text(a->port,93,226,9,0x003366,GAME_ALIGN_CENTER,"Inventory");
 game_port_hotspot(a->port,4,0,58,28,58,enabled);game_port_hotspot(a->port,5,291,58,29,58,enabled);
 unsigned held=number(a->state,"held");if(held)for(unsigned i=0;i<6;i++)if(tools[i]==held)game_port_text(a->port,160,226,8,0x003366,GAME_ALIGN_CENTER,toolNames[i]);
 if(!strcmp(a->room,"telescope")||!strcmp(a->room,"tallmountaintop"))button(a,7,225,192,88,38,"Go back");
 else game_port_text(a->port,263,210,9,0x003366,GAME_ALIGN_CENTER,"L / R: look");
}
static bool lower(void *game,SWLMatrix *view){Agent *a=game;if(!a->loaded)return false;*view=(SWLMatrix){1,0,0,1,0,0};return true;}
static bool lower_bank(void *game,unsigned bank,SWLMatrix *view){Agent *a=game;if(bank==1){*view=(SWLMatrix){1,0,0,1,0,0};return a->uiLoaded;}if(bank||!a->loaded||(strcmp(phase(a),"room")&&strcmp(phase(a),"note")))return false;*view=(SWLMatrix){.3541667f,0,0,.3541667f,25.5f,0};return true;}
static bool bank_view(void *game,unsigned bank,SWLMatrix *view){Agent *a=game;(void)view;return bank==0&&a->loaded;}
static void close_game(void *game){game_port_leave(((Agent*)game)->port);}
const GameController GAME_AGENT_CONTROLLER={.key="mission1",.kind=GAME_CARD,.room=907,.backgroundRGB=0xe6f3fb,.banks={"missions/1/auntarctic.gpk","missions/1/ui.gpk"},.lazy=3,.order={0,1,2},.bank_resource=resource,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=bottom,.bank_view=bank_view,.lower_view=lower,.lower_bank_view=lower_bank};
