#include "offline_shop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fresh(OfflineShop *s){memset(s,0,sizeof *s);s->coins=OFFLINE_START_COINS;s->igloo=1;s->igloos[s->nIgloos++]=1;}
static bool has(const unsigned *list,unsigned count,unsigned id){for(unsigned n=0;n<count;n++)if(list[n]==id)return true;return false;}
bool offline_has_item(const OfflineShop *s,unsigned id){return has(s->items,s->nItems,id);}
bool offline_has_igloo(const OfflineShop *s,unsigned id){return has(s->igloos,s->nIgloos,id);}
static bool valid_layout(const char *text){
 // Only furniture entries (digits, '|' and ','): the text goes back into a server-format packet.
 for(const char *p=text;*p;p++)if(!((*p>='0'&&*p<='9')||*p=='|'||*p==','||*p=='-'))return false;
 return true;
}
void offline_load(OfflineShop *s,const char *path){
 fresh(s);FILE *f=path?fopen(path,"r"):NULL;if(!f)return;
 char line[OFFLINE_LAYOUT+32];bool igloos=false;
 while(fgets(line,sizeof line,f)){
  line[strcspn(line,"\r\n")]=0;unsigned a,b;
  if(sscanf(line,"coins %u",&a)==1)s->coins=a;
  else if(sscanf(line,"igloo %u",&a)==1&&a)s->igloo=a;
  else if(sscanf(line,"floor %u",&a)==1)s->floor=a;
  else if(sscanf(line,"item %u",&a)==1&&a&&s->nItems<OFFLINE_ITEMS&&!offline_has_item(s,a))s->items[s->nItems++]=a;
  else if(sscanf(line,"owned-igloo %u",&a)==1&&a){if(!igloos){s->nIgloos=0;igloos=true;}if(s->nIgloos<OFFLINE_IGLOOS&&!offline_has_igloo(s,a))s->igloos[s->nIgloos++]=a;}
  else if(sscanf(line,"furniture %u %u",&a,&b)==2&&a&&b&&s->nFurniture<OFFLINE_FURNITURE)s->furniture[s->nFurniture++]=(OfflineFurniture){a,b};
  else if(!strncmp(line,"layout ",7)&&strlen(line+7)<OFFLINE_LAYOUT&&valid_layout(line+7))strcpy(s->layout,line+7);
 }
 fclose(f);if(!offline_has_igloo(s,s->igloo))s->igloo=s->igloos[0];
}
bool offline_save(const OfflineShop *s,const char *path){
 char temp[256];if(snprintf(temp,sizeof temp,"%s.tmp",path)>=(int)sizeof temp)return false;
 FILE *f=fopen(temp,"w");if(!f)return false;
 fprintf(f,"# GoPenguin offline preview store. Delete this file to reset it.\ncoins %u\nigloo %u\nfloor %u\n",s->coins,s->igloo,s->floor);
 for(unsigned n=0;n<s->nItems;n++)fprintf(f,"item %u\n",s->items[n]);
 for(unsigned n=0;n<s->nIgloos;n++)fprintf(f,"owned-igloo %u\n",s->igloos[n]);
 for(unsigned n=0;n<s->nFurniture;n++)fprintf(f,"furniture %u %u\n",s->furniture[n].id,s->furniture[n].count);
 fprintf(f,"layout %s\n",s->layout);
 bool ok=!ferror(f);ok=fclose(f)==0&&ok;
 if(!ok){remove(temp);return false;}
 remove(path);return rename(temp,path)==0;
}
bool offline_buy(OfflineShop *s,unsigned kind,unsigned id,unsigned cost,char *packet,size_t cap){
 if(!id||kind>3||cost>s->coins)return false;
 if(kind==0&&(offline_has_item(s,id)||s->nItems>=OFFLINE_ITEMS))return false;
 if(kind==2&&(offline_has_igloo(s,id)||s->nIgloos>=OFFLINE_IGLOOS))return false;
 if(kind==1){unsigned n=0;while(n<s->nFurniture&&s->furniture[n].id!=id)n++;if(n==s->nFurniture){if(n>=OFFLINE_FURNITURE)return false;s->furniture[s->nFurniture++]=(OfflineFurniture){id,0};}s->furniture[n].count++;}
 s->coins-=cost;
 if(kind==0)s->items[s->nItems++]=id;
 if(kind==2)s->igloos[s->nIgloos++]=id;
 if(kind==3)s->floor=id;
 static const char *const replies[]={"ai","af","au","ag"};
 int n=snprintf(packet,cap,"%%xt%%%s%%-1%%%u%%%u%%",replies[kind],id,s->coins);return n>0&&(size_t)n<cap;
}
static bool append(char *packet,size_t cap,size_t *used,const char *text){size_t n=strlen(text);if(*used+n+2>=cap)return false;memcpy(packet+*used,text,n);*used+=n;packet[(*used)++]='%';packet[*used]=0;return true;}
bool offline_inventory_packet(const OfflineShop *s,char *packet,size_t cap){
 int head=snprintf(packet,cap,"%%xt%%gi%%-1%%");if(head<0||(size_t)head>=cap)return false;size_t used=(size_t)head;
 for(unsigned n=0;n<s->nItems;n++){char id[16];snprintf(id,sizeof id,"%u",s->items[n]);if(!append(packet,cap,&used,id))return false;}
 return true;
}
bool offline_furniture_packet(const OfflineShop *s,char *packet,size_t cap){
 int head=snprintf(packet,cap,"%%xt%%gf%%-1%%");if(head<0||(size_t)head>=cap)return false;size_t used=(size_t)head;
 for(unsigned n=0;n<s->nFurniture;n++){char entry[32];snprintf(entry,sizeof entry,"%u|%u",s->furniture[n].id,s->furniture[n].count);if(!append(packet,cap,&used,entry))return false;}
 return true;
}
bool offline_igloo_packet(const OfflineShop *s,char *packet,size_t cap){
 int n=snprintf(packet,cap,"%%xt%%gm%%-1%%%u%%%u%%0%%%u%%%s%%",OFFLINE_OWNER,s->igloo,s->floor,s->layout);return n>0&&(size_t)n<cap;
}
bool offline_set_layout(OfflineShop *s,const char *layout){if(strlen(layout)>=OFFLINE_LAYOUT||!valid_layout(layout))return false;strcpy(s->layout,layout);return true;}
