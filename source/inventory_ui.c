#include "inventory_ui.h"
#include "asset_cache.h"
#include "wear.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static const char *filters[]={"All items","Colours","Head","Face","Neck","Body","Hand","Feet","Pins","Backgrounds"};
static const char *commands[]={"upc","uph","upf","upn","upb","upa","upe","upl","upp"};
static bool contains(const char *text,const char *query){if(!*query)return true;for(;*text;text++){const char *a=text,*b=query;while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}if(!*b)return true;}return false;}
static bool worn(const Actor *a,const Product *item){return item->type==1?(unsigned)a->colour==item->id:item->type>=2&&item->type<=9&&a->look[item->type-2]==item->id;}
static void shorten(char *out,size_t capacity,const char *text,unsigned limit){snprintf(out,capacity,"%.*s",(int)limit,text);if(strlen(text)>limit&&limit>=3)strcpy(out+limit-3,"...");}
void inventory_prepare(InventoryUI *ui,const ClientData *data,const GPProfile *profile,const Actor *actor){
 if(ui->stamp!=profile->revision){ui->count=0;for(unsigned i=0;i<profile->count;i++){Product item;if(client_product(data,0,profile->inventory[i],&item)&&(!ui->filter||item.type==ui->filter)&&contains(item.name,ui->query))ui->filtered[ui->count++]=item.id;}if(ui->page*6>=ui->count)ui->page=0;ui->stamp=profile->revision;}
 for(unsigned i=0;i<8;i++)if(actor->look[i])wear_want(asset_key(i==6?ASSET_ICON:i==7?ASSET_PHOTO:ASSET_PAPER,actor->look[i]));
 for(unsigned i=ui->page*6;i<ui->count&&i<ui->page*6+6;i++){Product item;if(client_product(data,0,ui->filtered[i],&item)&&item.type!=1)wear_want(asset_key(ASSET_ICON,item.id));}
}
void inventory_draw(InventoryUI *ui,const ClientData *data,const GPProfile *profile,Actor *actor,const InventoryWidgets *w){
 bool rename=w->rename&&pnet_state()==PNET_ONLINE;
 unsigned fill=w->cardFill?w->cardFill(actor->id):0x0085c7,ink=w->cardInk?w->cardInk(actor->id):0xffffff;
 w->box(7,33,133,168,fill);w->center(rename?62:73,40,13,ink,actor->name);
 /* Visible rename button (the whole name row opens the rename flow, see inventory_touch). */
 if(rename){w->box(108,34,30,24,0xffd23f);w->center(123,40,10,0x6b3b00,"Edit");}w->portrait(actor);char label[80];snprintf(label,sizeof label,"%u coins",profile->coins);w->center(73,184,10,ink,pnet_state()==PNET_OFFLINE?"Offline penguin":label);
 w->box(149,34,86,24,0xffffff);w->center(192,40,11,0x003366,filters[ui->filter]);w->box(239,34,73,24,0xffffff);w->center(275,40,11,0x003366,*ui->query?"Clear":"Search");
 for(unsigned i=0;i<6;i++){unsigned index=ui->page*6+i;if(index>=ui->count)break;Product item;if(!client_product(data,0,ui->filtered[index],&item))continue;float x=149+i%2*83,y=62+i/2*36;w->box(x,y,80,33,worn(actor,&item)?0x2fb344:0xffffff);if(item.type==1)w->box(x+3,y+3,24,23,w->colour(item.id));else wear_fit(asset_key(ASSET_ICON,item.id),x+3,y+2,25,28);shorten(label,sizeof label,item.name,9);w->text(x+30,y+10,10,0x003366,label);}
 if(!profile->ready)w->text(152,91,11,0x003366,"Loading inventory...");else if(!ui->count)w->text(152,91,11,0x003366,*ui->query?"No matching items.":"No items here yet.");
 shorten(label,sizeof label,ui->selected,23);w->text(150,171,10,0x003366,label);w->box(149,184,33,19,0xffffff);w->center(165,187,11,0x003366,"<");w->box(279,184,33,19,0xffffff);w->center(295,187,11,0x003366,">");snprintf(label,sizeof label,"%u/%u (%u)",ui->page+1,ui->count?(ui->count+5)/6:1,ui->count);w->center(230,188,10,0x003366,label);
 if(ui->filters){w->box(144,34,170,169,0xffffff);for(unsigned i=0;i<10;i++)w->text(153,40+i*16,11,i==ui->filter?0x0085c7:0x003366,filters[i]);}
}
static void equip(unsigned type,unsigned value,const GPProfile *profile,Actor *actor,const InventoryWidgets *w){
 if(type<1||type>9)return;
 // Offline preview: wear locally whatever the offline store holds (offline_shop.c); nothing is sent.
 if(pnet_state()==PNET_OFFLINE){if(type==1)actor->colour=(int)value;else if(!value||profile_has(profile,value))actor->look[type-2]=value;else w->notice("This item is not in your inventory.");return;}
 if(pnet_state()!=PNET_ONLINE){w->notice("Reconnect to GoPenguin before changing clothing.");return;}
 if(value&&!profile_has(profile,value)){w->notice("This item is not in your inventory.");return;}char command[16],arg[20];snprintf(command,sizeof command,"s#%s",commands[type-1]);snprintf(arg,sizeof arg,"%u",value);if(!pnet_send(command,1,(const char *[]){arg}))w->notice("Could not send the clothing change. Try again.");
}
void inventory_touch(InventoryUI *ui,const ClientData *data,const GPProfile *profile,Actor *actor,const InventoryWidgets *w,int x,int y){
 /* The name at the top of your card opens the same rename flow as Help > Account. */
 if(x<145&&y>=33&&y<62){if(w->rename)w->rename();return;}
 if(ui->filters){if(x>=144&&y>=37&&y<197){ui->filter=(unsigned)(y-37)/16;if(ui->filter>9)ui->filter=9;ui->page=0;ui->stamp=0;}ui->filters=false;return;}
 if(x>=149&&y>=34&&y<58){if(x<235)ui->filters=true;else{if(*ui->query)ui->query[0]=0;else w->keyboard(ui->query,sizeof ui->query,"Search your clothing",false);ui->page=0;ui->stamp=0;}return;}
 if(x>=149&&y>=184&&y<204){if(x<183&&ui->page)ui->page--;if(x>=279&&(ui->page+1)*6<ui->count)ui->page++;return;}
 if(x>=149&&x<312&&y>=62&&y<170){unsigned index=ui->page*6+(unsigned)(y-62)/36*2+(unsigned)(x-149)/83;if(index<ui->count){Product item;if(client_product(data,0,ui->filtered[index],&item)){snprintf(ui->selected,sizeof ui->selected,"%.63s",item.name);equip(item.type,worn(actor,&item)&&item.type!=1?0:item.id,profile,actor,w);}}return;}
 int slot=w->portraitHit(actor,x,y);if(slot>=0&&slot<8&&actor->look[slot])equip((unsigned)slot+2,0,profile,actor,w);
}
