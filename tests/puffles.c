#include "../source/puffles.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static Puffles pets;
static Demo world;
static struct {unsigned calls,count;char command[20],args[2][80];bool reject;} wire;
static bool send_packet(void *user,const char *command,unsigned count,const char *const *args){
 assert(user==&wire);if(wire.reject)return false;
 assert(count<=2);wire.calls++;wire.count=count;snprintf(wire.command,sizeof wire.command,"%s",command);
 for(unsigned i=0;i<count;i++)snprintf(wire.args[i],sizeof wire.args[i],"%s",args[i]);
 return true;
}
static bool packet(const char *command,unsigned count,char **args){PNetMessage m={.count=count,.args=args};snprintf(m.command,sizeof m.command,"%s",command);return puffles_apply(&pets,&m,&world);}
static void step(unsigned ticks){for(unsigned i=0;i<ticks;i++)puffles_step(&pets,&world,1.0f/24);}
int main(void){
 puffles_init(&pets,send_packet,&wire);world.count=2;world.actors[0]=(Actor){.id=10,.x=400,.y=300};world.actors[1]=(Actor){.id=20,.x=500,.y=330};puffles_room(&pets,10,10);
 assert(puffles_request(&pets,10));assert(!strcmp(wire.command,"p#pg")&&!strcmp(wire.args[0],"10"));assert(!puffles_request(&pets,20));
 assert(packet("pg",2,(char *[]){"1|Blue|0|100|95|90|100|100|100|0|0|0|0","2|Black|2|100|100|100|100|100|100|0|0|0|0"}));assert(pets.count==2);Puffle *blue=puffles_find(&pets,10,1);assert(blue&&blue->food==95&&puffles_visible(&pets,blue,&world));
 assert(packet("pw",2,(char *[]){"20","1|Pink|1|100|100|100|100|100|100|0|0|0|1"}));assert(pets.count==3&&puffles_find(&pets,20,1)!=blue);assert(puffles_visible(&pets,puffles_find(&pets,20,1),&world));
 for(unsigned id=2;id<150;id++){char row[100];snprintf(row,sizeof row,"%u|Pink|1|100|100|100|100|100|100|0|0|0|1",id);assert(packet("pw",2,(char *[]){"20",row}));assert(pets.count==3&&puffles_find(&pets,20,id));}assert(packet("pw",2,(char *[]){"20","1|Pink|1|100|100|100|100|100|100|0|0|0|1"}));
 assert(puffles_walk(&pets,1,true));assert(!strcmp(wire.command,"p#pw")&&!blue->walking);assert(!puffles_care(&pets,1,3));
 assert(packet("pw",2,(char *[]){"10","1|Blue|0|100|95|90|100|100|100|0|0|0|1"}));assert(pets.action==PUFFLE_IDLE&&blue->walking);step(100);assert(hypotf(blue->x-380,blue->y-308)<6);
 assert(puffles_walk(&pets,2,true));assert(packet("pw",2,(char *[]){"10","2|Black|2|100|100|100|100|100|100|0|0|0|1"}));assert(!blue->walking&&puffles_find(&pets,10,2)->walking);
 assert(packet("pw",2,(char *[]){"10","1|Blue|0|100|95|90|100|100|100|0|0|0|0"}));assert(puffles_find(&pets,10,2)->walking);
 assert(puffles_request(&pets,10));assert(puffles_care(&pets,1,3));unsigned coins;
 assert(!puffles_take_coins(&pets,&coins));assert(packet("pcid",3,(char *[]){"10","1|100|100|93|99|0","123"}));assert(puffles_take_coins(&pets,&coins)&&coins==123&&!puffles_take_coins(&pets,&coins));assert(blue->food==100&&blue->rest==93&&blue->health==99&&blue->reaction==3);
 assert(packet("pg",2,(char *[]){"1|Blue|0|1|2|3|100|100|100|0|0|0|0","2|Black|2|100|100|100|100|100|100|0|0|0|1"}));assert(blue->food==100&&blue->health==99&&puffles_find(&pets,10,2)->walking);
 assert(puffles_care(&pets,1,8));assert(packet("pcid",4,(char *[]){"10","1","123","coins"}));assert(blue->food==100&&strstr(pets.error,"coins")&&pets.action==PUFFLE_IDLE);
 assert(puffles_care(&pets,1,37));assert(!packet("pcid",3,(char *[]){"20","1|1|100|2|3|0","0"}));assert(pets.action==PUFFLE_CARE);assert(packet("pcid",3,(char *[]){"10","1|90|100|100|100|0","123"}));step(10);assert(blue->frame==25);step(201);assert(!blue->reaction);
 assert(puffles_request(&pets,10));assert(packet("pw",2,(char *[]){"10","1|Blue|0|100|90|100|100|100|100|0|0|0|1"}));assert(packet("pg",2,(char *[]){"1|Blue|0|100|90|100|100|100|100|0|0|0|0","2|Black|2|100|100|100|100|100|100|0|0|0|1"}));assert(blue->walking&&!puffles_find(&pets,10,2)->walking);assert(packet("pw",2,(char *[]){"10","1|Blue|0|100|90|100|100|100|100|0|0|0|0"}));
 assert(puffles_request(&pets,20));puffles_room(&pets,10,20);assert(pets.count==2&&!puffles_request(&pets,20));assert(packet("pg",1,(char *[]){"1|Stale|1|100|100|100|100|100|100|0|0|0|0"}));assert(!puffles_find(&pets,20,1));
 assert(puffles_request(&pets,20));assert(packet("pg",1,(char *[]){"1|Pink|1|100|100|100|100|100|100|0|0|0|0"}));assert(pets.count==3&&puffles_visible(&pets,puffles_find(&pets,20,1),&world)&&!puffles_visible(&pets,puffles_find(&pets,10,1),&world));
 world.count=1;puffles_departed(&pets,20);assert(puffles_find(&pets,20,1));assert(puffles_visible(&pets,puffles_find(&pets,20,1),&world));
 puffles_room(&pets,10,0);assert(!puffles_find(&pets,20,1));assert(!packet("pw",2,(char *[]){"20","1|Late|1|100|100|100|100|100|100|0|0|0|1"}));
 assert(!puffles_adopt(&pets,9,"Brown"));assert(!puffles_adopt(&pets,0,"   "));assert(!puffles_adopt(&pets,0,"Bad%Name"));assert(puffles_adopt(&pets,0,"New Blue"));assert(!strcmp(wire.command,"p#pcn"));assert(!packet("pcn",2,(char *[]){"Other","1"}));assert(packet("pcn",2,(char *[]){"New Blue","1"}));assert(pets.action==PUFFLE_ADOPT&&!strcmp(wire.command,"p#pn"));assert(!puffles_find(&pets,10,3));
 assert(!packet("pn",2,(char *[]){"0","3|Wrong|0|100|100|100|100|100|100"}));assert(packet("pn",2,(char *[]){"321","3|New Blue|0|100|100|100|100|100|100"}));assert(puffles_find(&pets,10,3)&&puffles_take_coins(&pets,&coins)&&coins==321);
 assert(puffles_adopt(&pets,1,"Pink"));assert(packet("pcn",2,(char *[]){"Pink","0"}));assert(pets.action==PUFFLE_IDLE&&strstr(pets.error,"name"));
 assert(puffles_request(&pets,10));unsigned before=pets.count;assert(packet("pg",1,(char *[]){"1|Malformed|8|100|101|100|100|100|100|0|0|0|0"}));assert(pets.count==before&&strstr(pets.error,"invalid"));
 assert(puffles_request(&pets,10));assert(packet("pg",1,(char *[]){""}));assert(pets.count==0);
 wire.reject=true;assert(!puffles_request(&pets,10)&&!pets.snapshotPending);wire.reject=false;
 assert(puffles_request(&pets,10));step(400);assert(!pets.snapshotPending&&strstr(pets.error,"timed out"));pets.error[0]=0;step(400);assert(!*pets.error);assert(puffles_request(&pets,10));assert(packet("pg",1,(char *[]){""}));assert(!pets.snapshotPending);
 assert(puffles_adopt(&pets,0,"Pending"));step(400);assert(pets.action==PUFFLE_IDLE&&strstr(pets.error,"timed out"));pets.error[0]=0;step(400);assert(!*pets.error);
 puffles_room(&pets,99,0);assert(pets.mine==99&&!pets.count&&pets.action==PUFFLE_IDLE);assert(!strcmp(puffles_colour(8),"orange")&&!puffles_colour(9));
 puffles_init(&pets,send_packet,&wire);puffles_room(&pets,10,500);world.count=CAST_COUNT;
 for(unsigned i=0;i<CAST_COUNT;i++)world.actors[i]=(Actor){.id=i?19+i:10,.x=300+i,.y=300};
 char rows[PUFFLES_OWNED][100],*args[PUFFLES_OWNED];for(unsigned i=0;i<PUFFLES_OWNED;i++){snprintf(rows[i],sizeof rows[i],"%u|Pet%u|%u|100|100|100|100|100|100|0|0|0|0",i+1,i,i%PUFFLES_COLOURS);args[i]=rows[i];}
 assert(puffles_request(&pets,10));assert(packet("pg",PUFFLES_OWNED,args));assert(puffles_request(&pets,500));assert(packet("pg",PUFFLES_OWNED,args));
 for(unsigned i=1;i<CAST_COUNT;i++){char owner[16];snprintf(owner,sizeof owner,"%u",world.actors[i].id);assert(packet("pw",2,(char *[]){owner,"1|Walking|0|100|100|100|100|100|100|0|0|0|1"}));}
 assert(pets.count==2*PUFFLES_OWNED+CAST_COUNT-1&&pets.nWalks==CAST_COUNT+1);
 for(unsigned i=1;i<CAST_COUNT;i++){char owner[16];snprintf(owner,sizeof owner,"%u",world.actors[i].id);assert(packet("pw",2,(char *[]){owner,"2|Replacement|0|100|100|100|100|100|100|0|0|0|1"}));assert(puffles_find(&pets,world.actors[i].id,2));}
 assert(pets.count==2*PUFFLES_OWNED+CAST_COUNT-1);step(240);for(unsigned i=0;i<pets.count;i++)assert(isfinite(pets.pets[i].x)&&isfinite(pets.pets[i].y));
 assert(puffles_request(&pets,500));assert(packet("pg",1,(char *[]){""}));assert(pets.count==PUFFLES_OWNED+CAST_COUNT-1&&puffles_find(&pets,10,1)&&!puffles_find(&pets,500,1));
 assert(puffles_request(&pets,10));assert(packet("pg",1,(char *[]){""}));assert(pets.count==CAST_COUNT-1&&!puffles_find(&pets,10,1));puffles_room(&pets,10,0);assert(!pets.count);assert(puffles_request(&pets,10));assert(packet("pg",1,(char *[]){""}));assert(!pets.count&&!pets.snapshotPending);
 printf("PASS puffle owner/ID collisions, serial snapshots, stale rooms, care races, walking replacement, adoption, authoritative coins and limits (%zu bytes)\n",sizeof pets);return 0;
}
