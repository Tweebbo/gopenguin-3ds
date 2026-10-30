#include "world_state.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned number(const char *s,unsigned max){if(!s||!*s)return 0;char *end;unsigned long n=strtoul(s,&end,10);return *end||n>max?0:(unsigned)n;}
Actor *world_actor(Demo *w,unsigned id){return (Actor *)demo_actor(w,id);}
bool world_penguin(const char *text,Actor *a){
 if(strlen(text)>1023)return false;
 char buffer[1024];strcpy(buffer,text);char *fields[20];unsigned count=1;fields[0]=buffer;
 for(char *p=buffer;*p;p++)if(*p=='|'){*p=0;if(count<20)fields[count++]=p+1;}
 if(count<15)return false;
 unsigned id=number(fields[0],0x7fffffff);if(!id)return false;
 *a=(Actor){.id=id,.x=number(fields[12],760),.y=number(fields[13],480),.direction=1,.action=(int)number(fields[14],30),.colour=(int)number(fields[3],255),.member=count>15&&fields[15][0]=='1'};
 if(!a->action)a->action=1;
 a->tx=a->x;a->ty=a->y;snprintf(a->name,sizeof a->name,"%s",fields[1]);
 for(unsigned i=0;i<8;i++)a->look[i]=number(fields[4+i],1000000);
 return true;
}
static void appearance(Demo *w,const char *text,unsigned mine){Actor value;if(!world_penguin(text,&value))return;Actor *a=value.id==mine?&w->actors[0]:world_actor(w,value.id);if(!a){if(w->count>=CAST_COUNT)return;a=&w->actors[w->count++];}*a=value;}

unsigned world_apply(Demo *w,const PNetMessage *m,unsigned mine){
 if(!strcmp(m->command,"lp")&&m->count){appearance(w,m->args[0],mine);return 0;}
 if(!strcmp(m->command,"jr")&&m->count){w->count=1;w->actors[0].id=mine;for(unsigned i=1;i<m->count;i++)appearance(w,m->args[i],mine);return number(m->args[0],0x7fffffff);}
 if(!strcmp(m->command,"ap")&&m->count){appearance(w,m->args[0],mine);return 0;}
 if(!m->count)return 0;
 unsigned id=number(m->args[0],0x7fffffff);Actor *a=world_actor(w,id);if(!a)return 0;
 if(!strcmp(m->command,"rp")&&id!=mine){unsigned i=(unsigned)(a-w->actors);w->actors[i]=w->actors[--w->count];return 0;}
 if(!strcmp(m->command,"sp")&&m->count>=3&&id!=mine){
  float x=number(m->args[1],760),y=number(m->args[2],480),dx=x-a->x,dy=y-a->y;
  int d=(int)lroundf((atan2f(dy,dx)*180.0f/3.14159265f-90)/45);a->direction=(d%8+8)%8+1;a->action=a->direction+8;a->clock=0;a->tx=x;a->ty=y;a->walking=true;
 }else if((!strcmp(m->command,"sa")||!strcmp(m->command,"sf"))&&m->count>=2&&id!=mine){unsigned frame=number(m->args[1],30);if(frame){if(a->walking&&!strcmp(m->command,"sf"))a->pendingFrame=frame;else{a->walking=false;a->action=(int)frame;a->clock=0;a->pendingFrame=0;}}}
 else if(!strcmp(m->command,"sm")&&m->count>=2)actor_say(a,m->args[1]);
 else if(!strcmp(m->command,"se")&&m->count>=2){a->emote=number(m->args[1],30);a->bubbleTime=125.0f/24;a->chat[0]=0;}
 else if(!strcmp(m->command,"upc")&&m->count>=2)a->colour=(int)number(m->args[1],255);
 else{const char *commands[]={"uph","upf","upn","upb","upa","upe","upl","upp"};for(unsigned i=0;i<8;i++)if(!strcmp(m->command,commands[i])&&m->count>=2)a->look[i]=number(m->args[1],1000000);}
 return 0;
}
