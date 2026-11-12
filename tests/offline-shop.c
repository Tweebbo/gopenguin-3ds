#include "offline_shop.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Offline preview store: purchases, style, music and persistence stay local. */
int main(int argc,char **argv){
 const char *path=argc>1?argv[1]:"offline-shop-test.txt";remove(path);
 static OfflineShop shop;offline_load(&shop,path);
 assert(shop.coins==OFFLINE_START_COINS&&shop.igloo==1&&shop.music==0&&shop.nIgloos==1);
 char packet[OFFLINE_LAYOUT+96];
 /* Buying furniture, an igloo style and flooring charges the local balance only. */
 assert(offline_buy(&shop,1,100,500,packet,sizeof packet));assert(!strcmp(packet,"%xt%af%-1%100%9500%"));
 assert(offline_buy(&shop,1,100,500,packet,sizeof packet));
 assert(offline_buy(&shop,2,25,3000,packet,sizeof packet));assert(!offline_buy(&shop,2,25,3000,packet,sizeof packet));
 assert(offline_buy(&shop,3,7,1000,packet,sizeof packet));assert(shop.floor==7&&shop.coins==5000);
 assert(!offline_buy(&shop,1,101,5001,packet,sizeof packet));assert(shop.coins==5000);
 /* Layout, music and the gm packet the igloo handlers read: owner, type, music, floor, layout. */
 assert(offline_set_layout(&shop,"100|300|320|1|1,100|400|330|2|1"));assert(!offline_set_layout(&shop,"100|1|1|1|1%xt%"));
 assert(offline_set_music(&shop,205));assert(!offline_set_music(&shop,2000000));
 assert(offline_igloo_packet(&shop,packet,sizeof packet));
 assert(!strcmp(packet,"%xt%gm%-1%1%1%205%7%100|300|320|1|1,100|400|330|2|1%"));
 /* A style must be owned; changing it returns furniture to storage and removes the floor. */
 assert(!offline_set_style(&shop,40));assert(shop.igloo==1&&shop.layout[0]);
 assert(offline_set_style(&shop,25));assert(shop.igloo==25&&shop.floor==0&&!shop.layout[0]&&shop.music==205);
 assert(offline_furniture_packet(&shop,packet,sizeof packet));assert(!strcmp(packet,"%xt%gf%-1%100|2%"));
 /* Everything survives a reload from SD. */
 assert(offline_save(&shop,path));static OfflineShop again;offline_load(&again,path);
 assert(again.coins==5000&&again.igloo==25&&again.music==205&&again.floor==0&&again.nIgloos==2&&again.nFurniture==1&&again.furniture[0].count==2);
 /* Older files without a music line still load. */
 FILE *f=fopen(path,"w");assert(f);fputs("coins 42\nigloo 1\nfloor 0\nlayout \n",f);fclose(f);
 offline_load(&again,path);assert(again.coins==42&&again.music==0);
 remove(path);puts("offline-shop: ok");return 0;
}
