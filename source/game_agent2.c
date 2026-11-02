#include "game_controller.h"
#include "font.h"
#include "mission2_guide.h"
#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
 GamePort *port;json_t *state;
 char resource[80],notice[96],queued[48],word[17],wordError[64];
 bool joined,waiting,polling,loaded,hasLandscape;
 SWLMatrix landscape;
 float sent,poll;unsigned page,textPage;int queuedArg;
} Agent2;
static const char *const rooms[]={"sportShop","mountain","hillSigns","miniGame","crashSite","clearing1","clearing2","puffles","river","outsideCave","cave","guide"};
static const char *str(json_t *o,const char *key){const char *v=json_string_value(json_object_get(o,key));return v?v:"";}
static int num(json_t *o,const char *key){return (int)json_number_value(json_object_get(o,key));}
static const char *phase(Agent2 *a){return str(a->state,"phase");}
static bool room_valid(const char *room){for(unsigned i=0;i<sizeof rooms/sizeof *rooms;i++)if(!strcmp(room,rooms[i]))return true;return false;}
static bool flag(Agent2 *a,const char *name){json_t *flags=json_object_get(a->state,"flags");for(size_t i=0;i<json_array_size(flags);i++){const char *s=json_string_value(json_array_get(flags,i));if(s&&!strcmp(s,name))return true;}return false;}
static SWLScene *scene(Agent2 *a){return game_bank_scene(game_port_bank(a->port,0));}
static unsigned guide_page(Agent2 *a){int page=num(a->state,"guidePage");return page<1?0:page>MISSION2_GUIDE_PAGE_COUNT?MISSION2_GUIDE_PAGE_COUNT-1:(unsigned)page-1;}
static void send_action(Agent2 *a,const char *action,int arg){
 if(!a->joined||!game_port_connected(a->port))return;
 if(a->waiting){if(a->polling&&strcmp(action,"poll")&&strcmp(action,"steer")){snprintf(a->queued,sizeof a->queued,"%s",action);a->queuedArg=arg;}return;}
 char value[24];snprintf(value,sizeof value,"%d",arg);
 if(game_port_send(a->port,'s',"q#psa",2,(const char *[]){action,value})){a->waiting=true;a->polling=!strcmp(action,"poll")||!strcmp(action,"steer");a->sent=game_port_clock(a->port);a->notice[0]=0;}
 else snprintf(a->notice,sizeof a->notice,"Could not reach the server. Retry.");
}
static const char *resource(void *game,unsigned bank){return bank?NULL:((Agent2*)game)->resource;}
static void present(Agent2 *a){
 SWLScene *s=scene(a);if(!s||!a->state)return;
 bool guide=!strcmp(phase(a),"guide");
 swl_transform(s,"root",(SWLMatrix){1,0,0,1,guide?0:(float)json_number_value(json_object_get(a->state,"pan")),0});
 if(guide){swl_target(s,"root",MISSION2_GUIDE_PAGES[guide_page(a)].frame,NULL,false);return;}
 json_t *poses=json_object_get(a->state,"poses");
 for(size_t i=0;i<json_array_size(poses);i++){
  json_t *p=json_array_get(poses,i),*f=json_object_get(p,"frame"),*v=json_object_get(p,"visible");const char *path=str(p,"path");
  if(json_is_string(f))swl_target(s,path,0,json_string_value(f),false);
  else if(json_is_integer(f))swl_target(s,path,(unsigned)json_integer_value(f),NULL,false);
  if(json_is_boolean(v))swl_visible(s,path,json_is_true(v));
 }
 json_t *ride=json_object_get(a->state,"ride");
 if(json_is_object(ride)){
  swl_transform(s,"root#1.sled",(SWLMatrix){1,0,0,1,(float)num(ride,"x"),326.5f});
  swl_target(s,"root#1.sled",(unsigned)num(ride,"sledFrame"),NULL,false);
  swl_visible(s,"root#1.centerbox",false);swl_visible(s,"root#1.curvebox",false);
  SWLInstance background;
  if(!a->hasLandscape&&swl_pose(s,"root#1.landscape",&background)){a->landscape=background.matrix;a->landscape.y+=60;a->hasLandscape=true;}
  if(a->hasLandscape){SWLMatrix transform=a->landscape;float scale=(float)json_number_value(json_object_get(ride,"scale"));transform.a*=scale;transform.d*=scale;swl_transform(s,"root#1.landscape",transform);}
  for(unsigned i=1;i<=21;i++){
   char path[40];snprintf(path,sizeof path,"root#1.seg%u",i);SWLInstance pose;
   if(swl_pose(s,path,&pose)){
    swl_transform(s,path,(SWLMatrix){pose.matrix.a,0,0,pose.matrix.d,380.75f-((float)json_number_value(json_object_get(ride,"curveX"))-380.75f)*i,200-((float)json_number_value(json_object_get(ride,"curveY"))-200)*i});
    swl_target(s,path,((unsigned)num(ride,"ticks")+21-i)%1010+1,NULL,true);
   }
  }
 }
 game_port_snap(a->port);
}
static void *create(GamePort *port){Agent2 *a=calloc(1,sizeof *a);if(a){a->port=port;snprintf(a->resource,sizeof a->resource,"missions/2/sportShop.gpk");}return a;}
static void destroy(void *game){Agent2 *a=game;json_decref(a->state);free(a);}
static bool ready(void *game){
 Agent2 *a=game;
 if(!game_port_descriptor(a->port)->online){snprintf(a->notice,sizeof a->notice,"Connect to save agent missions.");return true;}
 a->waiting=game_port_send(a->port,'s',"j#jr",3,(const char *[]){"908","0","0"});a->sent=game_port_clock(a->port);return true;
}
static bool packet(void *game,const char *command,unsigned count,char *const *args){
 Agent2 *a=game;
 if(!strcmp(command,"jg")&&count&&atoi(args[0])==908){a->waiting=false;a->joined=true;send_action(a,"select",2);return true;}
 if(strcmp(command,"psa"))return false;
 if(count!=1||strlen(args[0])>16384)return true;
 json_t *next=json_loads(args[0],JSON_REJECT_DUPLICATES,NULL);
 if(!json_is_object(next)||num(next,"mission")!=2||!room_valid(str(next,"room"))||!json_is_array(json_object_get(next,"poses"))||!json_is_array(json_object_get(next,"flags"))){json_decref(next);return true;}
 bool changed=strcmp(str(next,"room"),str(a->state,"room"))||strcmp(str(next,"phase"),phase(a))||strcmp(str(next,"text"),str(a->state,"text"))||num(next,"guidePage")!=num(a->state,"guidePage");
 if(!strcmp(str(next,"phase"),"answer")&&strcmp(phase(a),"answer"))a->word[0]=a->wordError[0]=0;
 const char *art=!strcmp(str(next,"phase"),"guide")?"guide":str(next,"room");char name[80];snprintf(name,sizeof name,"missions/2/%s.gpk",art);
 if(strcmp(name,a->resource)){game_port_release_bank(a->port,0);snprintf(a->resource,sizeof a->resource,"%s",name);a->loaded=a->hasLandscape=false;}
 json_decref(a->state);a->state=next;a->waiting=a->polling=false;a->notice[0]=0;if(changed)a->page=a->textPage=0;present(a);
 if(*a->queued){char action[48];snprintf(action,sizeof action,"%s",a->queued);a->queued[0]=0;send_action(a,action,a->queuedArg);}
 return true;
}
static json_t *entries(Agent2 *a){return json_object_get(a->state,!strcmp(phase(a),"bag")?"items":!strcmp(phase(a),"room")?"actions":"choices");}
static void activate(Agent2 *a,unsigned index){
 json_t *e=json_array_get(entries(a),index);
 if(!e)return;
 if(!strcmp(phase(a),"bag"))send_action(a,"item",(int)json_integer_value(e));
 else if(!strcmp(phase(a),"room"))send_action(a,str(e,"action"),num(e,"arg"));
 else{const char *label=json_string_value(e);send_action(a,label&&!strcmp(label,"Continue")?"next":"choice",(int)index);}
}
static void edit_word(Agent2 *a){
 const GameHost *host=game_port_host(a->port);char value[sizeof a->word];memcpy(value,a->word,sizeof value);
 if(!host->edit_text||!host->edit_text(host->user,value,sizeof value,"The word:"))return;
 value[sizeof value-1]=0;unsigned used=0;
 for(unsigned i=0;value[i]&&used<sizeof a->word-1;i++)if((value[i]>='a'&&value[i]<='z')||(value[i]>='A'&&value[i]<='Z'))a->word[used++]=value[i];
 a->word[used]=0;a->wordError[0]=0;
}
static unsigned choice_page_size(Agent2 *a);
static unsigned guide_parts(Agent2 *a);
static void guide_step(Agent2 *a,int direction){
 if(direction<0){if(a->textPage)a->textPage--;else if(guide_page(a)>0)send_action(a,"page",-1);}
 else{if(a->textPage+1<guide_parts(a))a->textPage++;else if(guide_page(a)+1<MISSION2_GUIDE_PAGE_COUNT)send_action(a,"page",1);}
}
static void frame(void *game,float seconds,const GameInput *input){
 Agent2 *a=game;
 if(!a->loaded&&a->state&&game_port_require_bank(a->port,0)==GAME_RESOURCE_READY){a->loaded=true;present(a);}
 if(a->waiting&&game_port_clock(a->port)-a->sent>10){a->waiting=a->polling=false;a->queued[0]=0;snprintf(a->notice,sizeof a->notice,"No reply. Retry to recover progress.");}
 if(json_is_object(json_object_get(a->state,"ride"))){a->poll+=seconds;if(a->poll>=.1f){a->poll=0;send_action(a,"poll",0);}}
 if(input->down&GAME_KEY_START){game_port_leave(a->port);return;}
 unsigned id=game_port_clicked(a->port);
 if(id==1){if(!a->joined)ready(a);else send_action(a,"state",0);}
 else if(id>=20&&id<23)activate(a,a->page*choice_page_size(a)+id-20);
 else if(id==2)send_action(a,"bag",0);
 else if(id==3)send_action(a,"map",0);
 else if(id==4)send_action(a,"cancel",0);
 else if(id==5){size_t count=json_array_size(entries(a));unsigned size=choice_page_size(a);if(count)a->page=(a->page+1)%((count+size-1)/size);}
 else if(id==6)send_action(a,"use",0);
 else if(id==7)send_action(a,"guide",0);
 else if(id==8)send_action(a,"room:sportShop",0);
 else if(id==9)send_action(a,"room:mountain",0);
 else if(id==10)send_action(a,"medal",0);
 else if(id==11)send_action(a,"letter",0);
 else if(id==12)a->textPage++;
 else if(id==60)guide_step(a,-1);
 else if(id==61)guide_step(a,1);
 else if(id==15)edit_word(a);
 else if(id==19){if(*a->word){char action[32];snprintf(action,sizeof action,"answer:%s",a->word);send_action(a,action,0);}else snprintf(a->wordError,sizeof a->wordError,"Enter the word first.");}
 else if(id==62)send_action(a,"riddle",0);
 else if(id==63&&a->textPage)a->textPage--;
 if(!strcmp(phase(a),"answer")&&(input->down&GAME_KEY_A)&&!a->waiting)edit_word(a);
 if(!strcmp(phase(a),"guide")&&!a->waiting&&!id){if(input->down&GAME_KEY_L)guide_step(a,-1);else if(input->down&GAME_KEY_R)guide_step(a,1);else if(input->down&GAME_KEY_B)send_action(a,"cancel",0);}
 if(!strcmp(phase(a),"room")){
  if(input->down&GAME_KEY_L)send_action(a,"pan",120);
  if(input->down&GAME_KEY_R)send_action(a,"pan",-120);
  if(input->down&GAME_KEY_X)send_action(a,"bag",0);
 }
}
static void tick(void *game){Agent2 *a=game;if(scene(a))swl_tick(scene(a));}
static void touch(void *game,GameTouch touch,float x,float y){Agent2 *a=game;(void)touch;if(json_is_object(json_object_get(a->state,"ride"))&&y<160)send_action(a,"steer",(int)lroundf(x*760/320));}
static unsigned wrap(Agent2 *a,const char *value,float x,float y,unsigned columns,unsigned limit,unsigned skip){
 unsigned row=0;const char *p=value;
 while(*p){char line[96];size_t n=strlen(p);if(n>columns){n=columns;while(n&&p[n]!=' ')n--;if(!n)n=columns;}if(n>95)n=95;memcpy(line,p,n);line[n]=0;
  if(row>=skip&&row<skip+limit)game_port_text(a->port,x,y+(row-skip)*13,10,0x003366,GAME_ALIGN_LEFT,line);
  row++;p+=n;while(*p==' ')p++;
 }return row;
}
static void button(Agent2 *a,unsigned id,float x,float y,float w,float h,const char *label){game_port_button(a->port,id,x,y,w,h,label,!a->waiting||a->polling);}
static unsigned readable(Agent2 *a,const char *value,float x,float y,float width,unsigned size,unsigned limit,unsigned skip){
 if(!value)value="";
 char normalized[2048];unsigned used=0;
 for(unsigned i=0;value[i]&&used<sizeof normalized-1;i++){
  /* Use the crisp bitmap apostrophe for the original English smart quotes. */
  if((unsigned char)value[i]==0xe2&&(unsigned char)value[i+1]==0x80&&((unsigned char)value[i+2]==0x98||(unsigned char)value[i+2]==0x99)){normalized[used++]='\'';i+=2;}
  else normalized[used++]=value[i];
 }
 normalized[used]=0;const char *p=normalized;unsigned row=0,font=(size<FONT_MIN?FONT_MIN:size>FONT_MAX?FONT_MAX:size)-FONT_MIN;
 while(*p){
  char line[96];unsigned n=0,space=0;float advance=0;
  while(p[n]&&p[n]!='\n'&&n<sizeof line-1){
   unsigned code=(unsigned char)p[n];const FontGlyph *glyph=&fontGlyphs[font][(code>=32&&code<127?code:'?')-32];
   if(n&&advance+glyph->advance>width)break;
   advance+=glyph->advance;if(p[n]==' ')space=n;n++;
  }
  if(p[n]&&p[n]!='\n'&&space)n=space;
  if(!n)n=1;
  memcpy(line,p,n);line[n]=0;
  if(row>=skip&&row-skip<limit)game_port_text(a->port,x,y+(row-skip)*(size+4),size,0x003366,GAME_ALIGN_LEFT,line);
  row++;p+=n;while(*p==' '||*p=='\n')p++;
 }
 return row;
}
static unsigned choice_page_size(Agent2 *a){
 if(strcmp(phase(a),"dialog")&&strcmp(phase(a),"confirm"))return 3;
 json_t *choices=entries(a);
 for(size_t i=0;i<json_array_size(choices);i++)if(readable(a,json_string_value(json_array_get(choices,i)),0,0,280,14,0,0)>2)return 1;
 return 2;
}
static void readable_button(Agent2 *a,unsigned id,float x,float y,float w,float h,const char *label,unsigned lines){
 button(a,id,x,y,w,h,"");unsigned rows=readable(a,label,0,0,w-24,14,0,0);if(rows>lines)rows=lines;
 readable(a,label,x+12,y+(h-(rows?rows*18-4:14))/2,w-24,14,lines,0);
}
static unsigned guide_parts(Agent2 *a){unsigned rows=readable(a,MISSION2_GUIDE_PAGES[guide_page(a)].text,0,0,284,14,0,0);return (rows+6)/7;}
static void guide_bottom(Agent2 *a){
 GamePort *p=a->port;unsigned page=guide_page(a),parts=guide_parts(a);
 game_port_text(p,16,8,16,0x003366,GAME_ALIGN_LEFT,"Survival guide");
 game_port_rect(p,6,30,308,140,0xffffff);
 readable(a,MISSION2_GUIDE_PAGES[page].text,16,36,284,14,7,a->textPage*7);
 char progress[64];snprintf(progress,sizeof progress,"Page %u / %u     Text %u / %u",page+1,MISSION2_GUIDE_PAGE_COUNT,a->textPage+1,parts);
 game_port_text(p,16,175,12,0x003366,GAME_ALIGN_LEFT,progress);
 const char *labels[]={"Previous","Next","Close"};
 for(unsigned i=0;i<3;i++){
  bool enabled=i==0?(a->textPage>0||page>0):i==1?(a->textPage+1<parts||page+1<MISSION2_GUIDE_PAGE_COUNT):true;
  float x=8+i*104;
  game_port_button(p,i==0?60:i==1?61:4,x,196,96,36,"",enabled&&(!a->waiting||a->polling));
  game_port_text(p,x+48,207,14,enabled?0x003366:0x526575,GAME_ALIGN_CENTER,labels[i]);
 }
}
static void dialog_bottom(Agent2 *a){
 GamePort *p=a->port;const char *speaker=str(a->state,"speaker");
 game_port_rect(p,6,6,308,98,0xffffff);
 game_port_text(p,16,12,16,0x003366,GAME_ALIGN_LEFT,!strcmp(speaker,"g")?"G":!strcmp(speaker,"agent")?"Agent":"Prototype sled");
 unsigned rows=readable(a,str(a->state,"text"),16,34,284,14,3,a->textPage*3),pages=(rows+2)/3;
 if(pages>1){char page[24];snprintf(page,sizeof page,"%u / %u",a->textPage+1,pages);game_port_text(p,302,12,12,0x003366,GAME_ALIGN_RIGHT,page);}
 if(rows>(a->textPage+1)*3){
  readable_button(a,12,8,120,304,48,"Continue reading",1);
  if(a->textPage)readable_button(a,63,8,184,146,42,"Previous text",1);
  return;
 }
 json_t *choices=entries(a);size_t count=json_array_size(choices);unsigned perPage=choice_page_size(a);
 for(unsigned i=0;i<perPage;i++){
  const char *label=json_string_value(json_array_get(choices,a->page*perPage+i));if(!label)break;
  readable_button(a,20+i,8,112+i*52,304,perPage==1?90:46,label,perPage==1?4:2);
 }
 if(count>perPage)readable_button(a,5,166,212,146,28,"More choices",1);
 if(a->textPage)readable_button(a,63,8,212,146,28,"Previous text",1);
}
static void bottom(void *game){
 Agent2 *a=game;GamePort *p=a->port;const char *mode=phase(a);
 if(!a->state||*a->notice){game_port_rect(p,0,0,320,240,0xe6f3fb);wrap(a,*a->notice?a->notice:"Loading mission...",12,40,46,8,0);if(*a->notice)button(a,1,8,188,304,40,"Retry");return;}
 if(!strcmp(mode,"ride")){game_port_rect(p,0,160,320,80,0xe6f3fb);wrap(a,"Drag to steer the prototype sled. START: exit mission.",10,174,46,4,0);return;}
 game_port_rect(p,0,0,320,240,0xe6f3fb);
 if(!strcmp(mode,"answer")){
  game_port_text(p,16,30,16,0x003366,GAME_ALIGN_LEFT,"The word:");
  readable_button(a,15,112,16,192,46,*a->word?a->word:"Tap to type",2);
  readable_button(a,19,8,86,304,48,"This is the word!",1);
  readable_button(a,62,8,144,304,64,"No, can you give me the riddle again?",2);
  game_port_text(p,16,218,12,0x003366,GAME_ALIGN_LEFT,*a->wordError?a->wordError:"A: type the word.");return;
 }
 if(!strcmp(mode,"dialog")||!strcmp(mode,"confirm")){dialog_bottom(a);return;}
 if(!strcmp(mode,"map")){game_port_text(p,160,20,16,0x003366,GAME_ALIGN_CENTER,"Mission map");button(a,8,8,65,304,42,"Sport Shop");button(a,9,8,116,304,42,"Ski Mountain");button(a,4,8,185,304,40,"Close map");return;}
 if(!strcmp(mode,"guide")){guide_bottom(a);return;}
 if(!strcmp(mode,"complete")){
  game_port_text(p,160,34,17,0x003366,GAME_ALIGN_CENTER,"Mission accomplished!");game_port_text(p,160,68,12,0x003366,GAME_ALIGN_CENTER,"G's Secret Mission");
  button(a,10,8,108,146,44,json_is_true(json_object_get(a->state,"medal"))?"Medal collected":"Get Medal");
  if(flag(a,"gotFish"))button(a,11,166,108,146,44,json_is_true(json_object_get(a->state,"letter"))?"Letter collected":"Get Letter");
  game_port_text(p,160,212,11,0x003366,GAME_ALIGN_CENTER,"START: return to the island");return;
 }
 const char *text=str(a->state,"text");unsigned lines=wrap(a,text,10,5,48,5,a->textPage*5);
 if(lines>(a->textPage+1)*5){button(a,12,8,183,304,40,"Read more");return;}
 json_t *list=entries(a);size_t count=json_array_size(list);
 for(unsigned i=0;i<3;i++){
  json_t *e=json_array_get(list,a->page*3+i);if(!e)break;
  char label[128];
  if(!strcmp(mode,"bag")){char key[16];snprintf(key,sizeof key,"%d",(int)json_integer_value(e));snprintf(label,sizeof label,"%s%s",str(json_object_get(a->state,"itemNames"),key),num(a->state,"held")==json_integer_value(e)?" (selected)":"");}
  else snprintf(label,sizeof label,"%s",!strcmp(mode,"room")?str(e,"label"):json_string_value(e)?json_string_value(e):"");
  button(a,20+i,8,77+i*36,304,33,"");wrap(a,label,16,82+i*36,46,2,0);
 }
 if(count>3)button(a,5,238,190,74,38,"More");
 if(!strcmp(mode,"room")){
  button(a,2,8,190,104,38,"Inventory");
  if(!flag(a,"crashed"))button(a,3,120,190,104,38,"Map");
 }else if(!strcmp(mode,"bag")){
  game_port_button(p,6,8,190,104,38,"Use item",num(a->state,"held")>0&&(!a->waiting||a->polling));button(a,4,120,190,104,38,"Put away");
  json_t *items=json_object_get(a->state,"items");for(size_t i=0;i<json_array_size(items);i++)if(json_integer_value(json_array_get(items,i))==9){button(a,7,8,44,304,28,"Read survival guide");break;}
 }
}
static bool bank_view(void *game,unsigned bank,SWLMatrix *view){(void)view;return !bank&&((Agent2*)game)->loaded;}
static bool lower(void *game,SWLMatrix *view){Agent2 *a=game;if(!a->loaded||!json_is_object(json_object_get(a->state,"ride")))return false;*view=(SWLMatrix){.3333333f,0,0,.3333333f,33.3333f,0};return true;}
static void close_game(void *game){game_port_leave(((Agent2*)game)->port);}
const GameController GAME_AGENT2_CONTROLLER={.key="mission2",.kind=GAME_CARD,.room=908,.backgroundRGB=0xe6f3fb,.banks={"missions/2/sportShop.gpk"},.lazy=1,.order={0,1,2},.bank_resource=resource,.create=create,.destroy=destroy,.ready=ready,.frame=frame,.tick=tick,.touch=touch,.packet=packet,.close=close_game,.draw_bottom=bottom,.bank_view=bank_view,.lower_view=lower};
