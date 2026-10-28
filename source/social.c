#include "social.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned number(const char *s){if(!s||!*s)return 0;char *end;unsigned long n=strtoul(s,&end,10);return *end||n>0x7fffffff?0:(unsigned)n;}
static unsigned split(char *text,char **parts,unsigned limit){unsigned n=1;parts[0]=text;for(char *p=text;*p;p++)if(*p=='|'){*p=0;if(n<limit)parts[n++]=p+1;}return n;}
static Buddy *find(Buddy *list,unsigned count,unsigned id){for(unsigned i=0;i<count;i++)if(list[i].id==id)return &list[i];return NULL;}
static void remove_buddy(Buddy *list,unsigned *count,unsigned id){for(unsigned i=0;i<*count;i++)if(list[i].id==id){memmove(list+i,list+i+1,(--*count-i)*sizeof *list);return;}}
static void people(Buddy *out,unsigned *count,const PNetMessage *m){*count=0;for(unsigned i=0;i<m->count&&*count<SOCIAL_LIMIT;i++){if(strlen(m->args[i])>160)continue;char text[161],*f[3];strcpy(text,m->args[i]);unsigned n=split(text,f,3),id=number(f[0]);if(n<2||!id||find(out,*count,id))continue;Buddy *b=&out[(*count)++];*b=(Buddy){.id=id,.online=n>=3&&!strcmp(f[2],"1")};snprintf(b->name,sizeof b->name,"%s",f[1]);}}
bool social_ignored(const Social *s,unsigned id){for(unsigned i=0;i<s->nIgnored;i++)if(s->ignored[i].id==id)return true;return false;}
const Buddy *social_friend(const Social *s,unsigned id){for(unsigned i=0;i<s->nFriends;i++)if(s->friends[i].id==id)return &s->friends[i];return NULL;}
Actor *social_player(Social *s,unsigned id){for(unsigned i=0;i<s->nPlayers;i++)if(s->players[i].id==id)return &s->players[i];return NULL;}
void social_dismiss(Social *s,unsigned id){remove_buddy(s->requests,&s->nRequests,id);s->revision++;}
static void remember(Social *s,const char *text){Actor a;if(!world_penguin(text,&a))return;Actor *p=social_player(s,a.id);if(!p){if(s->nPlayers==SOCIAL_PLAYERS){memmove(s->players,s->players+1,(SOCIAL_PLAYERS-1)*sizeof *s->players);s->nPlayers--;}p=&s->players[s->nPlayers++];}*p=a;}
void social_apply(Social *s,const PNetMessage *m){
 unsigned id=m->count?number(m->args[0]):0;
 if(!strcmp(m->command,"gb"))people(s->friends,&s->nFriends,m);
 else if(!strcmp(m->command,"gn"))people(s->ignored,&s->nIgnored,m);
 else if(!strcmp(m->command,"go")){for(unsigned i=0;i<s->nFriends;i++){s->friends[i].online=false;for(unsigned j=0;j<m->count;j++)if(s->friends[i].id==number(m->args[j]))s->friends[i].online=true;}}
 else if(!strcmp(m->command,"bon")){Buddy *b=find(s->friends,s->nFriends,id);if(b)b->online=true;}
 else if(!strcmp(m->command,"rb"))remove_buddy(s->friends,&s->nFriends,id);
 else if(!strcmp(m->command,"br")&&id&&m->count>=2&&!social_ignored(s,id)&&!find(s->requests,s->nRequests,id)&&s->nRequests<SOCIAL_LIMIT){Buddy *b=&s->requests[s->nRequests++];*b=(Buddy){.id=id,.online=true};snprintf(b->name,sizeof b->name,"%s",m->args[1]);}
 else if((!strcmp(m->command,"gp")||!strcmp(m->command,"ap")||!strcmp(m->command,"lp"))&&m->count)remember(s,m->args[0]);
 else if(!strcmp(m->command,"jr")){for(unsigned i=1;i<m->count;i++)remember(s,m->args[i]);}
 else if(!strcmp(m->command,"mst"))s->unread=id;
 else if(!strcmp(m->command,"bf"))s->foundRoom=id;
 else if(!strcmp(m->command,"mg")){s->nMail=0;for(unsigned i=0;i<m->count&&s->nMail<SOCIAL_LIMIT;i++){if(strlen(m->args[i])>1023)continue;char text[1024],*f[7];strcpy(text,m->args[i]);if(split(text,f,7)<7)continue;Mail *mail=&s->mail[s->nMail++];*mail=(Mail){.senderId=number(f[1]),.postcard=number(f[2]),.uid=number(f[5]),.read=!strcmp(f[6],"1")};snprintf(mail->sender,sizeof mail->sender,"%s",f[0]);snprintf(mail->details,sizeof mail->details,"%s",f[3]);snprintf(mail->timestamp,sizeof mail->timestamp,"%s",f[4]);}}
 else return;
 s->revision++;
}
