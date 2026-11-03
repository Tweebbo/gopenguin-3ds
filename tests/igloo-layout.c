#include "igloo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool layout(Igloo *i,const char *owner,const char *items){
 char ownerText[32],itemText[IGLOO_LAYOUT_TEXT];snprintf(ownerText,sizeof ownerText,"%s",owner);snprintf(itemText,sizeof itemText,"%s",items);
 char *args[]={ownerText,"2","201","5",itemText};PNetMessage message={.count=5,.args=args};strcpy(message.command,"gm");
 return igloo_apply(i,&message);
}
int main(void){
 Igloo igloo={0};assert(igloo_request(&igloo,42));
 assert(!layout(&igloo,"43","1|300|320|1|1"));assert(igloo.wantedOwner==42&&!igloo.ready);
 assert(layout(&igloo,"42","1|300|320|1|1,2|400|350|2|3"));
 IglooLayout saved=igloo.layout;assert(saved.count==2&&saved.type==2&&saved.floor==5&&saved.music==201);
 /* The room loader pins the selected owner until its required bank is ready. */
 igloo.wantedOwner=42;assert(!layout(&igloo,"43",""));assert(!memcmp(&saved,&igloo.layout,sizeof saved));
 igloo.wantedOwner=0;assert(igloo_begin_edit(&igloo,42));
 assert(igloo_place(&igloo,0,310,330,2,2));saved=igloo.layout;
 assert(!igloo_request(&igloo,43));assert(!layout(&igloo,"42",""));assert(!memcmp(&saved,&igloo.layout,sizeof saved));
 assert(igloo_save_prepare(&igloo));assert(!igloo_request(&igloo,43));
 assert(!layout(&igloo,"43",""));assert(igloo.awaitingSave);
 assert(layout(&igloo,"42",igloo.expected));assert(igloo.saveVerified&&!igloo.dirty&&!igloo.awaitingSave);
 assert(!memcmp(&saved,&igloo.layout,sizeof saved));
 assert(igloo_request(&igloo,43));assert(layout(&igloo,"43",""));assert(igloo.layout.owner==43);
 puts("PASS: selected owner, full furniture layout, style/floor/music, dirty edits and confirmed saves survive navigation.");
}
