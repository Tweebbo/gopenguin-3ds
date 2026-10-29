#include <3ds.h>
#include <citro2d.h>
#include <tex3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <float.h>
#include <sys/stat.h>
#include "demo.h"
#include "animations.h"
#include "penguin_atlas.h"
#include "font.h"
#include "viewport.h"
#include "font_layout.h"
#include "swl.h"
#include "swl_gpu.h"
#include "island.h"
#include "penguin_net.h"
#include "offline_shop.h"
#include "chat_keyboard.h"
#include "chat_log.h"
#include "chat_log_view.h"
#include "world_state.h"
#include "settings.h"
#include "transport/conn.h"
#include "transport/ws.h"
#include "asset_cache.h"
#include "wear.h"
#include "client_data.h"
#include "profile.h"
#include "membership.h"
#include "../vendor/qrcodegen/qrcodegen.h"
#include "gp_update.h"
#ifdef GP_NINJA_GAMES
#include "game_card_preview.h"
#include "room_npc.h"
#endif
#include "inventory_ui.h"
#include "paper_registration.h"
#include "emotes.h"
#include "overlay.h"
#include "map.h"
#include "books.h"
#include "movie_bank.h"
#include "social.h"
#include "postcards.h"
#include "igloo.h"
#include "igloo_geometry.h"
#include "gp_audio.h"
#include "puffles.h"
#include "newspaper.h"
#include "game_port.h"
#include "room_tube.h"
#include "epf_access.h"
#include "lobby.h"
#include "lobby_geometry.h"
#include "ui_theme.h"
#include <jansson.h>

typedef struct {C3D_Tex tex;Tex3DS_SubTexture sub;bool loaded;} Texture;
#define MAP_LOW_TOP 28
#define MAP_LOW_VISIBLE 177
static Texture icons,emotes,islandMap,islandMapLow,fontTexture,paperBody,paperDetail,minimap,bookCovers,body[PENGUIN_PAGES],detail[PENGUIN_PAGES],sceneAtlas[SWL_MAX_PAGES];
static SWLScene *scene;
static bool sceneOK=true;
static bool isNew3DS;
static C3D_RenderTarget *sceneTop,*sceneBottom;
static IslandNav *navigation;
static const IslandRoom *currentRoom;
static const IslandRoom iglooRoomInfo={.key="igloo",.title="Igloo"};
static unsigned currentRoomId=100,wantedIglooOwner;
static bool waitingIgloo,wantedIgloo;
static Igloo igloo;
static struct {unsigned page,type;bool preparing;u64 started;} styles;
static Puffles puffles;
static unsigned petPage,petSelected,wantedPetOwner,renderedIglooType,petRoom;
static unsigned musicPage;
static u64 musicStarted;
static Newspaper newspaper;
static GamePort *game;
static RoomTube roomTube;
static EpfAccess epfAccess;
static bool epfQuery;
static Lobby lobby;
static struct {bool open,moving,returning,artDirty,refresh,leavePending,raceReturn;unsigned routeRoom,cancelledWaddle;int cancelledSeat;float walkTime;const LobbyGeometrySeat *point;} lobbyUI;
static GameDescriptor queuedGame;
static bool gameQueued,gameLeaving,gameSuspended,gameMultiplayerLost;
static unsigned gameReturnRoom;
static int gameReturnX,gameReturnY,gameTouchX,gameTouchY;
static unsigned gamePage;
static const char *gameKeys[]={"hydro","roundup","beans","cart","thinice","fish","four","mancala","sled","pizzatron"
#ifdef GP_NINJA_GAMES
 ,"card","fire"
#endif
};
static const char *gameNames[]={"Hydro Hopper","Puffle Roundup","Bean Counters","Cart Surfer","Thin Ice","Ice Fishing","Find Four","Mancala","Sled Racing","Pizzatron 3000"
#ifdef GP_NINJA_GAMES
 ,"Card-Jitsu","Card-Jitsu Fire"
#endif
};
static IglooGeometry *iglooGeometry;
static MovieBank *roomBank;
static float furnitureTime[IGLOO_ITEMS];
static SWLArt lastFloorArt;
static bool floorPresent;
static struct {int selected;bool dragging,moved,inventory;int oldX,oldY;float pressX,pressY;unsigned page,category,revision,count,items[IGLOO_OWNED];char search[61];u64 saveAt;} editor={.selected=-1};
static unsigned wantedRoom,mapPage;
static unsigned activeTrigger=~0u;
#ifdef GP_NINJA_GAMES
static RoomNpc roomNpc;
#endif
static float wantedX=-1,wantedY=-1,triggerDelay;
static bool serverRoom;
static float interpolation=1;
static Settings settings;
static ClientData *clientData;
static GPProfile profile;
static Social social;
static unsigned peopleKind,peoplePage,selectedPlayer,mailPage,postcardPage,postcardRecipient,postcardId;
static unsigned iglooPage;
static BookInfo resourceInfo={.frames=1,.key="",.title="POSTCARD"};
static InventoryUI inventoryUI;
static InventoryWidgets inventoryWidgets;
static json_t *shellText;
static struct {bool open,confirm;unsigned purchase,kind,room;char command[20],argument[32];char text[1024];} modal;
static ChatKeyboard chatKeyboard;
static ChatLog chatLog;
static ChatLogView chatHistory;
static C2D_TextBuf chatGlyphs,chatMeasure;
static unsigned roomInventoryStamp;
static Overlay overlay;
static bool aiming,emoteMenu,actionPosition;
static bool mapList;
#ifdef GP_LAN_ONLY
static bool loadTesting;
#endif
static struct {const BookInfo *info;MovieBank *bank;unsigned page,wanted,item,colour,symbol,secrets,resourceId,mailUid;char sender[49];bool open,close,roomReleased,sportPink,zoom,boundsReady;float bounds[4];float clock,x,y;} book;
static unsigned hoveredIndex=~0u,hoverFirst,hoverCount;
static unsigned safeParent,safePage,bookListPage;
static void content(const char *name);
static void open_igloo(unsigned owner);
static void editor_open(void);
static void editor_draw(void);
static void editor_touch(int x,int y);
static void editor_drag(u32 held,u32 up);
static void editor_save(void);
static void editor_turn(int rotation,int variation);
static void editor_remove(void);
static void igloo_list_open(void);
static void igloo_list_draw(void);
static void igloo_list_touch(int x,int y);
static void styles_open(void);
static void styles_draw(void);
static void styles_touch(int x,int y);
static void styles_update(void);
static void pets_draw(void);
static void pets_touch(int x,int y);
static void pets_adopt(unsigned type);
static void pets_enter(unsigned owner);
static void music_open(void);
static void music_draw(void);
static void music_touch(int x,int y);
static void game_start(const char *key);
static void game_transition(void);
static void games_draw(void);
static void games_touch(int x,int y);
static bool lobby_locked(void);
static bool lobby_before_room(void);
static void lobby_room_enter(unsigned room);
static void lobby_prompt(LobbyKind kind,unsigned id);
static void lobby_join(LobbyKind kind,unsigned id);
static void lobby_draw(void);
static void lobby_touch(int x,int y);
static void lobby_close(void);
static void lobby_update(float seconds);
static void lobby_progress(float seconds);
static void lobby_art(void);
static bool lobby_apply(const PNetMessage *packet);
static void lobby_disconnected(void);
static GameInput game_input(u32 down,u32 held,const circlePosition *pad);
static void people_touch(int x,int y);
static void people_draw(void);
static void player_draw(void);
static void player_touch(int x,int y);
static void mail_draw(void);
static void mail_touch(int x,int y);
static void postcards_draw(void);
static void postcards_touch(int x,int y);
#define SETTINGS_FILE DATA_DIR "/settings.json"
#ifndef GOPENGUIN_VERSION
#define GOPENGUIN_VERSION "dev"
#endif

static Demo demo;
static const unsigned colours[]={0x003366,0x003366,0x009900,0xff3399,0x333333,0xcc0000,0xff6600,0xffcc00,0x660099,0x996600,0xff6666,0x006600,0x0099cc,0x8ae302,0x93a0a4,0x02a797};
static const char *tabs[]={"Room","Map","Me","People","Help"};
static const int tabIcons[]={6,8,4,5,7};
static bool membershipPaid;
/* Staff badges (gpstaff after login), moderator notices (gpmsg) and Captain Rockhopper's logbook. */
#define ROCKHOPPER_ID 0x7fffff00u
static unsigned staffIds[64],staffCount;static char staffRole[64];
static u64 kickResume;static bool kickStop;
static float fps=60,frameMS;static char notice[100];static float noticeTime;
static u32 rgb(unsigned hex){return C2D_Color32(hex>>16,(hex>>8)&255,hex&255,255);}
static bool load(Texture *t,const char *name){char path[120];snprintf(path,sizeof path,"romfs:/gfx/%s.t3x",name);FILE *f=fopen(path,"rb");if(!f)return false;Tex3DS_Texture packed=Tex3DS_TextureImportStdio(f,&t->tex,NULL,false);fclose(f);if(!packed)return false;if(Tex3DS_GetNumSubTextures(packed)!=1){Tex3DS_TextureFree(packed);C3D_TexDelete(&t->tex);return false;}t->sub=*Tex3DS_GetSubTexture(packed,0);Tex3DS_TextureFree(packed);C3D_TexSetFilter(&t->tex,GPU_LINEAR,GPU_LINEAR);C3D_TexSetWrap(&t->tex,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);t->loaded=true;return true;}
static void image(Texture *t,float x,float y,float w,float h,u32 colour){if(!t->loaded)return;C2D_Image im={&t->tex,&t->sub};C2D_ImageTint tint;C2D_PlainImageTint(&tint,colour,1);C2D_DrawImageAt(im,x,y,.5f,&tint,w/t->sub.width,h/t->sub.height);}
static void crop(Texture *t,int sx,int sy,int sw,int sh,float x,float y,float scale,u32 colour){if(!t->loaded)return;Tex3DS_SubTexture sub={.width=sw,.height=sh,.left=t->sub.left+(float)sx/t->tex.width,.top=t->sub.top-(float)sy/t->tex.height,.right=t->sub.left+(float)(sx+sw)/t->tex.width,.bottom=t->sub.top-(float)(sy+sh)/t->tex.height};C2D_Image im={&t->tex,&sub};C2D_ImageTint tint;C2D_PlainImageTint(&tint,colour,1);C2D_DrawImageAt(im,x,y,.5f,&tint,scale,scale);}
typedef struct {struct {unsigned tag;float depth;} items[CAST_COUNT+IGLOO_ITEMS+PUFFLES_LIMIT];unsigned count,next;float scale,ox,oy;} ActorOrder;
static void make_order(ActorOrder *order,bool players,float scale,float ox,float oy);
static bool actors_before(void *user,float depth);
static bool actors_needed(void *user,float depth);
static void scene_draw_to(C3D_RenderTarget *target,unsigned width,unsigned height,float scaleX,float scaleY,float ox,float oy,ActorOrder *order){
 SWLGPUAtlas atlases[SWL_MAX_PAGES];for(unsigned i=0;i<swl_pages(scene);i++)atlases[i]=(SWLGPUAtlas){&sceneAtlas[i].tex,&sceneAtlas[i].sub};
 if(sceneOK){SWLMatrix view={scaleX,0,0,scaleY,ox,oy};if(roomBank)sceneOK=movie_bank_draw_depth(roomBank,target,width,height,view,interpolation,order?actors_before:NULL,order?actors_needed:NULL,order,false);else sceneOK=swl_gpu_draw(scene,target,width,height,view,-1,interpolation,atlases,order?actors_before:NULL,order?actors_needed:NULL,order,false);}
 C2D_ViewReset();C2D_SetTintMode(C2D_TintMult);
}
static void rect(float x,float y,float w,float h,unsigned colour){C2D_DrawRectSolid(x,y,.5f,w,h,rgb(colour));}
static void box(float x,float y,float w,float h,unsigned fill){ui_box(x,y,w,h,fill);}
static unsigned fontRow(unsigned size){return (size<FONT_MIN?FONT_MIN:size>FONT_MAX?FONT_MAX:size)-FONT_MIN;}
static const FontGlyph *glyph(unsigned row,unsigned code){return &fontGlyphs[row][(code>=32&&code<127?code:'?')-32];}
static float chat_font_width(unsigned size,const char *s);
static void chat_font_text(float x,float y,unsigned size,unsigned colour,const char *s);
static bool unicode_text(const char *s){for(;*s;s++)if((unsigned char)*s>=128)return true;return false;}
static int textWidth(unsigned size,const char *s){if(unicode_text(s))return (int)ceilf(chat_font_width(size,s));int width=0;unsigned row=fontRow(size);for(;*s;s++)width+=glyph(row,(unsigned char)*s)->advance;return width;}
static void text(float x,float y,unsigned size,unsigned colour,const char *s){
 if(unicode_text(s)){chat_font_text(x,y,size,colour,s);return;}
 unsigned row=fontRow(size);int pen=lroundf(x),baseline=lroundf(y)+fontAscents[row];
 for(;*s;s++){
  const FontGlyph *g=glyph(row,(unsigned char)*s);
  if(g->w&&g->h)crop(&fontTexture,g->x,g->y,g->w,g->h,pen+g->left,baseline+g->top,1,rgb(colour));
  pen+=g->advance;
 }
}
static void center(float x,float y,unsigned size,unsigned colour,const char *s){text(x-textWidth(size,s)/2.0f,y,size,colour,s);}
static bool chat_font_ready(void){
 if(!chatGlyphs)chatGlyphs=C2D_TextBufNew(4096);
 if(!chatMeasure)chatMeasure=C2D_TextBufNew(512);
 return chatGlyphs&&chatMeasure;
}
static float chat_font_width(unsigned size,const char *s){
 if(!chat_font_ready())return strlen(s)*size*.6f;
 C2D_Text parsed;float width=0;C2D_TextBufClear(chatMeasure);C2D_TextParse(&parsed,chatMeasure,s);C2D_TextGetDimensions(&parsed,size/30.0f,size/30.0f,&width,NULL);return width;
}
static void chat_font_text(float x,float y,unsigned size,unsigned colour,const char *s){
 if(!chat_font_ready()){text(x,y,size,colour,"?");return;}
 C2D_Text parsed;C2D_TextParse(&parsed,chatGlyphs,s);C2D_DrawText(&parsed,C2D_WithColor,x,y,.5f,size/30.0f,size/30.0f,rgb(colour));
}
static void chat_font_center(float x,float y,unsigned size,unsigned colour,const char *s){chat_font_text(x-chat_font_width(size,s)/2,y,size,colour,s);}
static void icon(int i,float x,float y,float scale){crop(&icons,i*32,0,32,32,x,y,scale,0xffffffff);}
static void actor(Actor *a,float x,float y,float scale){int action=a->action>=1&&a->action<=30?a->action:1;Sequence seq=sequences[action];unsigned n=seq.start+((unsigned)a->clock%seq.count);const PenguinFrame *f=&penguinFrames[n];C2D_SetTintMode(C2D_TintSolid);crop(&body[0],f->x,f->y,f->w,f->h,x+f->left*scale,y+f->top*scale,scale,rgb(colours[(unsigned)a->colour<16?(unsigned)a->colour:0]));C2D_SetTintMode(C2D_TintMult);crop(&detail[0],f->x,f->y,f->w,f->h,x+f->left*scale,y+f->top*scale,scale,0xffffffff);const unsigned slots[]={5,3,2,4,1,0};for(unsigned i=0;i<6;i++)wear_draw(a->look[slots[i]],(unsigned)action,(unsigned)a->clock,x,y,scale);}
static float actor_depth(unsigned index){return 901+index+1000*floorf(demo.actors[index].y);}
static void make_order(ActorOrder *order,bool players,float scale,float ox,float oy){
 memset(order,0,sizeof *order);order->scale=scale;order->ox=ox;order->oy=oy;
 if(roomBank)for(unsigned i=0;i<igloo.layout.count;i++){IglooFurniture *f=&igloo.layout.items[i];Product product;unsigned type=client_product(clientData,1,f->id,&product)?product.type:1;order->items[order->count].tag=CAST_COUNT+i;order->items[order->count++].depth=type==2?6+f->y/1000.0f:type==3?7+f->y/1000.0f:900+1000*floorf(f->y);}
 if(players&&!igloo.editing)for(unsigned i=0;i<demo.count;i++){order->items[order->count].tag=i;order->items[order->count++].depth=actor_depth(i);}
 if(players&&!igloo.editing)for(unsigned i=0;i<puffles.count;i++)if(puffles_visible(&puffles,&puffles.pets[i],&demo)){order->items[order->count].tag=CAST_COUNT+IGLOO_ITEMS+i;order->items[order->count++].depth=900+1000*floorf(puffles.pets[i].y);}
 for(unsigned i=1;i<order->count;i++){unsigned j=i;while(j&&order->items[j-1].depth>order->items[j].depth){typeof(order->items[0]) swap=order->items[j-1];order->items[j-1]=order->items[j];order->items[j]=swap;j--;}}
}
static bool actors_needed(void *user,float depth){ActorOrder *order=user;return order->next<order->count&&order->items[order->next].depth<=depth;}
static bool actors_before(void *user,float depth){
 ActorOrder *order=user;
 while(actors_needed(user,depth)){
  unsigned n=order->items[order->next++].tag;
  if(n>=CAST_COUNT+IGLOO_ITEMS){Puffle *p=&puffles.pets[n-CAST_COUNT-IGLOO_ITEMS];float x=p->oldX+(p->x-p->oldX)*interpolation,y=p->oldY+(p->y-p->oldY)*interpolation;wear_draw_pose(asset_key(ASSET_PUFFLE,p->type+1),p->frame,1,p->clock,order->ox+x*order->scale,order->oy+y*order->scale,order->scale);continue;}
  if(n>=CAST_COUNT){unsigned index=n-CAST_COUNT;IglooFurniture *f=&igloo.layout.items[index];wear_draw_pose(asset_key(ASSET_FURNITURE,f->id),f->rotation,f->frame,(unsigned)(furnitureTime[index]*24),order->ox+f->x*order->scale,order->oy+f->y*order->scale,order->scale);continue;}
  Actor *a=&demo.actors[n];float x=order->ox+a->x*order->scale,y=order->oy+a->y*order->scale;
  if(!n)C2D_DrawEllipseSolid(x-10*order->scale,y-3*order->scale,.4f,20*order->scale,6*order->scale,rgb(0x00ccff));
  actor(a,x,y,order->scale);
 }
 return true;
}
static void message(const char *s){snprintf(notice,sizeof notice,"%.99s",s);noticeTime=4;}
static void room_inventory(void){if(!navigation||!scene)return;for(unsigned i=0;i<island_events(navigation);i++){IslandEvent event=island_event(navigation,i);if(event.type==1&&(!event.id||profile_has(&profile,event.id)))swl_target(scene,event.path,event.frame,NULL,event.playing);}roomInventoryStamp=profile.inventoryRevision;}
static bool room_textures(const IslandRoom *room){
 if(roomBank&&room==&iglooRoomInfo)return movie_bank_reload_textures(roomBank);
 bool ok=true;unsigned bytes=0;for(unsigned i=0;i<room->pages;i++){char name[64];snprintf(name,sizeof name,"swl-%s-%u",room->key,i);if(!load(&sceneAtlas[i],name))return false;bytes+=sceneAtlas[i].tex.size;if(bytes>6u*1024u*1024u||sceneAtlas[i].tex.width!=SWL_ATLAS_WIDTH||sceneAtlas[i].tex.height!=SWL_ATLAS_HEIGHT)return false;}
 char name[64];
#ifdef GP_NINJA_GAMES
 snprintf(name,sizeof name,room_npc_spec(room->id)?GP_NPC_MINIMAP_FORMAT:"mini-low-%s",room->key);
#else
 snprintf(name,sizeof name,"mini-low-%s",room->key);
#endif
 return load(&minimap,name)&&ok;
}
static unsigned tube_frame(void *unused,const char *path){(void)unused;return scene?swl_frame(scene,path):0;}
static void tube_target(void *unused,const char *path,unsigned frame,const char *label,bool playing){(void)unused;if(scene)swl_target(scene,path,frame,label,playing);}
static const RoomTubeHooks tubeHooks={.frame=tube_frame,.target=tube_target};
static void epf_target(void *unused,const char *path,unsigned frame,bool playing){tube_target(unused,path,frame,NULL,playing);}
static const EpfAccessHooks epfHooks={.target=epf_target};
static void room_latch_entry(float x,float y){IslandTrigger trigger;activeTrigger=epf_access_trigger(&epfAccess,(int)lroundf(x),(int)lroundf(y),&trigger)?trigger.index:~0u;}
static void release_room_textures(void){if(roomBank)movie_bank_unload_textures(roomBank);for(unsigned i=0;i<SWL_MAX_PAGES;i++)if(sceneAtlas[i].loaded){C3D_TexDelete(&sceneAtlas[i].tex);sceneAtlas[i].loaded=false;}if(minimap.loaded){C3D_TexDelete(&minimap.tex);minimap.loaded=false;}}
static bool enter_room(unsigned id,float x,float y){
 if(!lobby_before_room())return false;
 if(igloo.dirty||igloo.awaitingSave||igloo.awaitingStyle||igloo.awaitingMusic||styles.preparing){message("Save or finish your igloo changes before leaving.");return false;}
 const IslandRoom *room=island_room(id);if(!room){message("That room is still being prepared.");return false;}
 const char *error=NULL;char file[96];snprintf(file,sizeof file,"romfs:/scenes/%s.nav",room->key);IslandNav *nav=island_load(file,&error);if(!nav){message(error?error:"Could not load this room.");return false;}
 snprintf(file,sizeof file,"romfs:/scenes/%s.swl",room->key);SWLScene *next=swl_load_file(file,&error);if(!next){island_free(nav);message(error?error:"Could not load this scene.");return false;}
 release_room_textures();
 if(!room_textures(room)){release_room_textures();if(currentRoom)room_textures(currentRoom);swl_free(next);island_free(nav);message("Could not load this room's artwork.");return false;}
 if(roomBank){movie_bank_free(roomBank);roomBank=NULL;}else swl_free(scene);igloo_geometry_free(iglooGeometry);iglooGeometry=NULL;igloo.editing=false;island_free(navigation);scene=next;navigation=nav;currentRoom=room;currentRoomId=id;sceneOK=true;
 if(x<0||y<0)island_start(nav,&x,&y);
 demo.blocked=epf_access_blocked;demo.room=&epfAccess;demo.mask=NULL;
 demo.actors[0].x=demo.actors[0].tx=x;demo.actors[0].y=demo.actors[0].ty=y;demo.actors[0].walking=false;demo.actors[0].clock=0;demo.actors[0].action=1;triggerDelay=1;
 if(!serverRoom){demo.count=1;if(pnet_state()==PNET_ONLINE)pnet_join(id,(int)x,(int)y);}
 serverRoom=false;
 activeTrigger=~0u;memset(&overlay,0,sizeof overlay);
 hoveredIndex=~0u;hoverCount=0;
 gp_audio_scene(room->key);gp_audio_room(room->key);swl_sound_handler(scene,gp_audio_sound,NULL);swl_restart(scene);
 epf_access_enter(&epfAccess,navigation,id,x,y,profile.epfReady,profile.epfAgent,&epfHooks);
 if(id==212)swl_visible(scene,"entryTriggers",false);
 room_latch_entry(x,y);
 room_tube_enter(&roomTube,id);room_inventory();pets_enter(0);lobby_room_enter(id);
#ifdef GP_NINJA_GAMES
 if(!room_npc_enter(&roomNpc,id,scene,"romfs:/npcs"))message("Could not load the Sensei interaction.");
#endif
 return true;
}
static int floor_content(void *unused,const char *path,unsigned id,SWLArt *art,unsigned capacity){
 (void)unused;(void)id;if(!iglooGeometry||!igloo.layout.floor||strcmp(path,"background_mc.igloo_floor_mc"))return -1;SWLGPUAtlas atlas;SWLArt frame;if(!wear_pose_view(asset_key(ASSET_FLOOR,igloo.layout.floor),iglooGeometry->floorFrame,1,(unsigned)(demo.time*24),&atlas,&frame))return -1;if(!frame.w||!frame.h)return 0;if(!capacity)return 1;frame.page=SWL_MAX_PAGES;art[0]=frame;return 1;
}
static void floor_update(void){
 if(!roomBank){floorPresent=false;return;}SWLGPUAtlas atlas={0};SWLArt art={0};bool present=iglooGeometry&&igloo.layout.floor&&wear_pose_view(asset_key(ASSET_FLOOR,igloo.layout.floor),iglooGeometry->floorFrame,1,(unsigned)(demo.time*24),&atlas,&art);movie_bank_extra(roomBank,0,atlas);if(present!=floorPresent||(present&&memcmp(&lastFloorArt,&art,sizeof art))){floorPresent=present;lastFloorArt=art;swl_invalidate(scene);}
}
static bool enter_pending_igloo(void){
 if(!lobby_before_room()){wantedIgloo=false;return false;}
 char path[256];unsigned type=igloo.layout.type?igloo.layout.type:1;Product product;
 if(type!=1&&!client_product(clientData,2,type,&product)){wantedIgloo=false;message("This igloo style is not available.");return false;}
 if(type==1)snprintf(path,sizeof path,"romfs:/data/basic-igloo.gpk");else{unsigned key=asset_key(ASSET_MOVIE,200000+type);if(!asset_cache_request(key,path,sizeof path)){if(asset_cache_status(key)==ASSET_UNAVAILABLE){wantedIgloo=false;message("This igloo is not available on the resources server.");}return false;}}
 release_room_textures();const char *error=NULL;MovieBank *bank=movie_bank_load(path,6u*1024u*1024u,&error);IglooGeometry *geometry=bank?igloo_geometry_load(movie_bank_meta(bank),&error):NULL;
 if(!bank||!geometry){movie_bank_free(bank);igloo_geometry_free(geometry);sceneOK=room_textures(currentRoom);wantedIgloo=false;message(error?error:"Could not load the igloo.");return false;}
 if(roomBank)movie_bank_free(roomBank);else swl_free(scene);igloo_geometry_free(iglooGeometry);island_free(navigation);roomBank=bank;scene=movie_bank_scene(bank);iglooGeometry=geometry;movie_bank_content(bank,floor_content,NULL);floorPresent=false;memset(furnitureTime,0,sizeof furnitureTime);navigation=geometry->navigation;geometry->navigation=NULL;
 currentRoom=&iglooRoomInfo;currentRoomId=igloo.layout.owner+2000;island_set_id(navigation,currentRoomId);sceneOK=true;demo.blocked=island_blocked;demo.room=navigation;demo.mask=NULL;float x,y;island_start(navigation,&x,&y);Actor *me=&demo.actors[0];me->x=me->tx=x;me->y=me->ty=y;me->walking=false;me->action=me->direction=1;me->clock=0;triggerDelay=1;activeTrigger=hoveredIndex=~0u;hoverCount=0;memset(&overlay,0,sizeof overlay);
 if(!serverRoom){demo.count=1;if(pnet_state()==PNET_ONLINE)pnet_join(currentRoomId,(int)x,(int)y);}serverRoom=false;wantedIgloo=false;demo.tab=0;renderedIglooType=type;
#ifdef GP_NINJA_GAMES
 room_npc_free(&roomNpc);
#endif
 room_tube_enter(&roomTube,currentRoomId);pets_enter(igloo.layout.owner);gp_audio_scene(type==1?"igloo1":"");gp_audio_music(igloo.layout.music);swl_sound_handler(scene,gp_audio_sound,NULL);swl_restart(scene);epf_access_enter(&epfAccess,navigation,currentRoomId,x,y,profile.epfReady,profile.epfAgent,&epfHooks);room_latch_entry(x,y);room_inventory();lobby_room_enter(currentRoomId);return true;
}
// Offline preview economy (offline_shop.c): without a session, purchases and the owner's igloo use
// a local store on SD; its replies are queued as server packets for the normal handlers.
#define OFFLINE_SHOP_FILE DATA_DIR "/offline.txt"
static OfflineShop offlineShop;static bool offlineShopReady;
static char *localPackets[8];static unsigned localCount;
static bool offline_active(void){return offlineShopReady&&pnet_state()==PNET_OFFLINE&&!settings.session[0];}
static unsigned local_player(void){return offline_active()?OFFLINE_OWNER:pnet_id();}
static void local_push(const char *text){char *copy=localCount<8?strdup(text):NULL;if(copy)localPackets[localCount++]=copy;}
static bool local_receive(PNetMessage *m){while(localCount){char *text=localPackets[0];memmove(localPackets,localPackets+1,--localCount*sizeof *localPackets);if(pnet_parse(text,m)){m->storage=text;return true;}free(text);}return false;}
#ifdef GP_LAN_ONLY
static void offline_begin(void){
 offline_load(&offlineShop,OFFLINE_SHOP_FILE);offlineShopReady=true;
 profile.coins=offlineShop.coins;profile.revision++;
 char *packet=malloc(OFFLINE_ITEMS*12+32);if(packet&&offline_inventory_packet(&offlineShop,packet,OFFLINE_ITEMS*12+32))local_push(packet);free(packet);
}
#endif
static bool offline_igloo_apply(void){
 static char packet[OFFLINE_LAYOUT+96];PNetMessage m;
 if(!offline_igloo_packet(&offlineShop,packet,sizeof packet)||!pnet_parse(packet,&m))return false;
 bool ok=igloo_apply(&igloo,&m);m.storage=NULL;pnet_message_free(&m);return ok;
}
static void open_igloo(unsigned owner){
 if(!lobby_before_room())return;
 if(pnet_state()==PNET_OFFLINE&&!owner){igloo.wantedOwner=0;if(!offline_active()||!offline_igloo_apply()){igloo.layout=(IglooLayout){.type=1};igloo.ready=true;}wantedIgloo=true;wantedIglooOwner=0;return;}
 if(pnet_state()!=PNET_ONLINE){message("Connect to GoPenguin to visit an igloo.");return;}
 if(!igloo_request(&igloo,owner)){message("Save your igloo changes before leaving.");return;}
 char id[20];snprintf(id,sizeof id,"%u",owner);if(!pnet_send_to('r',"gm",1,(const char *[]){id})){message("Could not request the igloo.");return;}wantedRoom=0;wantedIglooOwner=owner;waitingIgloo=true;message("Loading igloo...");
}
static SWLMatrix book_view(unsigned width,unsigned height,float y,bool zoom){
 if(book.resourceId&&book.bank&&book.boundsReady){const float *b=book.bounds;if(b[2]>b[0]&&b[3]>b[1]){float scale=fminf((width-16)/(b[2]-b[0]),(height-16)/(b[3]-b[1]));if(zoom&&book.zoom)scale*=1.6f;return (SWLMatrix){.a=scale,.d=scale,.x=width/2.0f-(zoom&&book.zoom?book.x:(b[0]+b[2])/2)*scale,.y=y+height/2.0f-(zoom&&book.zoom?book.y:(b[1]+b[3])/2)*scale};}}
 float scale=fminf(width/760.0f,height/480.0f);if(zoom&&book.zoom)scale*=1.6f;return (SWLMatrix){.a=scale,.d=scale,.x=width/2.0f-(zoom&&book.zoom?book.x:380)*scale,.y=y+height/2.0f-(zoom&&book.zoom?book.y:240)*scale};
}
static void top(void){
 if(game){game_port_draw_top(game,sceneTop);return;}
 if(book.open){rect(0,0,400,240,0xffffff);if(book.bank){movie_bank_draw(book.bank,sceneTop,400,240,book_view(400,240,0,true),book.clock*swl_rate(movie_bank_scene(book.bank)));C2D_ViewReset();C2D_SetTintMode(C2D_TintMult);}return;}
 if(demo.tab==1&&!mapList&&islandMap.loaded){image(&islandMap,26,0,347,240,0xffffffff);return;}
 Viewport view=viewport_cover(400,240,760,480,demo.zoom?demo.actors[0].x:380,demo.zoom?demo.actors[0].y-60:240,demo.zoom?1.2f:1);
 float scale=view.scale,ox=view.x,oy=view.y;
 ActorOrder order;make_order(&order,true,scale,ox,oy);scene_draw_to(sceneTop,400,240,scale,scale,ox,oy,&order);
 for(unsigned i=0;!igloo.editing&&i<demo.count;i++){Actor *a=&demo.actors[i];center(ox+a->x*scale,oy+a->y*scale+2,10,0x003366,a->name);}

 for(unsigned i=0;!igloo.editing&&i<64;i++){float x,y;if(overlay_ball(&overlay.balls[i],&x,&y)){C2D_DrawCircleSolid(ox+x*scale,oy+y*scale,.5f,3.3f,rgb(0x7d8b99));C2D_DrawCircleSolid(ox+x*scale,oy+y*scale,.5f,2.5f,rgb(0xffffff));}}
 for(unsigned i=0;!igloo.editing&&i<demo.count;i++){Actor *a=&demo.actors[i];if(a->bubbleTime<=0)continue;if(a->emote>=1&&a->emote<=30){const EmoteArt *f=&emoteArt[a->emote-1];crop(&emotes,f->x,f->y,f->w,f->h,ox+(a->x+f->left)*scale,oy+(a->y+f->top)*scale,scale*2,0xffffffff);continue;}float x=fmaxf(80,fminf(320,ox+a->x*scale)),y=fmaxf(30,oy+a->y*scale-43);char label[145];snprintf(label,sizeof label,"%.144s",a->chat+a->chatOffset);char *separator=strchr(label,'|');if(separator)*separator=0;if(strlen(label)>45)strcpy(label+42,"...");float w=fminf(376,textWidth(11,label)+12);box(x-w/2,y,w,23,0xffffff);center(x,y+5,11,0x003366,label);}
 if(currentRoom){const char *title=currentRoom->title;ui_label(5,5,textWidth(11,title)+34,21);text(29,9,11,0xffffff,title);}
 if(demo.stats){char s[80];snprintf(s,sizeof s,"%.0f FPS  |  %.1f ms  |  %lu KB free",fps,frameMS,(unsigned long)(linearSpaceFree()/1024));box(4,219,392,18,0x003366);text(9,221,11,0xffffff,s);}
 if(noticeTime>0){box(12,188,376,25,0xffffff);center(200,194,11,0x003366,notice);}
}
static void button_label(float x,float y,unsigned size,unsigned colour,const char *label){FontBounds b=font_ink(size,label);text(x-(b.left+b.right)/2.0f,y-(b.top+b.bottom)/2.0f,size,colour,label);}
static void button_state(float x,float y,float w,float h,const char *label,int ico,bool enabled){float dy=ui_button(x,y,w,h,enabled&&ui_touching(x,y,w,h),enabled)?1:0;if(ico>=0){float pixels=fminf(25,h-15);icon(ico,x+(w-pixels)/2,y+2+dy,pixels/32);}button_label(x+w/2,y+(ico>=0?h-7:h/2)+dy,11,enabled?0xffffff:0x003366,label);}
static void button(float x,float y,float w,float h,const char *label,int ico){button_state(x,y,w,h,label,ico,true);}
static void account_button(float x,float y,float w,float h,const char *label,int ico){
 float dy=ui_button(x,y,w,h,ui_touching(x,y,w,h),true)?1:0;
 icon(ico,x+7,y+(h-20)/2+dy,.625f);button_label(x+(w+25)/2,y+h/2+dy,11,0xffffff,label);
}
static void transfer_symbol(float x,float y,bool fromPC){
 float pc=x+(fromPC?0:34),penguin=x+(fromPC?34:0);
 rect(pc,y+3,22,15,0xffffff);rect(pc+2,y+5,18,11,0x003366);
 rect(pc+9,y+18,4,3,0xffffff);rect(pc+5,y+21,12,2,0xffffff);
 icon(2,penguin,y,.75f);text(x+25,y+7,11,0xffffff,">");
}
static void transfer_button(float y,bool fromPC,const char *label,const char *description){
 float dy=ui_button(8,y,304,49,ui_touching(8,y,304,49),true)?1:0;
 transfer_symbol(18,y+12+dy,fromPC);text(88,y+9+dy,12,0xffffff,label);text(88,y+28+dy,9,0xffffff,description);
}
static unsigned item_colour(unsigned id){return colours[id<16?id:0];}
static void portrait(Actor *a){
 const float factor=112.0f/144,scale=PAPER_SCALE*factor,x=18+PAPER_X*factor,y=52+PAPER_Y*factor;
 if(a->look[7])wear_fit(asset_key(ASSET_PHOTO,a->look[7]),11,52,125,130);
 image(&paperBody,18,52,112,176*factor,rgb(colours[(unsigned)a->colour<16?(unsigned)a->colour:0]));image(&paperDetail,18,52,112,176*factor,0xffffffff);
 const unsigned slots[]={5,3,2,4,1,0};for(unsigned i=0;i<6;i++)wear_draw(asset_key(ASSET_PAPER,a->look[slots[i]]),1,0,x,y,scale);
 if(a->look[6])wear_fit(asset_key(ASSET_ICON,a->look[6]),14,69,22,22);
}
static int portrait_hit(const Actor *a,int px,int py){const float factor=112.0f/144,scale=PAPER_SCALE*factor,x=18+PAPER_X*factor,y=52+PAPER_Y*factor;const unsigned slots[]={0,1,4,2,3,5};if(a->look[6]&&px>=14&&px<36&&py>=69&&py<91)return 6;for(unsigned i=0;i<6;i++)if(a->look[slots[i]]&&wear_contains(asset_key(ASSET_PAPER,a->look[slots[i]]),x,y,scale,px,py))return (int)slots[i];if(a->look[7]&&px>=11&&px<136&&py>=52&&py<182)return 7;return -1;}
static void wrapped(float x,float y,unsigned size,unsigned colour,const char *source,unsigned width,unsigned lines){
 const char *cursor=source;for(unsigned line=0;line<lines&&*cursor;line++){char buffer[160];unsigned n=0,lastSpace=0;while(cursor[n]&&cursor[n]!='\n'&&n<sizeof buffer-1){buffer[n]=cursor[n];buffer[n+1]=0;if(textWidth(size,buffer)>(int)width)break;if(cursor[n]==' ')lastSpace=n;n++;}if(cursor[n]&&cursor[n]!='\n'&&lastSpace)n=lastSpace;if(!n)n=1;memcpy(buffer,cursor,n);buffer[n]=0;text(x,y+line*14,size,colour,buffer);cursor+=n;while(*cursor==' '||*cursor=='\n')cursor++;}
}
static void modal_message(const char *text){modal.open=true;modal.confirm=false;modal.purchase=modal.room=0;modal.command[0]=0;snprintf(modal.text,sizeof modal.text,"%.1023s",text);}
static void modal_draw(void){box(7,34,306,169,0xffffff);center(160,42,13,0x003366,modal.confirm?"Confirm":"Message");wrapped(18,64,11,0x003366,modal.text,284,7);if(modal.confirm){button(25,174,120,25,"Yes",-1);button(175,174,120,25,"No",-1);}else button(100,174,120,25,"Okay",-1);}
static void buy_kind(unsigned kind,unsigned id){Product product;if(pnet_state()!=PNET_ONLINE&&!offline_active()){message("Connect to GoPenguin to buy items.");return;}if(!client_product(clientData,kind,id,&product)){message("This item is not available.");return;}if(!kind&&profile_has(&profile,id)){modal_message("You already have this item.");return;}if(profile.coins<product.cost){modal_message("You need more coins.");return;}modal.open=modal.confirm=true;modal.purchase=id;modal.kind=kind;modal.room=0;modal.command[0]=0;if(!product.cost)snprintf(modal.text,sizeof modal.text,"You have found a %s.\nWould you like to pick it up?",product.name);else snprintf(modal.text,sizeof modal.text,"Would you like to buy %s for %u coins?\nYou have %u coins.",product.name,product.cost,profile.coins);}
static void buy(unsigned id){buy_kind(0,id);}
static void book_page(void *unused,unsigned frame){
 (void)unused;
 if(newspaper.open){if(!newspaper_goto(&newspaper,frame))return;unsigned resource=newspaper_resource(&newspaper);if(resource!=book.resourceId){book.resourceId=resource;book.wanted=frame;}else if(book.bank){swl_target(movie_bank_scene(book.bank),"news#1",frame,NULL,false);book.page=frame;}return;}
 if(book.info&&frame&&frame<=book.info->frames&&frame!=book.page)book.wanted=frame;
}
static void book_toggle_zoom(bool reset){
 if(!book.bank)return;
 book.zoom=!book.zoom;if(reset){book.x=book.resourceId&&book.boundsReady?(book.bounds[0]+book.bounds[2])/2:380;book.y=book.resourceId&&book.boundsReady?(book.bounds[1]+book.bounds[3])/2:240;}
#ifdef GP_LAN_ONLY
 if(!book.resourceId){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"catalogue zoom=%u id=%u\n",book.zoom,book.info->id);fclose(trace);}}
#endif
}
static void book_buy(void *unused,unsigned kind,unsigned id){(void)unused;buy_kind(kind,id);}
static void book_close(void *unused){(void)unused;book.close=true;}
static void book_adopt(void *unused,unsigned type){(void)unused;pets_adopt(type);}
static bool book_custom(void *unused,unsigned id,const char *path){
 if(newspaper.open){const char *notice=newspaper_notice(id);if(notice){modal_message(notice);return true;}return false;}
 (void)unused;(void)id;const char *name=strrchr(path,'.');name=name?name+1:path;SWLScene *movie=movie_bank_scene(book.bank);
 if(book.info->id==1&&book.page==3){
  if((name[0]=='c'||name[0]=='s')&&name[1]>='0'&&name[1]<='4'&&!name[2]){if(*name=='c')book.colour=name[1]-'0';else book.symbol=name[1]-'0';swl_target(movie,"book.tshirtbuilt",book.colour*5+book.symbol+1,NULL,false);return true;}
  if(!strcmp(name,"tshirtbuy_btn")){static const unsigned shirts[5][5]={{4241,4242,4158,4177,4176},{4245,4246,4163,4178,4179},{4247,4248,4168,4180,4181},{4243,4244,4174,4171,4175},{4249,4250,4253,4251,4252}};buy(shirts[book.colour][book.symbol]);return true;}
 }
 if(book.info->id==6&&book.page==2){
  if((name[0]=='i'||name[0]=='c'||name[0]=='s')&&name[1]>='0'&&name[1]<='4'&&!name[2]){if(*name=='i')book.item=name[1]-'0';else if(*name=='c')book.colour=name[1]-'0';else book.symbol=name[1]-'0';swl_target(movie,"book.itembuilt",book.item*15+book.colour*5+book.symbol+1,NULL,false);return true;}
  if(!strcmp(name,"furnbuy_btn")){buy_kind(1,1001+book.item*15+book.colour*5+book.symbol);return true;}
 }
 if(book.info->id==2&&book.page==7){
  if(!strcmp(path,"book.penguin")){book.sportPink=!book.sportPink;book.secrets=0;swl_visible(movie,"book.penguin.pink",book.sportPink);swl_visible(movie,"book.star.btn",book.sportPink);swl_visible(movie,"book.shell.btn",book.sportPink);swl_visible(movie,"book.penguin.fire",!book.sportPink);swl_visible(movie,"book.penguin.norm_face",true);const char *hidden[]={"book.penguin.silver","book.penguin.happy_face","book.silver_mc","book.star.active","book.shell.active"};for(unsigned i=0;i<COUNT(hidden);i++)swl_visible(movie,hidden[i],false);return true;}
  if(!strcmp(path,"book.star.btn")||!strcmp(path,"book.shell.btn")){unsigned bit=!strcmp(path,"book.star.btn")?1:2;book.secrets^=bit;swl_visible(movie,bit==1?"book.star.active":"book.shell.active",!!(book.secrets&bit));if(book.secrets==3){const char *shown[]={"book.penguin.silver","book.penguin.happy_face","book.silver_mc"},*hidden[]={"book.penguin.pink","book.penguin.fire","book.penguin.norm_face"};for(unsigned i=0;i<3;i++){swl_visible(movie,shown[i],true);swl_visible(movie,hidden[i],false);}}return true;}
 }
 return false;
}
static void book_room(void *unused,const char *key){(void)unused;const IslandRoom *room=island_key(key);if(room){wantedRoom=room->id;wantedX=wantedY=-1;book.close=true;}}
static const MovieHooks bookHooks={.custom=book_custom,.page=book_page,.buy=book_buy,.adopt=book_adopt,.close=book_close,.room=book_room};
static void book_open(const char *key){for(unsigned i=0;i<BOOK_COUNT;i++)if(!strcmp(bookInfo[i].key,key)){book.info=&bookInfo[i];book.resourceId=book.mailUid=0;book.sender[0]=0;book.open=true;book.close=false;book.wanted=1;book.page=0;book.clock=0;book.zoom=false;book.x=380;book.y=240;demo.actors[0].walking=false;demo.actors[0].tx=demo.actors[0].x;demo.actors[0].ty=demo.actors[0].y;return;}modal_message("This catalogue is not available yet.");}
static void book_update(void){
#ifdef GP_LAN_ONLY
 if(book.close&&newspaper.open){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fputs("newspaper closed\n",trace);fclose(trace);}}
 if(book.close&&book.open&&!book.resourceId){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"catalogue closed id=%u\n",book.info->id);fclose(trace);}}
#endif
 if(book.close){newspaper_close(&newspaper);movie_bank_free(book.bank);book.bank=NULL;book.open=book.close=false;book.wanted=0;if(book.roomReleased){if(!room_textures(currentRoom)){release_room_textures();sceneOK=false;message("Could not restore the room artwork.");}book.roomReleased=false;}return;}
 if(!book.open||!book.wanted)return;
 char path[256];unsigned key=book.resourceId?asset_key(ASSET_MOVIE,book.resourceId):asset_key(ASSET_BOOK,book.info->id*1024+book.wanted);
 bool embedded=false;if(!book.resourceId){snprintf(path,sizeof path,"romfs:/books/%u.gpk",book.info->id*1024+book.wanted);FILE *file=fopen(path,"rb");if(file){fclose(file);embedded=true;}}
 if(!embedded&&!asset_cache_request(key,path,sizeof path)){if(asset_cache_status(key)==ASSET_UNAVAILABLE){modal_message("This resource is not available on the resources server.");book.wanted=0;if(!book.bank)book.close=true;}return;}
 movie_bank_free(book.bank);book.bank=NULL;
 if(!book.roomReleased){release_room_textures();book.roomReleased=true;}
 const char *error=NULL;book.bank=movie_bank_load(path,6u*1024u*1024u,&error);
 if(!book.bank){modal_message(error?error:"Could not load this catalogue.");book.close=true;return;}
 if(book.resourceId&&*book.sender)movie_bank_text(book.bank,".name_txt",book.sender);
 if(newspaper.open)swl_target(movie_bank_scene(book.bank),"news#1",newspaper.frame,NULL,false);
#ifdef GP_LAN_ONLY
 if(newspaper.open){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"newspaper page=%u frame=%u resource=%u\n",newspaper.page,newspaper.frame,book.resourceId);fclose(trace);}}
#endif
 book.boundsReady=book.resourceId&&swl_bounds(movie_bank_scene(book.bank),book.bounds);if(book.boundsReady){book.x=(book.bounds[0]+book.bounds[2])/2;book.y=(book.bounds[1]+book.bounds[3])/2;}
 book.page=book.wanted;book.wanted=0;book.clock=0;book.item=book.colour=book.symbol=book.secrets=0;book.sportPink=false;
#ifdef GP_LAN_ONLY
 if(!book.resourceId){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"catalogue id=%u page=%u embedded=%u\n",book.info->id,book.page,embedded);fclose(trace);}}
#endif
}
static void resource_open(unsigned id,const char *sender,unsigned uid){if(!newspaper.open){resourceInfo.frames=1;resourceInfo.title="POSTCARD";}book.info=&resourceInfo;book.resourceId=id;book.mailUid=uid;snprintf(book.sender,sizeof book.sender,"%.48s",sender);book.open=true;book.close=false;book.wanted=1;book.page=0;book.clock=0;book.zoom=false;book.x=380;book.y=240;demo.actors[0].walking=false;}
static void newspaper_show(void){postcardId=0;newspaper_open(&newspaper);resource_open(newspaper_resource(&newspaper),"",0);resourceInfo.frames=NEWSPAPER_FRAMES;resourceInfo.title="CLUB PENGUIN TIMES";}
static void confirm_command(const char *text,const char *command,unsigned id){modal_message(text);modal.confirm=true;snprintf(modal.command,sizeof modal.command,"%s",command);snprintf(modal.argument,sizeof modal.argument,"%u",id);}
static Actor *selected_actor(void){Actor *a=world_actor(&demo,selectedPlayer);return a?a:social_player(&social,selectedPlayer);}
static void rockhopper_open(void);
static void open_player(unsigned id){if(id==pnet_id()){demo.tab=2;return;}if(id==ROCKHOPPER_ID){rockhopper_open();return;}selectedPlayer=id;demo.tab=7;char value[20];snprintf(value,sizeof value,"%u",id);pnet_send("u#gp",1,(const char *[]){value});}
static char staff_role(unsigned id){for(unsigned i=0;i<staffCount;i++)if(staffIds[i]==id)return staffRole[i];return 0;}
/** A = administrator (gold), M = moderator (blue), next to the name on the card. */
static void staff_badge(float x,float y,unsigned id){char role=staff_role(id);if(role!='a'&&role!='m')return;box(x,y,18,18,role=='a'?0xffd23f:0x1a78c9);center(x+9,y+3,11,role=='a'?0x6b3b00:0xffffff,role=='a'?"A":"M");}
/* Captain Rockhopper stands in Town (local actor, never sent to the server). */
static void rockhopper_sync(void){
 Actor *npc=world_actor(&demo,ROCKHOPPER_ID);
 if(currentRoomId!=100){if(npc&&npc!=&demo.actors[0])*npc=demo.actors[--demo.count];return;}
 if(npc||!demo.count||demo.count>=CAST_COUNT)return;
 Actor *a=&demo.actors[demo.count++];
 *a=(Actor){.id=ROCKHOPPER_ID,.x=262,.y=366,.tx=262,.ty=366,.direction=1,.action=1,.colour=5};
 a->look[0]=442;a->look[1]=152;a->look[2]=161;a->look[3]=4946;a->look[4]=5020;snprintf(a->name,sizeof a->name,"Captain Rockhopper");
 lobbyUI.artDirty=true;
}
static bool rockhopper_near(void){Actor *npc=world_actor(&demo,ROCKHOPPER_ID);if(!npc)return false;float dx=npc->x-demo.actors[0].x,dy=npc->y-demo.actors[0].y;return dx*dx+dy*dy<70.0f*70.0f;}
static bool room_npc_action(void){
#ifdef GP_NINJA_GAMES
 const char *game=demo.tab?NULL:room_npc_near(&roomNpc,demo.actors[0].x,demo.actors[0].y);if(game){game_start(game);return true;}
#endif
 return false;
}
static void rockhopper_open(void){
 if(pnet_state()!=PNET_ONLINE||!settings.session[0]){modal_message("Ahoy, matey! Connect to GoPenguin to write in my logbook.");return;}
 if(pnet_account_state()==PNET_ACCOUNT_BUSY){message("Captain Rockhopper is still reading your last message.");return;}
 pnet_send("u#rh",0,NULL);
 char text[1024]="";SwkbdState kb;swkbdInit(&kb,SWKBD_TYPE_NORMAL,3,500);
 swkbdSetHintText(&kb,"Ahoy! A suggestion or bug for the GoPenguin crew");
 swkbdSetButton(&kb,SWKBD_BUTTON_LEFT,"Cancel",false);swkbdSetButton(&kb,SWKBD_BUTTON_MIDDLE,"Bug report",true);swkbdSetButton(&kb,SWKBD_BUTTON_RIGHT,"Suggestion",true);
 swkbdSetValidation(&kb,SWKBD_NOTEMPTY_NOTBLANK,0,0);swkbdSetFeatures(&kb,SWKBD_MULTILINE);
 gp_audio_pause(true);SwkbdButton pressed=swkbdInputText(&kb,text,sizeof text);gp_audio_pause(false);
 if(pressed!=SWKBD_BUTTON_MIDDLE&&pressed!=SWKBD_BUTTON_RIGHT)return;
 if(strlen(text)<10){modal_message("Write at least 10 characters, matey.");return;}
 char id[33];u64 seed=osGetTime()^svcGetSystemTick();for(unsigned i=0;i<32;i++){seed=seed*6364136223846793005ull+1442695040888963407ull;id[i]="0123456789abcdef"[(seed>>59)&15];}id[32]=0;
 if(pnet_suggest(&settings.network,settings.session,pressed==SWKBD_BUTTON_MIDDLE?"bug":"suggestion",text,id))message("Sending your message to Captain Rockhopper...");
 else modal_message("Captain Rockhopper could not take your message right now. Try again.");
}
static void people_draw(void){
 const char *labels[]={"Room","Buddies","Ignored","Requests"};for(unsigned i=0;i<4;i++){box(5+i*79,32,76,23,i==peopleKind?0x0085c7:0xffffff);center(43+i*79,37,11,i==peopleKind?0xffffff:0x003366,labels[i]);}
 unsigned count=peopleKind==0?demo.count:peopleKind==1?social.nFriends:peopleKind==2?social.nIgnored:social.nRequests;if(peoplePage*6>=count)peoplePage=count?(count-1)/6:0;
 for(unsigned row=0;row<6&&peoplePage*6+row<count;row++){unsigned index=peoplePage*6+row;const Buddy *b=peopleKind==1?&social.friends[index]:peopleKind==2?&social.ignored[index]:peopleKind==3?&social.requests[index]:NULL;const char *name=b?b->name:demo.actors[index].name;box(9,59+row*21,302,20,0xffffff);text(17,61+row*21,11,0x003366,name);center(283,61+row*21,10,0x003366,b&&!b->online?"Offline":peopleKind==3?"Reply":"Online");}
 if(!count)center(160,105,11,0x003366,peopleKind==3?"No buddy requests.":"No penguins here.");
 button(9,187,75,18,"Previous",-1);button(236,187,75,18,"Next",-1);char status[48];snprintf(status,sizeof status,"%u total",count);center(160,190,10,0x003366,status);
}
static void people_touch(int x,int y){
 if(y>=32&&y<55){peopleKind=(unsigned)x/79;if(peopleKind>3)peopleKind=3;peoplePage=0;if(peopleKind==1)pnet_send("b#gb",0,NULL);if(peopleKind==2)pnet_send("n#gn",0,NULL);return;}
 unsigned count=peopleKind==0?demo.count:peopleKind==1?social.nFriends:peopleKind==2?social.nIgnored:social.nRequests;
 if(y>=187){if(x<85&&peoplePage)peoplePage--;if(x>=236&&(peoplePage+1)*6<count)peoplePage++;return;}
 if(y<59||y>=185)return;
 unsigned index=peoplePage*6+(unsigned)(y-59)/21;if(index>=count)return;
 if(peopleKind==3){Buddy b=social.requests[index];char prompt[140];snprintf(prompt,sizeof prompt,"%s wants to be your buddy. Accept?",b.name);confirm_command(prompt,"b#ba",b.id);social_dismiss(&social,b.id);}else open_player(peopleKind==0?demo.actors[index].id:peopleKind==1?social.friends[index].id:social.ignored[index].id);
}
static void player_draw(void){
 Actor *a=selected_actor();if(!a){center(160,100,12,0x003366,"Loading penguin...");return;}
 box(8,33,304,22,0x0085c7);center(160,37,12,0xffffff,a->name);staff_badge(12,35,a->id);portrait(a);
 const Buddy *buddy=social_friend(&social,a->id);button(146,62,163,27,buddy?"Remove buddy":"Add buddy",-1);button(146,95,163,27,social_ignored(&social,a->id)?"Stop ignoring":"Ignore penguin",-1);button(146,128,163,27,"Send postcard",-1);button(146,161,79,27,"Find",-1);button(230,161,79,27,"Igloo",-1);
}
static void player_touch(int x,int y){
 Actor *a=selected_actor();if(!a||x<146)return;char prompt[180];
 if(y>=62&&y<89){bool buddy=social_friend(&social,a->id)!=NULL;snprintf(prompt,sizeof prompt,buddy?"Remove %s from your buddy list?":"Add %s to your buddy list?",a->name);confirm_command(prompt,buddy?"b#rb":"b#br",a->id);}
 else if(y>=95&&y<122){bool ignored=social_ignored(&social,a->id);snprintf(prompt,sizeof prompt,ignored?"Stop ignoring %s?":"Ignore %s?",a->name);confirm_command(prompt,ignored?"n#rn":"n#an",a->id);}
 else if(y>=128&&y<155){postcardRecipient=a->id;postcardPage=0;demo.tab=9;}
 else if(y>=161&&y<188){if(x<230){char id[20];snprintf(id,sizeof id,"%u",a->id);pnet_send("b#bf",1,(const char *[]){id});}else{char command[40];snprintf(command,sizeof command,"igloo:%u",a->id);content(command);}}
}
static const char *postcard_subject(unsigned id){for(unsigned i=0;i<POSTCARD_COUNT;i++)if(postcardInfo[i].id==id)return postcardInfo[i].subject;return "Postcard";}
static void mail_draw(void){
 text(12,33,12,0x003366,"Mail");button(220,32,91,22,"New postcard",-1);
 if(mailPage*6>=social.nMail)mailPage=social.nMail?(social.nMail-1)/6:0;
 for(unsigned i=0;i<6&&mailPage*6+i<social.nMail;i++){Mail *m=&social.mail[mailPage*6+i];box(9,60+i*21,302,20,0xffffff);char label[80];snprintf(label,sizeof label,"%s%.18s: %.22s",m->read?"":"* ",m->sender,postcard_subject(m->postcard));text(16,62+i*21,11,0x003366,label);}
 if(!social.nMail)center(160,100,12,0x003366,"You have no postcards.");
 button(9,187,75,18,"Previous",-1);button(236,187,75,18,"Next",-1);char count[32];snprintf(count,sizeof count,"%u postcards",social.nMail);center(160,190,10,0x003366,count);
}
static void mail_touch(int x,int y){
 if(y>=32&&y<55&&x>=220){postcardRecipient=0;postcardPage=0;demo.tab=9;return;}
 if(y>=187){if(x<85&&mailPage)mailPage--;if(x>=236&&(mailPage+1)*6<social.nMail)mailPage++;return;}
 if(y<60||y>=186)return;
 unsigned index=mailPage*6+(unsigned)(y-60)/21;if(index>=social.nMail)return;
 Mail *m=&social.mail[index];if(!m->postcard||m->postcard>99999){modal_message("This postcard is not available.");return;}postcardId=0;resource_open(100000+m->postcard,m->sender,m->uid);pnet_send("l#mc",0,NULL);pnet_send("l#mst",0,NULL);for(unsigned i=0;i<social.nMail;i++)social.mail[i].read=true;
}
static void postcards_draw(void){
 static const char *categories[]={"","EPF","Featured Postcards","Friendship","Puffles","Party","Games","Rooms","From us"};
 unsigned pages=postcardRecipient?POSTCARD_PAGES:(social.nFriends+5)/6;if(postcardPage>=pages)postcardPage=pages?pages-1:0;
 PostcardPage page=postcardRecipient?postcardPages[postcardPage]:(PostcardPage){.first=postcardPage*6,.count=social.nFriends-postcardPage*6};
 text(12,33,12,0x003366,postcardRecipient?categories[page.category]:"Choose a recipient");
 for(unsigned i=0;i<6&&i<page.count;i++){unsigned index=page.first+i;const char *label=postcardRecipient?postcardInfo[index].subject:social.friends[index].name;box(9,60+i*21,302,20,0xffffff);text(17,62+i*21,11,0x003366,label);}
 if(!pages)wrapped(15,90,11,0x003366,"Add a buddy or open a penguin's player card to send a postcard.",290,4);
 button(9,187,75,18,"Previous",-1);button(236,187,75,18,"Next",-1);char label[40];snprintf(label,sizeof label,"%u/%u | 10 coins",postcardPage+1,pages?pages:1);center(160,190,10,0x003366,label);
}
static void postcards_touch(int x,int y){
 unsigned pages=postcardRecipient?POSTCARD_PAGES:(social.nFriends+5)/6;
 if(y>=187){if(x<85&&postcardPage)postcardPage--;if(x>=236&&postcardPage+1<pages)postcardPage++;return;}
 if(y<60||y>=186||!pages)return;
 unsigned row=(unsigned)(y-60)/21;if(row>=6)return;
 if(!postcardRecipient){unsigned index=postcardPage*6+row;if(index<social.nFriends){postcardRecipient=social.friends[index].id;postcardPage=0;}}else{PostcardPage page=postcardPages[postcardPage];if(row>=page.count)return;postcardId=postcardInfo[page.first+row].id;resource_open(100000+postcardId,demo.actors[0].name,0);}
}
static void books_draw(void){
 for(unsigned i=0;i<6&&bookListPage*6+i<BOOK_COUNT;i++){
  const BookInfo *b=&bookInfo[bookListPage*6+i];float x=7+i%2*157,y=34+i/2*50;box(x,y,149,49,0xffffff);unsigned tile=b->id-1;
  crop(&bookCovers,tile%8*64,tile/8*64,64,64,x+1,y+1,.72f,0xffffffff);
  wrapped(x+49,y+7,10,0x003366,b->title,96,2);
  char pages[24];snprintf(pages,sizeof pages,"%u %s",b->frames,b->frames==1?"page":"pages");text(x+49,y+34,10,0x003366,pages);
 }
 button_state(7,185,87,21,"Previous",-1,bookListPage>0);button_state(226,185,87,21,"Next",-1,(bookListPage+1)*6<BOOK_COUNT);char page[24];snprintf(page,sizeof page,"%u / %u",bookListPage+1,(BOOK_COUNT+5)/6);center(160,190,10,0x003366,page);
}
static void content(const char *name){
 if(!name||!*name)return;
 if(!strncmp(name,"room:",5)){char key[40];float x,y;if(sscanf(name+5,"%39[^:]:%f:%f",key,&x,&y)==3&&isfinite(x)&&isfinite(y)){const IslandRoom *room=island_key(key);if(room){wantedRoom=room->id;wantedX=x;wantedY=y;}}return;}
 if(!strncmp(name,"igloo:",6)){open_igloo((unsigned)strtoul(name+6,NULL,10));return;}
 if(!strncmp(name,"buy:",4)){buy((unsigned)strtoul(name+4,NULL,10));return;}
 if(!strncmp(name,"prompt:",7)){const char *text=shellText?json_string_value(json_object_get(shellText,name+7)):NULL;modal_message(text?text:"This activity is not available yet.");return;}
 size_t length=strlen(name);if(length>10&&!strcmp(name+length-10,"_catalogue")){char key[48];if(length-10<sizeof key){memcpy(key,name,length-10);key[length-10]=0;book_open(key);}return;}
 if(!strcmp(name,"map")){demo.tab=1;return;}
 if(!strcmp(name,"news")){newspaper_show();return;}
 if(!strncmp(name,"game:",5)){game_start(name+5);return;}
 if(!strncmp(name,"table:",6)||!strncmp(name,"waddle:",7)){bool table=name[0]=='t';const char *value=name+(table?6:7);char *end;unsigned long id=strtoul(value,&end,10);if(*value&&!*end&&id<=1000)lobby_prompt(table?LOBBY_TABLE:LOBBY_WADDLE,(unsigned)id);return;}
 if(!strncmp(name,"unavailable:",12)){modal_message("This activity is not available yet.");return;}
 message(!strncmp(name,"game:",5)?"This game is being ported.":"This window is being ported.");
}
static bool keyboard(char *value,size_t capacity,const char *hint,bool password){SwkbdState keyboard;swkbdInit(&keyboard,SWKBD_TYPE_NORMAL,2,(int)capacity-1);swkbdSetHintText(&keyboard,hint);swkbdSetInitialText(&keyboard,value);swkbdSetValidation(&keyboard,SWKBD_NOTEMPTY_NOTBLANK,0,0);swkbdSetPasswordMode(&keyboard,password?SWKBD_PASSWORD_HIDE:SWKBD_PASSWORD_NONE);gp_audio_pause(true);bool accepted=swkbdInputText(&keyboard,value,capacity)==SWKBD_BUTTON_CONFIRM;gp_audio_pause(false);return accepted;}
static void stop_position(void){Actor *a=&demo.actors[0];if(pnet_state()==PNET_ONLINE){char x[20],y[20];snprintf(x,sizeof x,"%d",(int)lroundf(a->x));snprintf(y,sizeof y,"%d",(int)lroundf(a->y));pnet_send("u#sp",2,(const char *[]){x,y});}actionPosition=true;}
static void action(int frame){stop_position();demo_action(&demo,frame);char arg[16];snprintf(arg,sizeof arg,"%d",demo.actors[0].action);if(pnet_state()==PNET_ONLINE)pnet_send(frame==17?"u#sf":"u#sa",1,(const char *[]){arg});}
static void snowball(void){aiming=!aiming;if(aiming){stop_position();demo_action(&demo,demo.actors[0].direction);message("Touch the minimap to aim your snowball.");}}
static void throw_to(Actor *a,float x,float y){int d=(int)lroundf((atan2f(y-a->y,x-a->x)*180.0f/3.14159265f-90)/45);d=(d%8+8)%8+1;a->walking=false;a->clock=0;a->action=26+(int)lroundf(d/2.0f);overlay_throw(&overlay,a->x,a->y,x,y);}
static bool chat_send(const char *value,void *unused){
 (void)unused;if(pnet_state()!=PNET_ONLINE)return false;char id[20];snprintf(id,sizeof id,"%u",pnet_id());
 if(!pnet_send("m#sm",2,(const char *[]){id,value}))return false;
 actor_say(&demo.actors[0],value);return true;
}
static void chat(void){chatHistory.open=false;chat_keyboard_open(&chatKeyboard,chat_send,NULL);}
static void history_open(void){stop_position();demo.actors[0].walking=false;demo.actors[0].tx=demo.actors[0].x;demo.actors[0].ty=demo.actors[0].y;chat_log_view_open(&chatHistory);}
static float history_width(unsigned size,const char *text){return textWidth(size,text);}
static void history_button(float x,float y,float w,float h,const char *label,bool enabled){button_state(x,y,w,h,label,-1,enabled);}
static unsigned chat_keys(u32 keys){
 const u32 native[]={KEY_DUP,KEY_DDOWN,KEY_DLEFT,KEY_DRIGHT,KEY_A,KEY_B,KEY_X,KEY_Y,KEY_R,KEY_START,KEY_SELECT};
 unsigned out=keys&KEY_L?CHAT_SHIFT:0;for(unsigned i=0;i<sizeof native/sizeof *native;i++)if(keys&native[i])out|=1u<<i;return out;
}
static void toggle_audio(void){
 bool enabled=!settings.audio;gp_audio_set_enabled(enabled);
 if(enabled&&!gp_audio_available()){gp_audio_set_enabled(false);modal_message("Audio is unavailable. Check that DSP support is installed.");return;}
 settings.audio=enabled;settings_save(&settings,SETTINGS_FILE);
}
static void power_apply(void){osSetSpeedupEnable(isNew3DS&&!settings.batterySaver);C3D_FrameRate(isNew3DS&&!settings.batterySaver?60:30);}
static void toggle_battery(void){
 settings.batterySaver=!settings.batterySaver;power_apply();
 if(!settings_save(&settings,SETTINGS_FILE))modal_message("Could not save the battery setting on SD.");
 else modal_message(settings.batterySaver?"Battery saver on: 30 FPS and normal CPU speed. Chat and crossplay remain connected. On an original 3DS, CPU and frame rate already use this setting.":"Battery saver off. New 3DS uses the faster CPU and up to 60 FPS.");
}

static void penguin_open(void){demo.tab=18;demo.actors[0].walking=false;demo.actors[0].tx=demo.actors[0].x;demo.actors[0].ty=demo.actors[0].y;message("Your penguin is saved automatically.");}
static bool account_backup_session(void);
static bool account_storage_ready(void){
 if(settings_save(&settings,SETTINGS_FILE)&&account_backup_session())return true;
 modal_message("Could not save your current penguin. Check the SD card before switching.");return false;
}
static bool account_select_session(const char *session,const char *name){
 bool changed=strcmp(settings.session,session)!=0;
 if((!changed||account_backup_session())&&settings_select_account(&settings,session,name,SETTINGS_FILE)){if(changed){chat_log_clear(&chatLog);memset(&chatHistory,0,sizeof chatHistory);}return true;}
 char error[400];snprintf(error,sizeof error,"Could not save this penguin on SD. Your previous character is still selected. Keep this private recovery code to restore the new character:\n\nTWEEBBO-%s",session);
 modal_message(error);return false;
}
static void penguin_import(void){
 char code[17]="";
 if(!keyboard(code,sizeof code,"PC: Account > Use on another device",false))return;
 unsigned letters=0;for(const char *c=code;*c;c++)if(*c!=' '&&*c!='-')letters++;
 if(letters!=8&&letters!=10){modal_message("On PC, choose Use on another device and enter its short code here.");return;}
 if(!account_storage_ready())return;
 if(!pnet_account_recover(&settings.network,code)){
  modal_message(pnet_account_state()==PNET_ACCOUNT_BUSY?"Wait for the current account request to finish.":"Could not connect. Check your connection and try again.");
  return;
 }
 message("Bringing your penguin from PC...");
}
static char deviceCode[16];
static u64 deviceExpires;
static uint8_t deviceQr[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
static void device_open(void){
 if(!settings.session[0]){modal_message("Connect to GoPenguin first.");return;}
 if(pnet_account_state()==PNET_ACCOUNT_BUSY){modal_message("Wait for the current account request to finish.");return;}
 memset(deviceCode,0,sizeof deviceCode);memset(deviceQr,0,sizeof deviceQr);deviceExpires=0;demo.tab=21;
 if(!pnet_account_device_link(&settings.network,settings.session))modal_message("Could not create a device code. Try again.");
}
static void device_draw(void){
 text(12,33,13,0x003366,"Take your penguin to PC");
 bool valid=deviceCode[0]&&osGetTime()<deviceExpires;
 if(valid){
  text(12,62,16,0x003366,deviceCode);
  wrapped(12,91,9,0x003366,"Scan the QR, or open gopenguin.lol and choose Account > Enter device code.",178,4);
  char remaining[64];unsigned seconds=(unsigned)((deviceExpires-osGetTime())/1000);snprintf(remaining,sizeof remaining,"Expires in %u:%02u",seconds/60,seconds%60);text(12,136,10,0x003366,remaining);
  int n=qrcodegen_getSize(deviceQr),scale=2,side=(n+8)*scale;float left=204+(108-side)/2.0f;
  rect(left,56,side,side,0xffffff);
  for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(qrcodegen_getModule(deviceQr,x,y))rect(left+(x+4)*scale,56+(y+4)*scale,scale,scale,0x000000);
 }else wrapped(12,65,11,0x003366,pnet_account_state()==PNET_ACCOUNT_BUSY?"Creating your private device code...":"This code is no longer available. Tap New code to try again.",294,4);
 text(12,153,10,0x003366,"Your progress follows your penguin.");
 button(8,178,148,24,"New code",-1);button(164,178,148,24,"Back",-1);
 if(valid)text(204,138,8,0x003366,"Keep this code private.");
}
static void device_touch(int x,int y){
 if(y>=178&&y<202){if(x<160)device_open();else{memset(deviceCode,0,sizeof deviceCode);demo.tab=22;}}
}
static bool account_backup_session(void){
 return settings_backup_session(&settings,DATA_DIR "/previous-session.txt");
}
static void penguin_rename(void){
 if(!settings.session[0]){modal_message("Connect to GoPenguin before changing your penguin name.");return;}
 char name[64];snprintf(name,sizeof name,"%s",settings.name[0]?settings.name:demo.actors[0].name);
 if(!keyboard(name,sizeof name,"Penguin name",false))return;
 if(!pnet_account_rename(&settings.network,settings.session,name)){
  modal_message(pnet_account_state()==PNET_ACCOUNT_BUSY?"Wait for the current account request to finish.":"Set the GoPenguin server in Help first.");
  return;
 }
 message("Saving your new name...");
}
static void penguin_draw(void){
 const PNetState state=pnet_state();
 text(12,33,12,0x003366,"Your penguin");
 text(235,35,9,0x003366,"v" GOPENGUIN_VERSION);
 box(7,50,306,38,0xffffff);
 char line[120];snprintf(line,sizeof line,"%s",settings.name[0]?settings.name:demo.actors[0].name[0]?demo.actors[0].name:"Not connected");
 text(15,57,13,0x003366,line);
 const char *status=state==PNET_ONLINE?"Online":state==PNET_OFFLINE?"Not connected":state==PNET_SIGN_IN?"Import from PC to reconnect":"Connecting...";
 if(state==PNET_ONLINE)snprintf(line,sizeof line,"%s - ID %u",status,pnet_id());else snprintf(line,sizeof line,"%s",status);
 text(15,74,10,0x003366,line);
 text(12,96,10,0x003366,"Your penguin is saved automatically.");
 float dy=ui_button(8,118,304,28,ui_touching(8,118,304,28),true)?1:0;
 transfer_symbol(19,119+dy,true);button_label(192,132+dy,11,0xffffff,"Transfer penguin");
 account_button(8,152,148,28,"Change name",2);button(164,152,148,28,"Updates",-1);
 button(8,184,304,20,"Back to Help",-1);
}
static void penguin_touch(int x,int y){
 if(y>=118&&y<146)demo.tab=22;
 else if(y>=152&&y<180){if(x<160)penguin_rename();else{demo.tab=20;if(gp_update_view().state==GP_UPD_IDLE)gp_update_check();}}
 else if(y>=184&&y<204)demo.tab=4;
}
static void transfer_draw(void){
 text(12,34,12,0x003366,"Play with the same penguin");
 text(12,54,10,0x003366,"Your name and progress come with you.");
 transfer_button(77,true,"From PC","Bring your PC penguin here.");
 transfer_button(133,false,"To PC","Continue on PC with this penguin.");
 button(8,188,304,16,"Back to Account",-1);
}
static void transfer_touch(int x,int y){
 if(x<8||x>=312)return;
 if(y>=77&&y<126)penguin_import();
 else if(y>=133&&y<182)device_open();
 else if(y>=188&&y<204)demo.tab=18;
}
static void update_draw(void){
 GPUpdateView view=gp_update_view();char line[96];
 text(12,34,13,0x003366,"GoPenguin updates");snprintf(line,sizeof line,"Installed: %s",GOPENGUIN_VERSION);text(12,58,11,0x003366,line);
 if(view.version[0]){snprintf(line,sizeof line,"Available: %s",view.version);text(12,78,11,0x003366,line);}
 wrapped(12,102,10,0x003366,view.message[0]?view.message:"Check for a new GoPenguin version.",294,4);
 if(view.state==GP_UPD_DOWNLOADING){snprintf(line,sizeof line,"%u / %u MB",view.done/1048576,view.total/1048576);text(12,153,11,0x003366,line);}
 bool busy=view.state==GP_UPD_CHECKING||view.state==GP_UPD_DOWNLOADING;
 button_state(8,174,148,26,busy?"Please wait...":view.state==GP_UPD_AVAILABLE||(view.state==GP_UPD_ERROR&&view.version[0])?"Install / resume":"Check again",-1,!busy&&view.state!=GP_UPD_INSTALLED);
 button(164,174,148,26,"Account",-1);
}
static void update_touch(int x,int y){
 if(y<174||y>=200)return;
 if(x>=160){demo.tab=18;return;}
 GPUpdateView view=gp_update_view();if(view.state==GP_UPD_AVAILABLE||(view.state==GP_UPD_ERROR&&view.version[0]))gp_update_start();else if(view.state!=GP_UPD_CHECKING&&view.state!=GP_UPD_DOWNLOADING&&view.state!=GP_UPD_INSTALLED)gp_update_check();
}
/** B5/B9: the same one-time support story as the web M panel, paid by phone via QR. */
static void membership_open(void){
 demo.tab=19;message("Choose $9.99 or a custom amount on your phone. Benefits start at $9.99 USD.");
}
static void membership_draw(void){
 membership_tick(osGetTime());
 if(membership_take_paid())membershipPaid=true;
 MembershipState state=membership_state();
 char price[32],coins[32];
 int cents=membership_price_cents()>0?membership_price_cents():999;
 unsigned reward=membership_coins()?membership_coins():15000;
 snprintf(price,sizeof price,"$%d.%02d USD",cents/100,cents%100);
 if(reward>=1000)snprintf(coins,sizeof coins,"%u,%03u coins",reward/1000,reward%1000);else snprintf(coins,sizeof coins,"%u coins",reward);
 if(state==MEMBERSHIP_READY&&membership_qr_size()){
  box(202,44,108,108,0xffffff);
  int n=membership_qr_size();float m=100.0f/n,o=(100.0f-m*n)/2;
  for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(membership_qr_dark(x,y))rect(206+o+x*m,48+o+y*m,m,m,0x111920);
  text(12,33,12,0x003366,"Support GoPenguin");
  text(12,52,10,0x003366,"Scan with your phone");
  text(12,68,11,0x003366,price);
  text(12,84,10,0x003366,coins);
  wrapped(12,100,9,0x003366,membership_items()[0]?membership_items():"Founder cosmetic set",180,3);
  wrapped(12,148,9,0x003366,"Custom amount on your phone. Benefits from $9.99. This screen updates after payment.",180,3);
  button(8,176,96,24,"New code",-1);button(164,176,148,24,"Back to Help",-1);
  return;
 }
 text(12,33,12,0x003366,"Support GoPenguin");
 if(state==MEMBERSHIP_PAID){
  wrapped(12,52,10,0x003366,membership_message(),296,5);
  button(8,176,148,24,"Back to Help",-1);
  return;
 }
 if(state==MEMBERSHIP_ERROR)wrapped(12,52,10,0x8b1d1d,membership_message(),296,4);
 else if(state==MEMBERSHIP_LOADING)wrapped(12,52,10,0x003366,"Creating your payment code...",296,3);
 else wrapped(12,52,10,0x003366,membership_member()?"You are already a member for life. You can support again.":"Become a GoPenguin member for life.",296,3);
 text(12,74,11,0x003366,price);text(12,90,10,0x003366,coins);
 wrapped(12,106,9,0x003366,membership_items()[0]?membership_items():"Founder set: Gold Viking Helmet, Golden Fairy Wings, The Rainbow Zephyr, Rainbow Checkered Shoes, Aurora Background",296,3);
 wrapped(12,150,9,0x003366,"Scan the QR. Choose a custom amount with PayPal or crypto. Benefits from $9.99 USD.",296,2);
 button(8,184,148,22,state==MEMBERSHIP_LOADING?"Wait...":"Pay / QR",-1);button(164,184,148,22,"Back to Help",-1);
}
static void membership_touch(int x,int y){
 if(y<176||y>=204)return;
 if(membership_state()==MEMBERSHIP_PAID||x>=160){demo.tab=4;membership_close();return;}
 if(membership_state()==MEMBERSHIP_LOADING)return;
 membership_link_open(&settings.network,settings.session);
}
static bool igloo_public(void){for(unsigned n=0;n<igloo.nOpen;n++)if(igloo.open[n].owner==pnet_id())return true;return false;}
static void igloo_list_open(void){if(pnet_state()!=PNET_ONLINE){message("Connect to GoPenguin to visit public igloos.");return;}demo.tab=10;iglooPage=0;igloo.openReady=false;pnet_send("g#gr",0,NULL);}
static void igloo_list_draw(void){
 if(iglooPage*6>=igloo.nOpen)iglooPage=igloo.nOpen?(igloo.nOpen-1)/6:0;
 button(6,33,97,23,"My igloo",-1);button(110,33,97,23,igloo_public()?"Close mine":"Open mine",-1);button(214,33,100,23,"Refresh",-1);
 for(unsigned n=0;n<6&&iglooPage*6+n<igloo.nOpen;n++){OpenIgloo *entry=&igloo.open[iglooPage*6+n];box(8,61+n*20,304,19,0xffffff);char label[64];snprintf(label,sizeof label,"%.38s",entry->name);text(16,63+n*20,10,0x003366,label);}
 if(!igloo.openReady)center(160,111,11,0x003366,"Loading public igloos...");else if(!igloo.nOpen)center(160,111,11,0x003366,"No public igloos are open.");
 button(8,184,76,20,"Previous",-1);button(236,184,76,20,"Next",-1);char label[40];snprintf(label,sizeof label,"%u / %u",iglooPage+1,igloo.nOpen?(igloo.nOpen+5)/6:1);center(160,188,10,0x003366,label);
}
static void igloo_list_touch(int x,int y){
 if(y>=33&&y<56){if(x<106){open_igloo(pnet_id());return;}if(x<211){if(!igloo.openReady)return;char id[20];snprintf(id,sizeof id,"%u",pnet_id());if(igloo_public())pnet_send("g#cr",1,(const char *[]){id});else pnet_send("g#or",2,(const char *[]){id,demo.actors[0].name});}igloo.openReady=false;pnet_send("g#gr",0,NULL);return;}
 if(y>=184){if(x<85&&iglooPage)iglooPage--;if(x>=236&&(iglooPage+1)*6<igloo.nOpen)iglooPage++;return;}
 if(y>=61&&y<181){unsigned index=iglooPage*6+(unsigned)(y-61)/20;if(index<igloo.nOpen)open_igloo(igloo.open[index].owner);}
}
static unsigned furniture_type(unsigned id){Product p;return client_product(clientData,1,id,&p)?p.type:1;}
static void editor_cancel_drag(void){if(editor.dragging&&editor.selected>=0&&(unsigned)editor.selected<igloo.layout.count){IglooFurniture *f=&igloo.layout.items[editor.selected];f->x=editor.oldX;f->y=editor.oldY;}editor.dragging=false;}
static void editor_open(void){
 if(!roomBank||(pnet_state()!=PNET_ONLINE&&!offline_active())||!igloo_begin_edit(&igloo,local_player())){message("You can edit your own igloo while connected.");return;}
 editor.selected=-1;editor.dragging=editor.inventory=false;editor.page=editor.category=editor.revision=0;editor.search[0]=0;demo.tab=0;demo.actors[0].walking=false;demo.actors[0].tx=demo.actors[0].x;demo.actors[0].ty=demo.actors[0].y;if(offline_active()){char *packet=malloc(OFFLINE_FURNITURE*24+32);if(packet&&offline_furniture_packet(&offlineShop,packet,OFFLINE_FURNITURE*24+32))local_push(packet);free(packet);}else pnet_send("g#gf",0,NULL);
}
static void editor_remove(void){if(igloo.awaitingSave)return;editor_cancel_drag();if(editor.selected>=0&&igloo_remove(&igloo,(unsigned)editor.selected)){for(unsigned n=(unsigned)editor.selected;n<igloo.layout.count;n++)furnitureTime[n]=furnitureTime[n+1];editor.selected=-1;}}
static void editor_turn(int rotation,int variation){
 if(igloo.awaitingSave||editor.selected<0||(unsigned)editor.selected>=igloo.layout.count)return;
 editor_cancel_drag();IglooFurniture *f=&igloo.layout.items[editor.selected];unsigned rotations,variations;
 if(!wear_pose_info(asset_key(ASSET_FURNITURE,f->id),f->rotation,&rotations,&variations)){message("Wait for this furniture to load.");return;}
 unsigned r=(f->rotation-1+rotations+rotation)%rotations+1,v=f->frame;
 if(rotation){wear_pose_info(asset_key(ASSET_FURNITURE,f->id),r,NULL,&variations);if(v>variations)v=variations;}
 else v=(v-1+variations+variation)%variations+1;
 if(igloo_place(&igloo,(unsigned)editor.selected,f->x,f->y,r,v))furnitureTime[editor.selected]=0;
}
static void editor_save(void){
 editor_cancel_drag();if(igloo.awaitingSave)return;if(!igloo.dirty){igloo.editing=false;editor.inventory=false;return;}if(pnet_state()!=PNET_ONLINE&&!offline_active()){message("Reconnect before saving your igloo.");return;}
 char entries[IGLOO_ITEMS][64],owner[20];const char *args[IGLOO_ITEMS];for(unsigned n=0;n<igloo.layout.count;n++){IglooFurniture *f=&igloo.layout.items[n];snprintf(entries[n],sizeof entries[n],"%u|%d|%d|%u|%u",f->id,f->x,f->y,f->rotation,f->frame);args[n]=entries[n];}
 if(!igloo_save_prepare(&igloo))return;
 if(offline_active()){
  // Store the layout locally, then confirm it with the server-format reply like an online save.
  static char layout[OFFLINE_LAYOUT];size_t used=0;layout[0]=0;
  for(unsigned n=0;n<igloo.layout.count;n++){int w=snprintf(layout+used,sizeof layout-used,"%s%s",n?",":"",entries[n]);if(w<0||(size_t)w>=sizeof layout-used){igloo.awaitingSave=false;message("This igloo is too large to save.");return;}used+=(size_t)w;}
  if(!offline_set_layout(&offlineShop,layout)||!offline_save(&offlineShop,OFFLINE_SHOP_FILE)){igloo.awaitingSave=false;message("The offline igloo could not be saved on SD.");return;}
  offline_igloo_apply();editor.saveAt=osGetTime();return;
 }
 snprintf(owner,sizeof owner,"%u",igloo.layout.owner);
 if(!pnet_send("g#ur",igloo.layout.count,args)||!pnet_send_to('r',"gm",1,(const char *[]){owner})){igloo.awaitingSave=false;message("Could not send your igloo. Try Save again.");return;}
 editor.saveAt=osGetTime();message("Saving igloo...");
}
static int furniture_compare(const void *a,const void *b){unsigned x=*(const unsigned *)a,y=*(const unsigned *)b;Product p,q;const char *xn=client_product(clientData,1,x,&p)?p.name:"",*yn=client_product(clientData,1,y,&q)?q.name:"";int n=strcasecmp(xn,yn);return n?n:(x>y)-(x<y);}
static bool furniture_matches(const char *name){if(!*editor.search)return true;size_t length=strlen(editor.search);for(;*name;name++)if(!strncasecmp(name,editor.search,length))return true;return false;}
static void editor_inventory(void){
 if(editor.revision==igloo.revision+1)return;
 editor.count=0;for(unsigned n=0;n<igloo.nOwned;n++){unsigned id=igloo.owned[n].id;Product p;bool known=client_product(clientData,1,id,&p);if(editor.category&&(!known||p.type!=editor.category))continue;if(!furniture_matches(known?p.name:""))continue;editor.items[editor.count++]=id;}
 qsort(editor.items,editor.count,sizeof *editor.items,furniture_compare);if(editor.page*6>=editor.count)editor.page=editor.count?(editor.count-1)/6:0;editor.revision=igloo.revision+1;
}
static void editor_draw(void){
 if(editor.inventory){
  editor_inventory();const char *categories[]={"All furniture","Room furniture","Wall furniture","Floor furniture","Puffle furniture"};button(4,29,108,22,categories[editor.category],-1);button(117,29,108,22,"Search",-1);button(230,29,86,22,"Clear",-1);
  for(unsigned n=0;n<6&&editor.page*6+n<editor.count;n++){unsigned id=editor.items[editor.page*6+n],amount=igloo_available(&igloo,id);float x=4+n%3*105,y=55+n/3*65;box(x,y,102,62,amount?0xffffff:0xc9dce8);wear_fit(asset_key(ASSET_FURNITURE_ICON,id),x+25,y+3,52,39);Product p;char label[32];snprintf(label,sizeof label,"%.16s",client_product(clientData,1,id,&p)?p.name:"Furniture");center(x+51,y+44,10,0x003366,label);snprintf(label,sizeof label,"%u",amount);text(x+6,y+5,10,0x003366,label);}
  if(!igloo.ownedReady)center(160,108,11,0x003366,"Loading your furniture...");else if(!editor.count)center(160,108,11,0x003366,*editor.search?"No furniture matches your search.":"No furniture in this category.");
  char label[96];snprintf(label,sizeof label,"%u / %u   %s",editor.page+1,editor.count?(editor.count+5)/6:1,editor.search);center(160,190,10,0x003366,label);button(2,207,76,31,"Previous",-1);button(82,207,76,31,"Back",-1);button(162,207,76,31,"Next",-1);button(242,207,76,31,"Save",-1);return;
 }
 ActorOrder order;make_order(&order,false,.35f,27,28);scene_draw_to(sceneBottom,320,240,.35f,.35f,27,28,&order);
 if(editor.selected>=0&&(unsigned)editor.selected<igloo.layout.count){IglooFurniture *f=&igloo.layout.items[editor.selected];float x=27+f->x*.35f,y=28+f->y*.35f;C2D_DrawCircleSolid(x,y,.5f,5,rgb(0x003366));C2D_DrawCircleSolid(x,y,.5f,3,rgb(0x00ccff));Product p;char label[80];snprintf(label,sizeof label,"%.25s  %u / %u",client_product(clientData,1,f->id,&p)?p.name:"Furniture",f->rotation,f->frame);box(20,181,280,22,0xffffff);center(160,185,10,0x003366,label);}
 else {box(15,181,290,22,0xffffff);center(160,185,10,0x003366,"Drag furniture. X: return item. B: save.");}
 button(2,207,60,31,"Furniture",-1);button(66,207,60,31,"Rotate <",-1);button(130,207,60,31,"Rotate >",-1);button(194,207,60,31,"Variation",-1);button(258,207,60,31,igloo.awaitingSave?"Saving...":"Save",-1);
}
static void editor_touch(int x,int y){
 if(igloo.awaitingSave)return;
 if(editor.inventory){editor_inventory();if(y>=207){if(x<80){if(editor.page)editor.page--;}else if(x<160)editor.inventory=false;else if(x<240){if((editor.page+1)*6<editor.count)editor.page++;}else editor_save();return;}
  if(y>=29&&y<51){if(x<113){editor.category=(editor.category+1)%5;editor.page=editor.revision=0;}else if(x<226){if(keyboard(editor.search,sizeof editor.search,"Search your furniture",false))editor.page=editor.revision=0;}else{editor.search[0]=0;editor.page=editor.revision=0;}return;}
  if(x>=4&&x<316&&y>=55&&y<182){unsigned index=editor.page*6+(unsigned)(y-55)/65*3+(unsigned)(x-4)/105;if(index>=editor.count)return;unsigned id=editor.items[index];if(!igloo_available(&igloo,id)){message("All copies of this furniture are in your igloo.");return;}if(igloo.layout.count==IGLOO_ITEMS){message("Your igloo can hold up to 99 furniture items.");return;}unsigned type=furniture_type(id);for(int wy=40;wy<460;wy+=10)for(int wx=40;wx<740;wx+=10)if(igloo_geometry_legal(iglooGeometry,type,wx,wy)){if(igloo_add(&igloo,id,wx,wy)){editor.selected=(int)igloo.layout.count-1;furnitureTime[editor.selected]=0;editor.inventory=false;}return;}message("There is no suitable area for this furniture.");}return;
 }
 if(y>=207){editor_cancel_drag();if(x<64){editor.inventory=true;editor.page=0;}else if(x<128)editor_turn(-1,0);else if(x<192)editor_turn(1,0);else if(x<256)editor_turn(0,1);else editor_save();return;}
 if(x<27||x>=293||y<28||y>=181)return;
 float wx=(x-27)/.35f,wy=(y-28)/.35f;ActorOrder order;make_order(&order,false,1,0,0);editor.selected=-1;
 for(unsigned n=order.count;n>0;n--){unsigned index=order.items[n-1].tag-CAST_COUNT;IglooFurniture *f=&igloo.layout.items[index];if(wear_pose_contains(asset_key(ASSET_FURNITURE,f->id),f->rotation,f->frame,(unsigned)(furnitureTime[index]*24),f->x,f->y,1,wx,wy)){editor.selected=(int)index;editor.oldX=f->x;editor.oldY=f->y;editor.pressX=wx;editor.pressY=wy;editor.moved=false;editor.dragging=true;break;}}
}
static void editor_drag(u32 held,u32 up){
 if(!editor.dragging||igloo.awaitingSave||editor.selected<0||(unsigned)editor.selected>=igloo.layout.count)return;
 IglooFurniture *f=&igloo.layout.items[editor.selected];if(held&KEY_TOUCH){touchPosition p;hidTouchRead(&p);float dx=(p.px-27)/.35f-editor.pressX,dy=(p.py-28)/.35f-editor.pressY;if(hypotf(dx,dy)>8)editor.moved=true;if(editor.moved){f->x=editor.oldX+(int)lroundf(dx);f->y=editor.oldY+(int)lroundf(dy);}}
 if(up&KEY_TOUCH){int x=f->x,y=f->y;bool moved=editor.moved;editor_cancel_drag();if(!moved)return;if(x<=20||x>=740||y<=20||y>=460||igloo_geometry_trash(iglooGeometry,x,y))editor_remove();else if(igloo_geometry_legal(iglooGeometry,furniture_type(f->id),x,y))igloo_place(&igloo,(unsigned)editor.selected,x,y,f->rotation,f->frame);else message("Place this furniture inside its allowed area.");}
}
static void header(const char *title){ui_header();unsigned size=15;while(size>10&&textWidth(size,title)>282)size--;FontBounds b=font_ink(size,title);text(31-b.left,13.5f-(b.top+b.bottom)/2.0f,size,0xffffff,title);}
static void styles_open(void){
 if(!roomBank||pnet_state()!=PNET_ONLINE||!igloo_styles_request(&igloo,pnet_id())){modal_message("Enter your igloo and save your furniture before changing its style.");return;}
 char owner[20];snprintf(owner,sizeof owner,"%u",pnet_id());
 igloo.editing=editor.inventory=false;styles.page=0;demo.tab=11;
 pnet_send_to('r',"gm",1,(const char *[]){owner});pnet_send("g#go",0,NULL);pnet_send("g#gail",1,(const char *[]){owner});
}
static void styles_draw(void){
 text(12,33,12,0x003366,"Stored igloos");
 if(!igloo.typesReady||!igloo.slotsReady){center(160,100,11,0x003366,"Loading your igloos...");return;}
 for(unsigned n=0;n<6&&styles.page*6+n<igloo.nTypes;n++){
  unsigned type=igloo.types[styles.page*6+n];Product p;char label[90];snprintf(label,sizeof label,"%.34s%s",client_product(clientData,2,type,&p)?p.name:"Igloo",type==igloo.layout.type?" (active)":"");
  button(9,60+n*21,302,20,label,-1);
 }
 if(!igloo.nTypes)center(160,105,11,0x003366,"No stored igloos.");
 if(styles.preparing||igloo.awaitingStyle)center(160,188,11,0x003366,styles.preparing?"Checking igloo artwork...":"Verifying your igloo...");
 else{button(9,187,75,18,"Previous",-1);button(122,187,76,18,"Refresh",-1);button(236,187,75,18,"Next",-1);}
}
static void styles_touch(int x,int y){
 if(styles.preparing||igloo.awaitingStyle)return;
 if(y>=187){if(x<85&&styles.page)styles.page--;else if(x>=236&&(styles.page+1)*6<igloo.nTypes)styles.page++;else if(x>=122&&x<198)styles_open();return;}
 if(y<60||y>=186||!igloo.typesReady||!igloo.slotsReady)return;
 unsigned n=styles.page*6+(unsigned)(y-60)/21;if(n>=igloo.nTypes)return;
 unsigned type=igloo.types[n];if(type==igloo.layout.type){modal_message("This igloo is already active.");return;}
 if(!igloo.activeKnown){modal_message("The active igloo layout could not be identified. Try Refresh.");return;}
 confirm_command("Changing your igloo returns all furniture to storage and removes the flooring. Continue?","igloo-style",type);
}
static void styles_update(void){
 if(!styles.preparing)return;
 if(pnet_state()!=PNET_ONLINE||osGetTime()-styles.started>60000){styles.preparing=false;modal_message("The igloo could not be prepared. Reconnect or try again.");return;}
 char path[256];unsigned key=asset_key(ASSET_MOVIE,200000+styles.type);
 if(styles.type==1)snprintf(path,sizeof path,"romfs:/data/basic-igloo.gpk");
 else if(!asset_cache_request(key,path,sizeof path)){if(asset_cache_status(key)==ASSET_UNAVAILABLE){styles.preparing=false;modal_message("This igloo's artwork is unavailable. Your current igloo was kept.");}return;}
 release_room_textures();const char *error=NULL;MovieBank *bank=movie_bank_load(path,6u*1024u*1024u,&error);IglooGeometry *geometry=bank?igloo_geometry_load(movie_bank_meta(bank),&error):NULL;
 bool valid=bank&&geometry;igloo_geometry_free(geometry);movie_bank_free(bank);sceneOK=room_textures(currentRoom);styles.preparing=false;
 if(!sceneOK){modal_message("Could not restore the current igloo artwork.");return;}
 if(!valid){modal_message(error?error:"This igloo's artwork is invalid. Your current igloo was kept.");return;}
 if(!igloo_style_prepare(&igloo,pnet_id(),styles.type)){modal_message("The igloo state changed. Refresh the stored igloo list.");return;}
 unsigned location=0;for(unsigned n=0;n<igloo.nSlots;n++)if(igloo.slots[n].id==igloo.activeSlot)location=igloo.slots[n].location;
 char slot[20],type[20],place[20],music[20],owner[20];snprintf(slot,sizeof slot,"%u",igloo.activeSlot);snprintf(type,sizeof type,"%u",styles.type);snprintf(place,sizeof place,"%u",location);snprintf(music,sizeof music,"%u",igloo.targetMusic);snprintf(owner,sizeof owner,"%u",pnet_id());
 if(!pnet_send("g#uic",6,(const char *[]){slot,type,"0",place,music,""})||!pnet_send_to('r',"gm",1,(const char *[]){owner})){igloo.awaitingStyle=false;modal_message("The igloo change could not be sent. Refresh before retrying.");return;}
 styles.started=osGetTime();
}
static bool pets_send(void *unused,const char *command,unsigned count,const char *const *args){(void)unused;return pnet_state()==PNET_ONLINE&&pnet_send(command,count,args);}
static void pets_enter(unsigned owner){if(petRoom==currentRoomId&&puffles.mine==pnet_id())return;puffles_room(&puffles,pnet_id(),owner);petRoom=currentRoomId;wantedPetOwner=owner?owner:pnet_id();}
static void pets_adopt(unsigned type){
 if(type>=PUFFLES_COLOURS){modal_message("This puffle colour is unavailable.");return;}
 if(pnet_state()!=PNET_ONLINE){modal_message("Connect to GoPenguin to adopt a puffle.");return;}
 if(profile.coins<800){modal_message("A puffle costs 800 coins. You need more coins.");return;}
 confirm_command("Adopt this puffle for 800 coins? You will choose its name next.","puffle-adopt",type);
}
static void pets_draw(void){
 if(demo.tab==14){text(12,33,12,0x003366,"Adopt a puffle: 800 coins");for(unsigned i=0;i<PUFFLES_COLOURS;i++)button(8+i%3*104,59+i/3*45,96,40,puffles_colour(i),-1);return;}
 if(demo.tab==13){
  Puffle *p=puffles_find(&puffles,pnet_id(),petSelected);if(!p){center(160,100,11,0x003366,"This puffle is unavailable. Refresh the list.");return;}
  char label[90];text(12,33,13,0x003366,p->name);snprintf(label,sizeof label,"Health: %u   Food: %u   Rest: %u",p->health,p->food,p->rest);text(12,57,11,0x003366,label);
  wear_draw_pose(asset_key(ASSET_PUFFLE,p->type+1),p->frame,1,p->clock,160,92,1.4f);
  const char *labels[]={p->walking?"Stop walking":"Walk puffle","Play","Rest","Food: 10 coins","Cookie: 5 coins","Gum: 5 coins","Bath: 5 coins","Back"};
  for(unsigned i=0;i<8;i++)button(8+i%2*156,108+i/2*23,148,21,labels[i],-1);
  if(puffles.action!=PUFFLE_IDLE)center(160,202,10,0x003366,"Waiting for GoPenguin...");
  return;
 }
 text(12,33,12,0x003366,"Your puffles");unsigned total=0;
 for(unsigned i=0;i<puffles.count;i++)if(puffles.pets[i].owner==pnet_id()){
  unsigned n=total++;if(n<petPage*6||n>=petPage*6+6)continue;
  Puffle *p=&puffles.pets[i];char label[90];snprintf(label,sizeof label,"%.30s (%s)%s",p->name,puffles_colour(p->type),p->walking?" - walking":"");button(9,60+(n-petPage*6)*21,302,20,label,-1);
 }
 if(!total)center(160,105,11,0x003366,puffles.snapshotPending?"Loading your puffles...":"You have no puffles.");
 button(8,187,72,18,"Previous",-1);button(84,187,72,18,"Refresh",-1);button(160,187,72,18,"Adopt",-1);button(236,187,76,18,"Next",-1);
}
static void pets_touch(int x,int y){
 if(pnet_state()!=PNET_ONLINE){modal_message("Connect to GoPenguin to manage puffles.");return;}
 if(demo.tab==14){if(y>=59&&y<194&&x>=8&&x<312){unsigned type=(unsigned)(y-59)/45*3+(unsigned)(x-8)/104;if(type<PUFFLES_COLOURS)pets_adopt(type);}return;}
 if(demo.tab==13){
  if(puffles.action!=PUFFLE_IDLE||y<108||y>=200)return;
  unsigned button=(unsigned)(y-108)/23*2+(x>=164);Puffle *p=puffles_find(&puffles,pnet_id(),petSelected);if(!p)return;
  if(button==7){demo.tab=12;return;}
  if(button==0)puffles_walk(&puffles,p->id,!p->walking);
  else{const unsigned items[]={0,27,37,3,4,5,8};if(button<7)puffles_care(&puffles,p->id,items[button]);}return;
 }
 if(y>=187){if(x<80&&petPage)petPage--;else if(x>=84&&x<156)puffles_request(&puffles,pnet_id());else if(x>=160&&x<232)demo.tab=14;else if(x>=236){unsigned count=0;for(unsigned i=0;i<puffles.count;i++)if(puffles.pets[i].owner==pnet_id())count++;if((petPage+1)*6<count)petPage++;}return;}
 if(y<60||y>=186)return;
 unsigned wanted=petPage*6+(unsigned)(y-60)/21,index=0;
 for(unsigned i=0;i<puffles.count;i++)if(puffles.pets[i].owner==pnet_id()&&index++==wanted){petSelected=puffles.pets[i].id;demo.tab=13;return;}
}
static void features_packet(const PNetMessage *packet,unsigned newRoom){
 if(pnet_id()&&puffles.mine!=pnet_id()){puffles_init(&puffles,pets_send,NULL);petRoom=0;}
 if(newRoom&&newRoom!=petRoom){unsigned owner=newRoom>2000?newRoom-2000:0;puffles_room(&puffles,pnet_id(),owner);petRoom=newRoom;wantedPetOwner=owner?owner:pnet_id();}
 if(igloo_floor_purchase(&igloo,packet,pnet_id())&&roomBank){floorPresent=false;swl_invalidate(scene);}
 if(!strcmp(packet->command,"au"))igloo.typesReady=false;
 if(igloo.styleVerified||igloo.styleFailed){bool verified=igloo.styleVerified;igloo.styleVerified=igloo.styleFailed=false;wantedIgloo=true;styles.preparing=false;message(verified?"Your igloo style has been changed.":"The igloo change could not be verified. Reloading the server layout.");}
 if(!strcmp(packet->command,"gm")&&roomBank&&igloo.layout.type!=renderedIglooType&&!waitingIgloo)wantedIgloo=true;
 if(igloo.musicVerified||igloo.musicFailed){bool verified=igloo.musicVerified;igloo.musicVerified=igloo.musicFailed=false;gp_audio_music(igloo.layout.music);message(verified?"Your igloo music has been saved.":"The igloo music change could not be verified.");}
 bool changed=puffles_apply(&puffles,packet,&demo);
 if(!strcmp(packet->command,"rp")&&packet->count)puffles_departed(&puffles,(unsigned)strtoul(packet->args[0],NULL,10));
 unsigned coins;if(puffles_take_coins(&puffles,&coins)){profile.coins=coins;profile.revision++;}
 if(changed&&!strcmp(packet->command,"pn")){book.close=book.open;demo.tab=12;petPage=0;message("Your puffle has been adopted.");}
}
static aptHookCookie audioHook;
static void audio_focus(APT_HookType type,void *unused){
 (void)unused;
 switch(type){
  case APTHOOK_ONEXIT:pnet_request_stop();asset_cache_request_stop();gp_update_request_stop();break;
  case APTHOOK_ONSUSPEND:case APTHOOK_ONSLEEP:gameSuspended=true;room_tube_cancel(&roomTube,&tubeHooks);gp_audio_pause(true);break;
  case APTHOOK_ONRESTORE:case APTHOOK_ONWAKEUP:gameSuspended=false;gp_audio_pause(false);break;
  default:break;
 }
}
static void music_open(void){
 if(!roomBank||pnet_state()!=PNET_ONLINE||igloo.layout.owner!=pnet_id()||igloo.dirty||igloo.awaitingSave||igloo.awaitingStyle||styles.preparing){modal_message("Enter your igloo and save your changes before choosing music.");return;}
 igloo.editing=false;musicPage=0;demo.tab=15;
}
static void music_draw(void){
 unsigned count=gp_audio_igloo_tracks()+1;text(12,33,12,0x003366,"Igloo music");
 for(unsigned n=0;n<6&&musicPage*6+n<count;n++){
  unsigned index=musicPage*6+n,id=0;const char *name="No music";if(index&&!gp_audio_igloo_track(index-1,&id,&name))continue;
  char label[90];snprintf(label,sizeof label,"%.34s%s",name,id==igloo.layout.music?" (active)":"");button(9,60+n*21,302,20,label,-1);
 }
 if(igloo.awaitingMusic)center(160,188,11,0x003366,"Verifying your music...");else{button(9,187,75,18,"Previous",-1);button(236,187,75,18,"Next",-1);}
}
static void music_touch(int x,int y){
 if(igloo.awaitingMusic)return;
 unsigned count=gp_audio_igloo_tracks()+1;
 if(y>=187){if(x<85&&musicPage)musicPage--;else if(x>=236&&(musicPage+1)*6<count)musicPage++;return;}
 if(y<60||y>=186)return;
 unsigned index=musicPage*6+(unsigned)(y-60)/21,id=0;if(index>=count)return;
 const char *name;if(index&&!gp_audio_igloo_track(index-1,&id,&name))return;
 if(!igloo_music_prepare(&igloo,pnet_id(),id)){modal_message("Save your igloo changes before choosing music.");return;}
 char track[20],owner[20];snprintf(track,sizeof track,"%u",id);snprintf(owner,sizeof owner,"%u",pnet_id());
 if(!pnet_send("g#um",1,(const char *[]){track})||!pnet_send_to('r',"gm",1,(const char *[]){owner})){igloo.awaitingMusic=false;modal_message("Could not send the music change. Please try again.");return;}musicStarted=osGetTime();
}
static void game_notice(void *unused,const char *value){(void)unused;message(value);}
static void game_coins(void *unused,unsigned value){(void)unused;profile.coins=value;profile.revision++;}
static bool game_send(void *unused,char extension,const char *command,unsigned count,const char *const *args){
 (void)unused;bool sent=pnet_state()==PNET_ONLINE&&pnet_send_to(extension,command,count,args);
#ifdef GP_LAN_ONLY
 if(sent){FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"game send=%s first=%lu\n",command,count?strtoul(args[0],NULL,10):0);fclose(trace);}}
#endif
 return sent;
}
static void game_leave(void *unused,unsigned room,int x,int y){(void)unused;if(gameLeaving)return;gameReturnRoom=room;gameReturnX=x;gameReturnY=y;gameLeaving=true;}
static void game_sound(void *unused,const char *bank,const char *name,unsigned volume,bool loop){(void)unused;gp_audio_named_sound(bank,name,(int)volume,loop);}
static void game_stop_sounds(void *unused,const char *bank){(void)unused;(void)bank;gp_audio_scene("");}
static void game_text(void *unused,float x,float y,float size,uint32_t colour,GameAlign align,const char *value){(void)unused;unsigned pixels=(unsigned)fmaxf(FONT_MIN,fminf(FONT_MAX,size));if(align==GAME_ALIGN_CENTER)x-=textWidth(pixels,value)/2.0f;else if(align==GAME_ALIGN_RIGHT)x-=textWidth(pixels,value);text(x,y,pixels,colour,value);}
static void game_rect(void *unused,float x,float y,float w,float h,uint32_t colour){(void)unused;ui_fill(x,y,w,h,colour);}
static void game_button(void *unused,float x,float y,float w,float h,const char *label,bool pressed,bool enabled){(void)unused;float dy=ui_button(x,y,w,h,pressed,enabled)?1:0;button_label(x+w/2,y+h/2+dy,11,enabled?0xffffff:0x003366,label);}
static GameResource game_resource(void *unused,const char *name,char *path,size_t capacity){
 (void)unused;unsigned id=0;
#ifdef GP_NINJA_GAMES
#ifdef GP_DEBUG_GAMES
 if(!strcmp(name,"games/beans.gpk"))name="games/debug-beans.gpk";
#endif
 if(!strcmp(name,"games/debug-beans.gpk")||!strcmp(name,GP_CARD_BANK)||!strcmp(name,GP_CARD_ACTORS_BANK)||!strcmp(name,GP_FIRE_BANK)||card_preview_resource_name(name)){
  if(snprintf(path,capacity,"romfs:/%s",name)>=(int)capacity)return GAME_RESOURCE_FAILED;
  FILE *file=fopen(path,"rb");if(!file)return GAME_RESOURCE_FAILED;fclose(file);return GAME_RESOURCE_READY;
 }
#endif
 for(unsigned n=0;n<COUNT(gameKeys);n++){
  char bank[80];snprintf(bank,sizeof bank,"games/%s.gpk",gameKeys[n]);if(!strcmp(name,bank))id=700001+n;
  snprintf(bank,sizeof bank,"games/%s-title.gpk",gameKeys[n]);if(!strcmp(name,bank))id=701001+n;
  snprintf(bank,sizeof bank,"games/%s-under.gpk",gameKeys[n]);if(!strcmp(name,bank))id=702001+n;
 }
 if(!strcmp(name,"games/fish-worms.gpk"))id=702006;
 if(!id)return GAME_RESOURCE_FAILED;
 if(snprintf(path,capacity,"romfs:/%s",name)>=(int)capacity)return GAME_RESOURCE_FAILED;
 FILE *file=fopen(path,"rb");if(file){fclose(file);return GAME_RESOURCE_READY;}
 unsigned key=asset_key(ASSET_MOVIE,id);if(asset_cache_request(key,path,capacity))return GAME_RESOURCE_READY;
 unsigned status=asset_cache_status(key);return status==ASSET_DISABLED||status==ASSET_UNAVAILABLE?GAME_RESOURCE_FAILED:GAME_RESOURCE_PENDING;
}
static GameInput game_input(u32 down,u32 held,const circlePosition *pad){
 static const u32 hardware[]={KEY_A,KEY_B,KEY_X,KEY_Y,KEY_L,KEY_R,KEY_START,KEY_SELECT,KEY_DLEFT,KEY_DRIGHT,KEY_DUP,KEY_DDOWN};
 GameInput input={.stickX=pad->dx/150.0f,.stickY=pad->dy/150.0f};
 for(unsigned i=0;i<COUNT(hardware);i++){if(down&hardware[i])input.down|=1u<<i;if(held&hardware[i])input.held|=1u<<i;}return input;
}
static const char *lobby_title(const char *key){for(unsigned i=0;i<COUNT(gameKeys);i++)if(key&&!strcmp(key,gameKeys[i]))return gameNames[i];return "Multiplayer";}
static bool lobby_locked(void){return lobbyUI.open||lobbyUI.moving||lobbyUI.leavePending||lobby.state==LOBBY_JOINING||lobby.state==LOBBY_WAITING||lobby.state==LOBBY_READY;}
static void lobby_stop(void){Actor *me=&demo.actors[0];me->walking=false;me->tx=me->x;me->ty=me->y;if(me->action>=9&&me->action<=16)me->action=me->direction;lobbyUI.moving=false;}
static bool lobby_before_room(void){
 if(lobbyUI.cancelledWaddle){message("Wait for the previous race queue to clear. Refresh its panel to check it.");return false;}
 if(lobbyUI.leavePending){message("Retry leaving the table before changing rooms.");return false;}
 if(lobby.needsLeave&&!lobby_cancel(&lobby)){message(lobby.error?lobby.error:"Could not leave this activity.");return false;}
 if(lobby.state==LOBBY_READY&&lobby.kind==LOBBY_WADDLE){message("The race is starting.");return false;}
 return true;
}
static void lobby_room_enter(unsigned room){
 bool chooser=lobbyUI.routeRoom==room;lobbyUI.routeRoom=lobbyUI.cancelledWaddle=0;lobby_stop();lobbyUI.point=NULL;lobbyUI.returning=lobbyUI.raceReturn=false;
 if(!lobby_enter(&lobby,room,demo.actors[0].name))return;
 lobbyUI.open=chooser;lobbyUI.artDirty=true;lobbyUI.refresh=lobby.entryCount!=0&&pnet_state()==PNET_ONLINE;
 if(chooser)demo.tab=17;else if(demo.tab==17)demo.tab=0;
}
static void lobby_art(void){
 if(!lobbyUI.artDirty||!scene||lobby.room!=currentRoomId||game)return;
 lobbyUI.artDirty=false;
 for(unsigned i=0;i<lobby.entryCount;i++){
  const LobbyEntry *entry=&lobby.entries[i];
  if(lobby.kind==LOBBY_TABLE){unsigned frame=lobby_geometry_table_frame(lobby.room,entry->id,entry->count);const char *path=lobby_geometry_population_path(lobby.room,LOBBY_TABLE,entry->id,0);if(frame&&path&&entry->known&&swl_frame(scene,path)!=frame)swl_target(scene,path,frame,NULL,false);}
  else for(unsigned seat=0;seat<entry->capacity;seat++){
   const char *path=lobby_geometry_population_path(lobby.room,LOBBY_WADDLE,entry->id,(int)seat);Actor *player=NULL;
   if(entry->names[seat][0])for(unsigned n=0;n<demo.count;n++)if(!strcmp(demo.actors[n].name,entry->names[seat])){player=&demo.actors[n];break;}
   bool ownSled=player&&(player->look[4]==5021||player->look[4]==5046||player->look[4]==5047);
   if(path)swl_visible(scene,path,player&&!ownSled);
  }
 }
}
static void lobby_walk(const LobbyGeometrySeat *point,bool returning){
 if(!point)return;
 lobbyUI.point=point;lobbyUI.returning=returning;lobbyUI.walkTime=0;lobbyUI.moving=true;
 int rawX=returning?point->doneX:point->x,rawY=returning?point->doneY:point->y;
 Actor *me=&demo.actors[0];me->tx=fmaxf(20,fminf(740,rawX));me->ty=fmaxf(20,fminf(430,rawY));
 float dx=me->tx-me->x,dy=me->ty-me->y;int direction=(int)lroundf((atan2f(dy,dx)*180.0f/3.14159265f-90)/45);
 me->direction=(direction%8+8)%8+1;me->walking=hypotf(dx,dy)>.5f;me->action=me->walking?me->direction+8:me->direction;me->clock=0;
 /* Player.moveTo is the web's blocker-free scripted seat/done tween; only these validated points use it. */
 if(pnet_state()==PNET_ONLINE){char x[16],y[16];snprintf(x,sizeof x,"%d",rawX);snprintf(y,sizeof y,"%d",rawY);pnet_send("u#sp",2,(const char *[]){x,y});}
}
static void game_snapshot(const char *key){
 Actor *me=&demo.actors[0];queuedGame=(GameDescriptor){.returnRoom=currentRoomId,.returnX=(int)me->x,.returnY=(int)me->y,.player=pnet_id(),.colour=(unsigned)me->colour,.colourRGB=item_colour((unsigned)me->colour),.hand=me->look[4],.online=pnet_state()==PNET_ONLINE};snprintf(queuedGame.key,sizeof queuedGame.key,"%s",key);gameQueued=true;gameMultiplayerLost=false;
 snprintf(queuedGame.name,sizeof queuedGame.name,"%.*s",(int)sizeof queuedGame.name-1,me->name);
}
static void lobby_launch(void){
 if(game||gameQueued||lobby.state!=LOBBY_READY||(lobby.kind==LOBBY_TABLE&&lobbyUI.moving))return;
 const char *key=lobby_key(lobby.room,lobby.kind,lobby.selected);if(!key||!game_port_supported(key))return;
 DescriptorLobbyLaunch launch;if(!lobby_take(&lobby,&launch))return;
 const LobbyGeometrySeat *point=lobby_geometry_lookup(currentRoomId,launch.kind,launch.contextId,launch.seat);
 lobby_stop();lobbyUI.open=false;game_snapshot(launch.key);queuedGame.seat=launch.seat;
 snprintf(queuedGame.name,sizeof queuedGame.name,"%.*s",(int)sizeof queuedGame.name-1,launch.username);
 if(launch.kind==LOBBY_TABLE)queuedGame.table=launch.contextId;else queuedGame.waddle=launch.contextId;
 if(point){queuedGame.returnX=point->doneX;queuedGame.returnY=point->doneY;}
#ifdef GP_LAN_ONLY
 FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"lobby launch table=%u waddle=%u seat=%d players=%u\n",queuedGame.table,queuedGame.waddle,queuedGame.seat,demo.count);fclose(trace);}
#endif
}
static void lobby_prompt(LobbyKind kind,unsigned id){
 const char *key=lobby_key(currentRoomId,kind,id);
 if(!key||!lobby_geometry_entry(currentRoomId,kind,id)){modal_message("This activity is not available in this room.");return;}
 if(!game_port_supported(key)){modal_message("This game is not available yet.");return;}
 if(pnet_state()!=PNET_ONLINE){modal_message("Connect to GoPenguin to play with other penguins.");return;}
 if(game||gameQueued||lobby.needsLeave||lobbyUI.moving||lobbyUI.leavePending)return;
 if(lobbyUI.cancelledWaddle){message("Waiting for the previous queue to be cleared. Refresh to check it.");return;}
 if(book.open||igloo.dirty||igloo.awaitingSave||igloo.awaitingStyle||igloo.awaitingMusic||styles.preparing){modal_message("Close the window and finish saving your igloo before playing.");return;}
 room_tube_cancel(&roomTube,&tubeHooks);lobby_stop();char prompt[160];snprintf(prompt,sizeof prompt,"Would you like to join this\n%s %s?",lobby_title(key),kind==LOBBY_TABLE?"game":"race");confirm_command(prompt,kind==LOBBY_TABLE?"lobby-table":"lobby-waddle",id);
}
static void lobby_join(LobbyKind kind,unsigned id){
 const char *key=lobby_key(currentRoomId,kind,id);if(!key||!game_port_supported(key)||game||gameQueued||lobbyUI.leavePending||lobbyUI.cancelledWaddle)return;
 if(pnet_state()!=PNET_ONLINE){message("Connect to GoPenguin before joining.");return;}
 snprintf(lobby.username,sizeof lobby.username,"%s",demo.actors[0].name);
 if(!lobby_begin(&lobby,kind,id)){message(lobby.error?lobby.error:"This queue is full or another entry is pending.");return;}
 lobbyUI.open=true;lobbyUI.point=NULL;lobbyUI.returning=false;demo.tab=17;
}
static bool lobby_return_race(void){
 const LobbyGeometrySeat *point=lobby_geometry_lookup(230,LOBBY_WADDLE,lobbyUI.cancelledWaddle,lobbyUI.cancelledSeat);
 int x=point?point->doneX:(int)demo.actors[0].x,y=point?point->doneY:(int)demo.actors[0].y;
 lobby_stop();pnet_manual_room(false);
 if(pnet_state()!=PNET_ONLINE||!pnet_join(230,x,y)){lobbyUI.raceReturn=lobbyUI.open=true;message("Could not return from the race. Retry returning to the Mountain.");return false;}
 lobbyUI.cancelledWaddle=0;lobbyUI.raceReturn=lobbyUI.open=false;demo.tab=0;return true;
}
static bool lobby_apply(const PNetMessage *packet){
 if(lobbyUI.cancelledWaddle&&!strcmp(packet->command,"jw")&&packet->count==1){
  char *end;unsigned long seat=strtoul(packet->args[0],&end,10);const LobbyEntry *entry=lobby_entry(&lobby,lobbyUI.cancelledWaddle);
  if(*packet->args[0]&&!*end&&entry&&seat<entry->capacity){lobbyUI.cancelledSeat=(int)seat;return true;}
 }
 if(lobbyUI.cancelledWaddle&&currentRoomId==230&&!strcmp(packet->command,"jg")&&packet->count==1&&!strcmp(packet->args[0],"999")){
  if(lobby_return_race())message("The queue started while leaving. Returning to the Mountain.");
  return true;
 }
 if(!lobby_packet(&lobby,packet->command,packet->count,packet->args))return false;
 if(lobbyUI.cancelledWaddle&&!strcmp(packet->command,"gw")){
  const LobbyEntry *entry=lobby_entry(&lobby,lobbyUI.cancelledWaddle);bool present=false,fresh=false;char prefix[16];snprintf(prefix,sizeof prefix,"%u|",lobbyUI.cancelledWaddle);
  for(unsigned i=0;i<packet->count;i++)if(!strncmp(packet->args[i],prefix,strlen(prefix)))fresh=true;
  if(fresh&&entry&&entry->known){for(unsigned i=0;i<entry->capacity;i++)if(!strcmp(entry->names[i],lobby.username))present=true;if(!present)lobbyUI.cancelledWaddle=0;}
 }
 lobbyUI.artDirty=true;
 if(!strcmp(packet->command,"jt")||!strcmp(packet->command,"jw")){
  const LobbyGeometrySeat *point=lobby_geometry_lookup(lobby.room,lobby.kind,lobby.selected,lobby.seat);
  if(point)lobby_walk(point,false);
 }
#ifdef GP_LAN_ONLY
 FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"lobby command=%s context=%u seat=%d state=%u players=%u\n",packet->command,lobby.selected,lobby.seat,(unsigned)lobby.state,demo.count);fclose(trace);}
#endif
 return true;
}
static void lobby_close(void){
 if(lobbyUI.raceReturn){lobby_return_race();return;}
 if(lobbyUI.returning&&lobbyUI.moving)return;
 if(lobby.state==LOBBY_READY&&lobby.kind==LOBBY_WADDLE){lobby_launch();return;}
 if(lobbyUI.leavePending){if(!game_send(NULL,'s',"lt",0,NULL)){message("Could not leave the table. Try again.");return;}lobbyUI.leavePending=false;}
 const LobbyGeometrySeat *point=lobbyUI.point;bool seated=lobby.seat>=0;unsigned cancelled=lobby.kind==LOBBY_WADDLE&&lobby.needsLeave?lobby.selected:0;int seat=lobby.seat;
 if(!lobby_cancel(&lobby)){message(lobby.error?lobby.error:"Could not leave this activity.");return;}
 if(cancelled){lobbyUI.cancelledWaddle=cancelled;lobbyUI.cancelledSeat=seat;}
 lobby_stop();lobbyUI.open=false;lobbyUI.refresh=true;demo.tab=0;triggerDelay=2;
 if(seated&&point)lobby_walk(point,true);else lobbyUI.point=NULL;
}
static void lobby_disconnected(void){
 bool active=lobby_locked()||lobby.state==LOBBY_LAUNCHED;
 lobby_stop();lobbyUI.open=lobbyUI.leavePending=lobbyUI.raceReturn=false;lobbyUI.point=NULL;lobbyUI.routeRoom=lobbyUI.cancelledWaddle=0;
 lobby_init(&lobby,game_send,NULL);lobby_enter(&lobby,currentRoomId,demo.actors[0].name);lobbyUI.artDirty=true;lobbyUI.refresh=true;
 if(active){demo.tab=0;if(modal.open&&(!strcmp(modal.command,"lobby-table")||!strcmp(modal.command,"lobby-waddle")))modal.open=false;message("Connection lost. The multiplayer entry has ended.");}
}
static void lobby_update(float seconds){
 if(lobbyUI.refresh&&!lobbyUI.raceReturn&&pnet_state()==PNET_ONLINE&&!game&&!gameQueued){lobbyUI.refresh=false;if(lobby.entryCount&&!lobby_refresh(&lobby))message("Could not refresh the tables or race queues.");}
 LobbyState before=lobby.state;unsigned context=lobby.selected;int seat=lobby.seat;lobby_tick(&lobby,seconds);
 if(lobby.state==LOBBY_FAILED&&before!=LOBBY_FAILED){
  if(lobby.kind==LOBBY_WADDLE&&!lobby.needsLeave){lobbyUI.cancelledWaddle=context;lobbyUI.cancelledSeat=seat;lobbyUI.refresh=true;}
  lobby_stop();if(lobbyUI.point)lobby_walk(lobbyUI.point,true);message(lobby.error?lobby.error:"This entry could not be completed.");
 }
 if(lobby.state==LOBBY_READY&&lobby.kind==LOBBY_WADDLE)lobby_launch();
 lobby_art();
}
static void lobby_progress(float seconds){
 if(!lobbyUI.moving){lobby_launch();return;}
 lobbyUI.walkTime+=seconds;
 if(lobbyUI.walkTime>12){lobby_stop();lobby_close();message("The seating walk could not finish. Please try again.");return;}
 if(demo.actors[0].walking)return;
 const LobbyGeometrySeat *point=lobbyUI.point;bool returning=lobbyUI.returning;lobbyUI.moving=false;
 if(!returning&&point){demo.actors[0].action=point->frame;demo.actors[0].clock=0;char frame[12];snprintf(frame,sizeof frame,"%u",point->frame);if(pnet_state()==PNET_ONLINE)pnet_send("u#sf",1,(const char *[]){frame});}
 else{lobbyUI.point=NULL;lobbyUI.returning=false;triggerDelay=2;}
 lobby_launch();
}
static void lobby_draw(void){
 const char *key=lobby_key(lobby.room,lobby.kind,lobby.selected?lobby.selected:lobby.entryCount?lobby.entries[0].id:0);text(12,33,12,0x003366,lobby_title(key));
 if(lobbyUI.raceReturn){wrapped(15,70,12,0x003366,"The Mountain return could not be sent. Retry before joining another race.",290,5);button(12,207,296,29,"Retry returning to Mountain",-1);return;}
 if(lobbyUI.leavePending){wrapped(15,70,12,0x003366,"The table could not be left. Retry before joining another game.",290,5);button(12,207,296,29,"Retry leaving table",-1);return;}
 if(lobby.state==LOBBY_JOINING||lobby.state==LOBBY_WAITING||lobby.state==LOBBY_READY||lobbyUI.moving){
  const char *status=lobbyUI.returning?"Returning from the activity...":lobbyUI.moving?"Walking to your seat...":lobby.state==LOBBY_JOINING?"Joining...":lobby.state==LOBBY_READY?"Opening game...":"Waiting for other penguins...";center(160,65,11,0x003366,status);
  const LobbyEntry *entry=lobby_entry(&lobby,lobby.selected);if(entry&&lobby.kind==LOBBY_WADDLE)for(unsigned i=0;i<entry->capacity;i++){char label[96];snprintf(label,sizeof label,"%u. %.63s",i+1,entry->names[i][0]?entry->names[i]:"Empty");text(20,91+i*24,11,0x003366,label);}
  button(12,207,296,29,lobbyUI.returning?"Returning...":"Leave",-1);return;
 }
 if(lobby.state==LOBBY_FAILED)wrapped(12,53,10,0x003366,lobby.error?lobby.error:"Could not join. Choose a table or race again.",296,2);
 else text(12,53,10,0x003366,lobby.kind==LOBBY_TABLE?"Choose a table. Full tables can be watched.":"Choose a race queue.");
 for(unsigned i=0;i<lobby.entryCount;i++){
  const LobbyEntry *entry=&lobby.entries[i];char label[80];if(entry->known)snprintf(label,sizeof label,"%s %u: %u / %u players",lobby.kind==LOBBY_TABLE?"Table":"Race",i+1,entry->count,entry->capacity);else snprintf(label,sizeof label,"%s %u: loading players...",lobby.kind==LOBBY_TABLE?"Table":"Race",i+1);
  button(9,83+i*23,302,21,label,-1);
 }
 button(12,207,142,29,"Back",-1);button(166,207,142,29,"Refresh",-1);
}
static void lobby_touch(int x,int y){
 if(lobbyUI.raceReturn){if(y>=207)lobby_return_race();return;}
 if(y>=207){if(lobbyUI.returning&&lobbyUI.moving)return;if(x<160||lobbyUI.leavePending||lobby.state==LOBBY_JOINING||lobby.state==LOBBY_WAITING||lobby.state==LOBBY_READY)lobby_close();else lobbyUI.refresh=true;return;}
 if(lobby.state==LOBBY_JOINING||lobby.state==LOBBY_WAITING||lobby.state==LOBBY_READY||lobbyUI.moving||lobbyUI.leavePending)return;
 if(y>=83&&y<83+(int)lobby.entryCount*23){unsigned index=(unsigned)(y-83)/23;lobby_prompt(lobby.kind,lobby.entries[index].id);}
}
static void game_start(const char *key){
 if(game||gameQueued)return;
#ifdef GP_NINJA_GAMES
 const char *npcGame=room_npc_route(currentRoomId,key);bool fromSensei=npcGame!=NULL;if(npcGame)key=npcGame;
#endif
 if(!game_port_supported(key)){modal_message("This game is not available yet.");return;}
 if(book.open||igloo.dirty||igloo.awaitingSave||igloo.awaitingStyle||igloo.awaitingMusic||styles.preparing){modal_message("Close the window and finish saving your igloo before playing.");return;}
 if(!strcmp(key,"four")||!strcmp(key,"mancala")||!strcmp(key,"sled")){
  if(lobby.needsLeave||lobbyUI.moving||lobbyUI.leavePending){message("Leave the current table or race queue first.");return;}
  unsigned room=!strcmp(key,"mancala")?111:!strcmp(key,"sled")?230:currentRoomId==221?221:220;
  if(currentRoomId!=room){lobbyUI.routeRoom=room;wantedRoom=room;wantedX=wantedY=-1;}else{lobbyUI.open=true;lobbyUI.refresh=true;demo.tab=17;}
  return;
 }
 if(lobby_locked()){message("Leave the multiplayer panel before starting another game.");return;}
 room_tube_cancel(&roomTube,&tubeHooks);
 game_snapshot(key);
#ifdef GP_NINJA_GAMES
 queuedGame.fromSensei=fromSensei;
#endif
}
static void game_restore(void){
 bool table=queuedGame.table!=0,lost=gameMultiplayerLost||pnet_state()!=PNET_ONLINE;
 const LobbyGeometrySeat *point=table?lobby_geometry_lookup(queuedGame.returnRoom,LOBBY_TABLE,queuedGame.table,queuedGame.seat):NULL;
 pnet_manual_room(false);game_port_free(game);game=NULL;gameLeaving=false;gameQueued=false;wantedRoom=0;demo.tab=0;modal.open=false;sceneOK=room_textures(currentRoom);lobby_release(&lobby);lobbyUI.open=false;lobby_stop();
 if(table){
  if(!lost){
   if(!game_send(NULL,'s',"lt",0,NULL)){lobbyUI.leavePending=lobbyUI.open=true;demo.tab=17;message("Could not leave the table. Retry leaving before another game.");}
   if(point)lobby_walk(point,true);
  }
  lobbyUI.refresh=true;lobbyUI.artDirty=true;
 }else{
  demo.count=1;Actor *me=&demo.actors[0];me->x=me->tx=queuedGame.waddle?fmaxf(20,fminf(740,gameReturnX)):gameReturnX;me->y=me->ty=queuedGame.waddle?fmaxf(20,fminf(430,gameReturnY)):gameReturnY;me->walking=false;
  if(pnet_state()==PNET_ONLINE&&!pnet_join(gameReturnRoom,gameReturnX,gameReturnY)&&queuedGame.waddle){lobbyUI.cancelledWaddle=queuedGame.waddle;lobbyUI.cancelledSeat=queuedGame.seat;lobbyUI.raceReturn=lobbyUI.open=true;message("Could not return from the race. Retry returning to the Mountain.");}
  if(gameReturnRoom!=currentRoomId){if(gameReturnRoom>2000)open_igloo(gameReturnRoom-2000);else{wantedRoom=gameReturnRoom;wantedX=gameReturnX;wantedY=gameReturnY;}}
 }
 petRoom=0;pets_enter(roomBank?igloo.layout.owner:0);gp_audio_scene(roomBank&&igloo.layout.type==1?"igloo1":currentRoom->key);if(roomBank)gp_audio_music(igloo.layout.music);else gp_audio_room(currentRoom->key);triggerDelay=2;gameMultiplayerLost=false;
 if(!sceneOK)message("Could not restore the room artwork.");
}
static void game_transition(void){
 if(gameLeaving)game_restore();
 if(!gameQueued)return;
 pnet_manual_room(queuedGame.online&&!queuedGame.table);
 // Texture releases and loading happen after the previous GPU frame has finished.
 release_room_textures();wear_free();gp_audio_scene(queuedGame.key);gp_audio_game(queuedGame.key);modal.open=false;igloo.editing=false;demo.actors[0].walking=false;if(!queuedGame.table)demo.count=1;
 const GameHost host={.notice=game_notice,.coins=game_coins,.resource=game_resource,.send=game_send,.leave=game_leave,.sound=game_sound,.stop_sounds=game_stop_sounds,.text=game_text,.rect=game_rect,.button=game_button};
 game=game_port_open(&queuedGame,&host);gameQueued=false;
 if(!game){gameReturnRoom=queuedGame.returnRoom;gameReturnX=queuedGame.returnX;gameReturnY=queuedGame.returnY;game_restore();message("Could not open this game.");}
}
static void games_draw(void){
 text(12,33,12,0x003366,"Games");unsigned count=0;
 for(unsigned i=0;i<COUNT(gameKeys);i++)if(game_port_supported(gameKeys[i])){unsigned n=count++;if(n>=gamePage*6&&n<gamePage*6+6)button(9,60+(n-gamePage*6)*21,302,20,gameNames[i],-1);}
 if(!count)center(160,105,11,0x003366,"No games are available.");
 button(9,187,75,18,"Previous",-1);button(236,187,75,18,"Next",-1);
}
static void games_touch(int x,int y){
 unsigned count=0;for(unsigned i=0;i<COUNT(gameKeys);i++)if(game_port_supported(gameKeys[i]))count++;
 if(y>=187){if(x<85&&gamePage)gamePage--;else if(x>=236&&(gamePage+1)*6<count)gamePage++;return;}
 if(y<60||y>=186)return;
 unsigned target=gamePage*6+(unsigned)(y-60)/21,n=0;
 for(unsigned i=0;i<COUNT(gameKeys);i++)if(game_port_supported(gameKeys[i])&&n++==target){game_start(gameKeys[i]);return;}
}
/** Lower-screen header: the room in Room, otherwise the open section. */
static const char *section_title(void){
 static const char *const sections[]={"Room","Map","Me","People","Help","Chat","Books","Player","Mail","Postcards","Public igloos","Igloo styles","Puffles","Puffles","Puffles","Igloo music","Games","Lobby","Account","Membership","Updates","Device code","Transfer penguin"};
 if(demo.tab==0&&currentRoom)return currentRoom->title;
 return demo.tab<COUNT(sections)?sections[demo.tab]:"Penguin";
}
static void bottom(void){
 if(chatKeyboard.active&&!modal.open){C2D_ViewReset();C2D_SetTintMode(C2D_TintMult);const ChatKeyboardWidgets widgets={box,chat_font_text,chat_font_center,chat_font_width};chat_keyboard_draw(&chatKeyboard,osGetTime(),&widgets);return;}
 if(game){game_port_draw_lower(game,sceneBottom);game_port_draw_bottom(game);return;}
 ui_frame();ui_background();header(book.open?book.info->title:section_title());
 if(modal.open){modal_draw();return;}
 if(lobbyUI.open||lobbyUI.moving||lobbyUI.leavePending){lobby_draw();return;}
 if(igloo.editing){editor_draw();header("Edit igloo");return;}
 if(chatHistory.open&&!book.open){header("Chat history");const ChatLogWidgets widgets={box,text,history_width,history_button};chat_log_view_draw(&chatHistory,&chatLog,&widgets);return;}

 if(book.open){
  rect(0,28,320,177,0xffffff);
  if(book.bank){movie_bank_draw(book.bank,sceneBottom,320,240,book_view(320,177,28,false),book.clock*swl_rate(movie_bank_scene(book.bank)));C2D_ViewReset();C2D_SetTintMode(C2D_TintMult);}
  if(book.info->id==1||book.info->archive){box(3,29,88,18,0x0085c7);center(47,31,10,0xffffff,"Past issues");}
  if(!book.bank)center(160,105,12,0x003366,settings.assets[0]?(newspaper.open?"Loading newspaper...":book.resourceId?"Loading postcard...":"Loading catalogue..."):"Set a resources server in Help.");
  if(book.bank&&book.resourceId&&book.mailUid){button(4,207,98,22,"Reply",-1);button(110,207,98,22,"Delete",-1);button(216,207,100,22,"Delete sender",-1);}else if(book.bank&&book.resourceId&&postcardId){button(25,207,120,22,"Send: 10 coins",-1);button(175,207,120,22,"Cancel",-1);}
  else if(!book.resourceId){button_state(7,207,75,21,"Previous",-1,book.bank&&!book.wanted&&book.page>1);button_state(238,207,75,21,"Next",-1,book.bank&&!book.wanted&&book.page<book.info->frames);button_state(90,207,140,21,book.zoom?"Overview (L/R)":"Zoom (L/R)",-1,book.bank!=NULL);}
  float footer=book.resourceId?230:228;rect(0,footer,320,240-footer,0x003366);char label[90];snprintf(label,sizeof label,"%u / %u  %s    B: close",newspaper.open?newspaper.page:book.page,newspaper.open?NEWSPAPER_PAGES:book.info->frames,book.wanted?"Loading...":book.zoom?"Drag / Circle Pad":"Tap an item");button_label(160,(footer+240)/2,10,0xffffff,label);return;
 }
 if(demo.tab==0){
  box(6,33,181,119,0xffffff);if(roomBank){ActorOrder order;make_order(&order,false,175.0f/760,9,36);scene_draw_to(sceneBottom,320,240,175.0f/760,110.0f/480,9,36,&order);}else image(&minimap,9,36,175,110,0xffffffff);
  for(unsigned i=0;i<demo.count;i++){Actor *a=&demo.actors[i];C2D_DrawCircleSolid(9+a->x*175/760,36+a->y*110/480,.5f,i?2:3,rgb(colours[(unsigned)a->colour<16?(unsigned)a->colour:0]));}
 if(roomBank){button(7,154,101,17,"Public igloos",-1);if(igloo.layout.owner==pnet_id())button(112,154,75,17,"Styles",-1);}else text(12,154,10,0x003366,"L: history  R: write");
  const char *actions[]={"Wave","Dance","Sit","Snowball","Chat","Zoom"};const int ico[]={-1,-1,-1,3,0,6},poses[]={25,26,17};
  for(int i=0;i<6;i++){
   float x=193+i%2*62,y=34+i/2*34;
   if(i>=3){button(x,y,59,32,actions[i],ico[i]);continue;}
   // Like the original action menu, show the player's own penguin in each pose.
   float dy=ui_button(x,y,59,32,ui_touching(x,y,59,32),true)?1:0;Actor pose=demo.actors[0];pose.action=poses[i];
   actor(&pose,x+29.5f,y+16+dy,.4f);
   button_label(x+29.5f,y+25+dy,11,0xffffff,actions[i]);
  }
  ui_button(193,139,121,26,false,false);icon(9,198,140,.75f);button_label(270,152,11,0x003366,"Membership");
  bool owner=roomBank&&igloo.layout.owner==local_player();box(7,173,owner?166:240,28,0xffffff);icon(0,10,174,.8f);text(41,179,11,0x003366,"Say something...");if(owner)button(177,173,70,28,"Music",-1);button(253,173,60,28,owner?"Edit":"Igloo",-1);
 }else if(demo.tab==1){
  if(mapList){for(unsigned i=0;i<10;i++){unsigned index=mapPage*10+i;if(index>=ISLAND_ROOMS)break;const IslandRoom *r=&islandRooms[index];box(7+i%2*156,34+i/2*29,150,26,r==currentRoom?0x0085c7:0xffffff);center(82+i%2*156,40+i/2*29,11,r==currentRoom?0xffffff:0x003366,r->title);}box(7,183,71,20,0xffffff);center(42,186,11,0x003366,"Previous");box(242,183,71,20,0xffffff);center(277,186,11,0x003366,"Next");char page[40];snprintf(page,sizeof page,"%u / %u",mapPage+1,(ISLAND_ROOMS+9)/10);center(160,186,11,0x003366,page);}
  // Full width between the header and the tabs, from a raster made at this size (map-bottom, 1:1).
  else{if(islandMapLow.loaded)crop(&islandMapLow,0,0,320,MAP_LOW_VISIBLE,0,MAP_LOW_TOP,1,0xffffffff);if(pnet_state()==PNET_OFFLINE)text(318-textWidth(10,"Select: all 46 rooms"),9,10,0xffffff,"Select: all 46 rooms");}
 }else if(demo.tab==2){
  inventory_draw(&inventoryUI,clientData,&profile,&demo.actors[0],&inventoryWidgets);
 }else if(demo.tab==3){people_draw();
 }else if(demo.tab==6){books_draw();
 }else if(demo.tab==8){mail_draw();
 }else if(demo.tab==9){postcards_draw();
 }else if(demo.tab==10){igloo_list_draw();
 }else if(demo.tab==11){styles_draw();
 }else if(demo.tab>=12&&demo.tab<=14){pets_draw();
 }else if(demo.tab==15){music_draw();
 }else if(demo.tab==16){games_draw();
 }else if(demo.tab==18){penguin_draw();
 }else if(demo.tab==19){membership_draw();
 }else if(demo.tab==20){update_draw();
 }else if(demo.tab==21){device_draw();
 }else if(demo.tab==22){transfer_draw();
 }else if(demo.tab==7){player_draw();
 }else if(demo.tab==4){
  button(8,32,96,24,"News",-1);button(112,32,96,24,"Puffles",-1);button(216,32,96,24,"Games",-1);
  float dy=ui_button(8,62,304,56,ui_touching(8,62,304,56),true)?1:0;
  icon(2,19,74+dy,1);text(61,73+dy,13,0xffffff,"Account");
  text(61,95+dy,9,0xffffff,"Transfer your penguin or change its name.");
  button(8,130,148,28,settings.audio?"Audio: on":"Audio: off",-1);button(164,130,148,28,settings.batterySaver?"Battery saver: on":"Battery saver: off",-1);
  button(8,164,148,28,"Catalogues",-1);char label[32];snprintf(label,sizeof label,"Mail (%u)",social.unread);button(164,164,148,28,label,-1);
 }else{
  box(9,33,90,22,0xffffff);center(54,37,11,0x003366,"Safe chat");box(104,33,90,22,0xffffff);center(149,37,11,0x003366,"Emotes");box(199,33,112,22,0xffffff);center(255,37,11,0x003366,safeParent?"Up":"Give tour");
  if(emoteMenu){for(unsigned i=0;i<30;i++){float x=9+i%6*51,y=58+i/6*29;box(x,y,48,27,0xffffff);const EmoteArt *f=&emoteArt[i];float k=fminf(42.0f/f->w,24.0f/f->h);crop(&emotes,f->x,f->y,f->w,f->h,x+(48-f->w*k)/2,y+(27-f->h*k)/2,k,0xffffffff);}}
  else{SafeMessage choices[6];unsigned total=client_safe_children(clientData,safeParent,safePage*6,choices,6);for(unsigned i=0;i<6&&safePage*6+i<total;i++){char label[64];snprintf(label,sizeof label,"%.45s",choices[i].name);if(strlen(choices[i].name)>45)strcpy(label+42,"...");box(9,58+i*22,302,20,0xffffff);text(17,60+i*22,11,0x003366,label);}box(9,192,65,13,0xffffff);center(42,192,10,0x003366,"Previous");box(246,192,65,13,0xffffff);center(278,192,10,0x003366,"Next");}
 }
 ui_tab_bar();for(int i=0;i<5;i++){
  // Centre icon and word as one group; capitals share the icon's midline so baselines match.
  FontBounds b=font_ink(10,tabs[i]),cap=font_ink(10,"H");float iconSize=32*.62f,gap=2,group=iconSize+gap+(b.right-b.left),x=i*64+2+(60-group)/2,mid=223;
  ui_tab(i*64+2,208,60,30,i==demo.tab);icon(tabIcons[i],x,mid-iconSize/2,.62f);text(x+iconSize+gap-b.left,mid-(cap.top+cap.bottom)/2.0f,10,i==demo.tab?0xffdd33:0xffffff,tabs[i]);}
}
static void room_hover(u32 held){
 IslandButton button={0};unsigned next=~0u;
 if(navigation&&!lobby_locked()&&!igloo.editing&&!modal.open&&!book.open&&demo.tab==0&&(held&KEY_TOUCH)){touchPosition point;hidTouchRead(&point);if(point.px>=9&&point.px<=184&&point.py>=36&&point.py<=146&&epf_access_button(&epfAccess,(int)((point.px-9)*760.0f/175),(int)((point.py-36)*480.0f/110),&button))next=button.index;}
 if(next==hoveredIndex)return;
 for(unsigned i=0;i<hoverCount;i++){IslandHover hover=island_hover(navigation,hoverFirst+i);if(hover.path)swl_target(scene,hover.path,hover.out,NULL,false);}
 hoveredIndex=next;hoverCount=next==~0u?0:button.hovers;hoverFirst=button.firstHover;
 for(unsigned i=0;i<hoverCount;i++){IslandHover hover=island_hover(navigation,hoverFirst+i);if(hover.path)swl_target(scene,hover.path,hover.over,NULL,hover.playOver);}
}
static void touch(int x,int y){
#ifdef GP_NINJA_GAMES
 roomNpc.approaching=false;
#endif
 if(room_tube_locked(&roomTube)){if(y>=207){room_tube_cancel(&roomTube,&tubeHooks);demo.tab=x/64;}return;}
 if(igloo.editing&&!modal.open){editor_touch(x,y);return;}
 if(modal.open){
  if(y>=174&&y<=200){
   bool confirm=modal.confirm&&x>=25&&x<145;
   if(confirm&&(!strcmp(modal.command,"lobby-table")||!strcmp(modal.command,"lobby-waddle")))lobby_join(!strcmp(modal.command,"lobby-table")?LOBBY_TABLE:LOBBY_WADDLE,(unsigned)strtoul(modal.argument,NULL,10));
   else if(confirm&&!strcmp(modal.command,"open-update"))demo.tab=20;
   else if(confirm&&!strcmp(modal.command,"igloo-style")){styles.type=(unsigned)strtoul(modal.argument,NULL,10);styles.preparing=true;styles.started=osGetTime();}
   else if(confirm&&!strcmp(modal.command,"puffle-adopt")){unsigned type=(unsigned)strtoul(modal.argument,NULL,10);char name[13]="";if(keyboard(name,sizeof name,"Puffle name",false))puffles_adopt(&puffles,type,name);}
   else if(confirm&&!strcmp(modal.command,"l#ms")){char card[20];snprintf(card,sizeof card,"%u",postcardId);pnet_send("l#ms",2,(const char *[]){modal.argument,card});}
   else if(confirm&&modal.room){wantedRoom=modal.room;wantedX=wantedY=-1;}
   else if(confirm&&*modal.command){pnet_send(modal.command,1,(const char *[]){modal.argument});if(!strncmp(modal.command,"b#",2))pnet_send("b#gb",0,NULL);if(!strncmp(modal.command,"n#",2))pnet_send("n#gn",0,NULL);if(!strncmp(modal.command,"l#md",4)){book.close=true;demo.tab=8;pnet_send("l#mg",1,(const char *[]){"0"});pnet_send("l#mst",0,NULL);}pnet_send("b#gb",0,NULL);}
   else if(confirm){
    if(modal.kind==3&&(igloo.dirty||igloo.awaitingSave||igloo.awaitingStyle||igloo.awaitingMusic)){modal_message("Save your igloo changes before buying flooring.");return;}
    if(offline_active()){
     // Offline preview: charge the local store and queue the reply the server would send.
     char reply[96];Product product;
     if(!client_product(clientData,modal.kind,modal.purchase,&product)||!offline_buy(&offlineShop,modal.kind,modal.purchase,product.cost,reply,sizeof reply)){modal_message("This purchase is not available.");return;}
     if(!offline_save(&offlineShop,OFFLINE_SHOP_FILE)){modal_message("The offline store could not be saved on SD.");return;}
     local_push(reply);if(modal.kind==3&&roomBank&&igloo.layout.owner==OFFLINE_OWNER)offline_igloo_apply();
    }else{
    char id[20];snprintf(id,sizeof id,"%u",modal.purchase);const char *commands[]={"i#ai","g#af","g#au","g#ag"};pnet_send(commands[modal.kind<4?modal.kind:0],1,(const char *[]){id});
    }
   }
   if(!modal.confirm||confirm||x>=175)modal.open=false;
  }
  return;
 }
 if(lobbyUI.open||lobbyUI.moving||lobbyUI.leavePending){lobby_touch(x,y);return;}
 if(!book.open&&y>=207){demo.tab=x/64;return;}
 if(demo.tab==6&&!book.open){if(y>=185){if(x<100&&bookListPage)bookListPage--;if(x>=221&&(bookListPage+1)*6<BOOK_COUNT)bookListPage++;}else if(y>=34&&y<184){unsigned col=x>=164,row=(unsigned)(y-34)/50,i=bookListPage*6+row*2+col;if(row<3&&i<BOOK_COUNT)book_open(bookInfo[i].key);}return;}
 if(book.open){
  if(!book.resourceId&&y>=207&&y<228){if(x<85){if(book.page>1&&!book.wanted)book_page(NULL,book.page-1);}else if(x>=235){if(book.page<book.info->frames&&!book.wanted)book_page(NULL,book.page+1);}else book_toggle_zoom(false);return;}
  if((book.info->id==1||book.info->archive)&&x<92&&y>=29&&y<=48){book.close=true;demo.tab=6;bookListPage=2;return;}
  if(book.bank&&book.resourceId&&!newspaper.open&&y>=207&&y<230){
   if(book.mailUid){
    if(x<105){postcardRecipient=0;for(unsigned i=0;i<social.nMail;i++)if(social.mail[i].uid==book.mailUid)postcardRecipient=social.mail[i].senderId;book.close=true;demo.tab=9;postcardPage=0;}
    else{confirm_command(x<212?"Delete this postcard?":"Delete all postcards from this sender?",x<212?"l#md":"l#mdp",book.mailUid);if(x>=212)for(unsigned i=0;i<social.nMail;i++)if(social.mail[i].uid==book.mailUid)snprintf(modal.argument,sizeof modal.argument,"%u",social.mail[i].senderId);}
   }else if(postcardId){if(x<160){modal_message("Send this postcard for 10 coins?");modal.confirm=true;snprintf(modal.command,sizeof modal.command,"l#ms");snprintf(modal.argument,sizeof modal.argument,"%u",postcardRecipient);}else book.close=true;}
   return;
  }
  if(book.bank&&!book.wanted){MovieHooks hooks=bookHooks;if(book.resourceId&&!newspaper.open)hooks.page=NULL;SWLMatrix view=book_view(320,177,28,false);movie_meta_click(movie_bank_meta(book.bank),movie_bank_scene(book.bank),(x-view.x)/view.a,(y-view.y)/view.d,&hooks,NULL);}
  return;
 }
 if(y>=207){demo.tab=x/64;return;}
 if(demo.tab==0){
  if(x>=9&&x<=184&&y>=36&&y<=146){
   float wx=(x-9)*760.0f/175,wy=(y-36)*480.0f/110;
   if(aiming){aiming=false;throw_to(&demo.actors[0],wx,wy);if(pnet_state()==PNET_ONLINE){char sx[20],sy[20];snprintf(sx,sizeof sx,"%d",(int)lroundf(wx));snprintf(sy,sizeof sy,"%d",(int)lroundf(wy));pnet_send("u#sb",2,(const char *[]){sx,sy});}return;}
   IslandButton button;
#ifdef GP_NINJA_GAMES
   float npcX,npcY;if(room_npc_click(&roomNpc,(int)wx,(int)wy,&npcX,&npcY)){demo_target(&demo,npcX,npcY);return;}
#endif
   if(navigation&&epf_access_button(&epfAccess,(int)wx,(int)wy,&button)){
    bool scripted=false;
    for(unsigned i=0;i<island_events(navigation);i++){IslandEvent event=island_event(navigation,i);unsigned frame=*event.guard?swl_frame(scene,event.guard):1;if(event.type==2&&event.id==button.index&&frame>=event.min&&frame<=event.max){swl_target(scene,event.path,event.frame,NULL,event.playing);scripted=true;}}
    if(!scripted){if(*button.content)content(button.content);else if(button.x>0&&button.y>0)demo_target(&demo,button.x,button.y);}
   }else demo_target(&demo,wx,wy);
   return;
  }
  if(roomBank&&x<187&&y>=154&&y<171){if(x>=112&&igloo.layout.owner==pnet_id())styles_open();else igloo_list_open();return;}
  if(y>=173&&y<=201){if(x>=253){if(roomBank&&igloo.layout.owner==local_player())editor_open();else open_igloo(pnet_id());}else if(x>=177&&roomBank&&igloo.layout.owner==pnet_id())music_open();else chat();return;}
  if(x>=193&&x<314&&y>=34&&y<134){int n=(y-34)/34*2+(x-193)/62;if(n==0)action(25);if(n==1)action(26);if(n==2)action(17);if(n==3)snowball();if(n==4)demo.tab=5;if(n==5)demo.zoom=!demo.zoom;}
  if(x>=193&&x<314&&y>=139&&y<165){membership_open();return;}
 }
 else if(demo.tab==1){if(mapList){if(y>=183){if(x<80&&mapPage)mapPage--;if(x>=242&&(mapPage+1)*10<ISLAND_ROOMS)mapPage++;}else if(y>=34&&y<179){unsigned index=mapPage*10+(unsigned)(y-34)/29*2+(x>=163);if(index<ISLAND_ROOMS){wantedRoom=islandRooms[index].id;wantedX=wantedY=-1;demo.tab=0;}}}else{float scale=320.0f/555,mx=x/scale,my=(y-MAP_LOW_TOP)/scale;if(y<MAP_LOW_TOP||y>=MAP_LOW_TOP+MAP_LOW_VISIBLE)return;for(unsigned i=0;i<COUNT(mapButtons);i++){const MapButton *b=&mapButtons[i];if(mx<b->x1||mx>b->x2||my<b->y1||my>b->y2)continue;if(b->id==65535)igloo_list_open();else{if(b->id){wantedRoom=b->id;wantedX=wantedY=-1;}demo.tab=0;}break;}}}
 else if(demo.tab==2)inventory_touch(&inventoryUI,clientData,&profile,&demo.actors[0],&inventoryWidgets,x,y);
 else if(demo.tab==3)people_touch(x,y);
 else if(demo.tab==7)player_touch(x,y);
 else if(demo.tab==8)mail_touch(x,y);
 else if(demo.tab==9)postcards_touch(x,y);
 else if(demo.tab==10)igloo_list_touch(x,y);
 else if(demo.tab==11)styles_touch(x,y);
 else if(demo.tab>=12&&demo.tab<=14)pets_touch(x,y);
 else if(demo.tab==15)music_touch(x,y);
 else if(demo.tab==16)games_touch(x,y);
 else if(demo.tab==18)penguin_touch(x,y);
 else if(demo.tab==19)membership_touch(x,y);
 else if(demo.tab==20)update_touch(x,y);
 else if(demo.tab==21)device_touch(x,y);
 else if(demo.tab==22)transfer_touch(x,y);
 else if(demo.tab==4){
  if(y>=32&&y<56){
   if(x<108)newspaper_show();
   else if(x<212){if(pnet_state()!=PNET_ONLINE){modal_message("Connect to GoPenguin to manage puffles.");return;}demo.tab=12;petPage=0;puffles_request(&puffles,pnet_id());}
   else{demo.tab=16;gamePage=0;}
  }
  else if(y>=62&&y<118)penguin_open();
  else if(y>=130&&y<158){if(x<160)toggle_audio();else toggle_battery();}
  else if(y>=164&&y<192){if(x<160){demo.tab=6;bookListPage=0;}else{demo.tab=8;mailPage=0;pnet_send("l#mg",1,(const char *[]){"0"});}}
 }
 else if(demo.tab==5){if(y>=33&&y<55){if(x<100)emoteMenu=false;else if(x<195)emoteMenu=true;else if(safeParent){SafeMessage parent;if(client_safe(clientData,safeParent,&parent))safeParent=parent.parent;safePage=0;}else if(demo.actors[0].look[0]!=428)modal_message("You need to wear your Tour Guide hat to give a tour.");else{actor_say(&demo.actors[0],client_tour(clientData,currentRoomId));char id[20];snprintf(id,sizeof id,"%u",currentRoomId);pnet_send("u#sl",1,(const char *[]){id});demo.tab=0;}return;}if(emoteMenu&&x>=9&&y>=58&&y<203){unsigned id=(unsigned)(y-58)/29*6+(unsigned)(x-9)/51+1;if(id<=30){demo.actors[0].emote=id;demo.actors[0].chat[0]=0;demo.actors[0].bubbleTime=125.0f/24;char number[12];snprintf(number,sizeof number,"%u",id);pnet_send("u#se",1,(const char *[]){number});demo.tab=0;}}else if(!emoteMenu&&y>=58&&y<190){SafeMessage choices[6];unsigned total=client_safe_children(clientData,safeParent,safePage*6,choices,6),index=(unsigned)(y-58)/22;if(index<6&&safePage*6+index<total){SafeMessage choice=choices[index];if(client_safe_children(clientData,choice.id,0,NULL,0)){safeParent=choice.id;safePage=0;}else{actor_say(&demo.actors[0],choice.name);char id[20];snprintf(id,sizeof id,"%u",choice.id);pnet_send("u#ss",1,(const char *[]){id});demo.tab=0;}}}else if(!emoteMenu&&y>=192){unsigned total=client_safe_children(clientData,safeParent,0,NULL,0);if(x<75&&safePage)safePage--;if(x>=246&&(safePage+1)*6<total)safePage++;}}
}
int main(int argc,char **argv){
 gp_update_init(argc>0?argv[0]:NULL);
 gfxInitDefault();romfsInit();APT_CheckNew3DS(&isNew3DS);C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(8192);C2D_Prepare();C2D_SetTintMode(C2D_TintMult);
 mkdir("sdmc:/3ds",0777);mkdir(DATA_DIR,0700);settings_load(&settings,SETTINGS_FILE);
 power_apply();
#ifndef GP_LAN_ONLY
 if(settings.session[0])pnet_start(&settings.network,settings.session,"","",false);
 else if(!pnet_start_guest(&settings.network))message("Could not connect. Check your connection and try again.");
#endif
#ifdef GP_LAN_ONLY
 FILE *loadFlag=fopen(DATA_DIR "/load-test","r");if(loadFlag){loadTesting=true;fclose(loadFlag);}
#endif
#ifdef GP_LAN_ONLY
 profile_init(&profile,!settings.session[0]);
#else
 profile_init(&profile,false);
#endif
 clientData=client_data_load("romfs:/data/client.bin");json_error_t jsonError;shellText=json_load_file("romfs:/data/shell-text.json",0,&jsonError);
 C3D_RenderTarget *topTarget=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT),*bottomTarget=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
 sceneTop=topTarget;sceneBottom=bottomTarget;
 C3D_RenderTarget *targets[]={topTarget,bottomTarget};bool gpuOK=swl_gpu_init()&&swl_gpu_reserve(3);
 for(unsigned i=0;i<2;i++){void *stencil=vramAlloc(C3D_CalcDepthBufSize(targets[i]->frameBuf.width,targets[i]->frameBuf.height,GPU_RB_DEPTH24_STENCIL8));if(!stencil)gpuOK=false;else{if(targets[i]->ownsDepth)vramFree(targets[i]->frameBuf.depthBuf);C3D_FrameBufDepth(&targets[i]->frameBuf,stencil,GPU_RB_DEPTH24_STENCIL8);targets[i]->ownsDepth=true;}}
 
 gp_audio_init("romfs:/audio",settings.audio);aptHook(&audioHook,audio_focus,NULL);puffles_init(&puffles,pets_send,NULL);
 const char *sceneError=NULL;demo_init(&demo,NULL);lobby_init(&lobby,game_send,NULL);
 inventoryWidgets=(InventoryWidgets){box,text,center,portrait,portrait_hit,keyboard,message,item_colour,penguin_rename};
 bool ok=clientData&&gpuOK&&load(&icons,"icons")&&load(&emotes,"emotes")&&load(&fontTexture,"font")&&load(&paperBody,"paper-body")&&load(&paperDetail,"paper-detail")&&load(&bookCovers,"book-covers");
 if(fontTexture.loaded)C3D_TexSetFilter(&fontTexture.tex,GPU_NEAREST,GPU_NEAREST);
 for(int i=0;i<PENGUIN_PAGES;i++){char name[30];snprintf(name,sizeof name,"penguin-body-%d",i);ok=load(&body[i],name)&&ok;snprintf(name,sizeof name,"penguin-detail-%d",i);ok=load(&detail[i],name)&&ok;}
 ok=enter_room(100,-1,-1)&&ok;
 if(!ok){consoleInit(GFX_BOTTOM,NULL);printf("Could not load demo assets.\n%s\nRun tools/export-scene.mjs and rebuild.\nPress START to exit.\n",sceneError?sceneError:"");while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gspWaitForVBlank();}goto done;}
#ifdef GP_LAN_ONLY
 if(settings.session[0])pnet_start(&settings.network,settings.session,"","",false);
 else if(settings.network.api[0]&&settings.network.world[0])pnet_start_guest(&settings.network);
 else offline_begin();
#endif
 if(settings.assets[0]&&!asset_cache_start(settings.assets,DATA_DIR "/cache"))message("Could not prepare the resource cache. Check your SD card and resource settings.");
 u64 last=osGetTime(),sample=last,lastMove=0,updateCheckAt=last+10000;char offeredUpdate[24]="";unsigned frames=0;float sceneClock=0,lastX=demo.actors[0].x,lastY=demo.actors[0].y;bool wasMoving=false;PNetState previousState=PNET_OFFLINE;
 while(aptMainLoop()){
  u64 now=osGetTime();float dt=(now-last)/1000.0f;last=now;if(dt>.1f)dt=.1f;hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();
  bool historyInput=chatHistory.open&&!modal.open&&!game&&!book.open;
  if(historyInput){
   unsigned keys=0;if(down&KEY_L)keys|=CHAT_LOG_OLDER;if(down&KEY_R)keys|=CHAT_LOG_NEWER;if(down&KEY_DUP)keys|=CHAT_LOG_UP;if(down&KEY_DDOWN)keys|=CHAT_LOG_DOWN;if(down&KEY_A)keys|=CHAT_LOG_WRITE;if(down&KEY_B)keys|=CHAT_LOG_CLOSE;
   unsigned result=chat_log_view_input(&chatHistory,keys);
   if((down&KEY_TOUCH)&&chatHistory.open){touchPosition point;hidTouchRead(&point);result=chat_log_view_touch(&chatHistory,point.px,point.py);}
   if(result==CHAT_LOG_WRITE)chat();
   down=held=0;
  }
  bool keyboardInput=chatKeyboard.active&&!modal.open;
  if(keyboardInput){
   chat_keyboard_input(&chatKeyboard,chat_keys(down),chat_keys(held),now);
   if((held&KEY_TOUCH)||(hidKeysUp()&KEY_TOUCH)){static touchPosition point;if(held&KEY_TOUCH)hidTouchRead(&point);chat_keyboard_touch(&chatKeyboard,point.px,point.py,down&KEY_TOUCH,hidKeysUp()&KEY_TOUCH);}
   down=held=0;
  }
  if(down&KEY_START){if(game)game_port_close(game);else if(lobby_locked())lobby_close();else if(igloo.dirty||igloo.awaitingSave)editor_save();else break;}
  if(!keyboardInput){
  if(game){
   if(held&KEY_TOUCH){touchPosition p;hidTouchRead(&p);gameTouchX=p.px;gameTouchY=p.py;game_port_touch(game,down&KEY_TOUCH?GAME_TOUCH_DOWN:GAME_TOUCH_MOVE,gameTouchX,gameTouchY);}
   if(hidKeysUp()&KEY_TOUCH)game_port_touch(game,GAME_TOUCH_UP,gameTouchX,gameTouchY);
  }else{
   if(down&KEY_TOUCH){touchPosition p;hidTouchRead(&p);touch(p.px,p.py);}
   if(book.open&&book.bank&&book.zoom&&!modal.open&&(held&KEY_TOUCH)){touchPosition p;hidTouchRead(&p);if(p.py>=28&&p.py<205){SWLMatrix v=book_view(320,177,28,false);book.x=(p.px-v.x)/v.a;book.y=(p.py-v.y)/v.d;}}
   if(lobby_locked()&&!modal.open){if(down&KEY_B)lobby_close();}
   else if(igloo.editing&&!modal.open){editor_drag(held,hidKeysUp());if(down&KEY_X)editor_remove();if(down&KEY_Y)editor_turn(0,1);if(down&KEY_L)editor_turn(-1,0);if(down&KEY_R)editor_turn(1,0);if(down&KEY_A){editor_cancel_drag();editor.inventory=!editor.inventory;}if(down&KEY_B){if(editor.inventory)editor.inventory=false;else editor_save();}}
   else if(!modal.open&&!book.open){if(down&KEY_X)action(25);if(down&KEY_Y)action(26);if(down&KEY_B){if(demo.tab)demo.tab=0;else action(17);}if(down&KEY_A){if(rockhopper_near())rockhopper_open();else if(!room_npc_action())chat();}if(down&KEY_L)history_open();else if(down&KEY_R)chat();}else if(down&KEY_B){if(modal.open)modal.open=false;else if(book.open)book.close=true;}
   if(book.open&&!modal.open&&(down&(KEY_L|KEY_R)))book_toggle_zoom(true);
   if(down&KEY_SELECT){if(demo.tab==1&&pnet_state()==PNET_OFFLINE)mapList=!mapList;else demo.stats=!demo.stats;}
  }
  }
  PNetState networkState=pnet_state();if(networkState!=previousState){
   char status[160];pnet_status(status,sizeof status);message(status);
   if(networkState==PNET_ONLINE){mapList=false;pnet_send("i#gi",0,NULL);pnet_send("l#mst",0,NULL);pnet_send("n#gn",0,NULL);if(!game)lobbyUI.refresh=true;}
   else if(previousState==PNET_ONLINE){
    profile_epf_reset(&profile);epfQuery=false;
    if((game||gameQueued)&&(queuedGame.table||queuedGame.waddle)){
     gameMultiplayerLost=true;if(game)game_leave(NULL,queuedGame.returnRoom,queuedGame.returnX,queuedGame.returnY);else gameQueued=false;
    }
    lobby_disconnected();
   }
   if(networkState==PNET_RETRYING||networkState==PNET_SIGN_IN)demo.count=1;
   previousState=networkState;
  }
  char connectedSession[160];
  if(pnet_take_session(connectedSession,sizeof connectedSession)&&!account_select_session(connectedSession,NULL))pnet_stop();
  PNetAccountState account=pnet_account_state();
  if(account==PNET_ACCOUNT_OK||account==PNET_ACCOUNT_ERROR){
   int kind=0;char accountSession[160]="",accountName[64]="",accountError[160]="";
   account=pnet_account_take(&kind,accountSession,sizeof accountSession,accountName,sizeof accountName,accountError,sizeof accountError);
   if(account==PNET_ACCOUNT_ERROR){if(!game)modal_message(accountError[0]?accountError:"The account request failed.");}
   else if((kind==1||kind==3)&&accountSession[0]){
    if(account_select_session(accountSession,accountName)){
    pnet_stop();
    profile_init(&profile,false);memset(&social,0,sizeof social);memset(&igloo,0,sizeof igloo);inventoryUI.stamp=0;demo.count=1;previousState=PNET_OFFLINE;
    if(settings.network.api[0]&&settings.network.world[0])pnet_start(&settings.network,settings.session,"","",false);
    demo.tab=0;if(kind==3){char created[96];snprintf(created,sizeof created,"Welcome, %s!",accountName[0]?accountName:"penguin");message(created);}else message(accountName[0]?accountName:"Welcome back to your penguin.");
    }
   }else if(kind==5&&demo.tab==21){
    char url[96];uint8_t temporary[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
    snprintf(url,sizeof url,"https://gopenguin.lol/#device=%s",accountSession);
    if(qrcodegen_encodeText(url,temporary,deviceQr,qrcodegen_Ecc_MEDIUM,qrcodegen_VERSION_MIN,8,qrcodegen_Mask_AUTO,true)){
     snprintf(deviceCode,sizeof deviceCode,"%.12s",accountSession);deviceExpires=osGetTime()+600000;
    }else modal_message("Could not draw the device QR. Tap New code to try again.");
   }else if(kind==4){modal_message("Thank ye! Your message is in my logbook for the GoPenguin crew.");
   }else if(kind==2&&accountName[0]){
    bool remembered=settings_remember_name(&settings,accountName,SETTINGS_FILE);
    snprintf(demo.actors[0].name,sizeof demo.actors[0].name,"%.48s",accountName);
    demo.tab=0;char renamed[160];snprintf(renamed,sizeof renamed,remembered?"Your penguin is now %s.":"Name changed to %s. SD name cache could not be updated.",accountName);message(renamed);
   }
  }
  if(membershipPaid){membershipPaid=false;pnet_stop();if(settings.network.api[0]&&settings.network.world[0])pnet_start(&settings.network,settings.session,"","",false);demo.tab=0;message("Donation confirmed. Thank you for supporting GoPenguin!");}
  PNetMessage packet;unsigned updates=0;
  if(game)game_port_connection(game,networkState==PNET_ONLINE);
  while(updates++<64&&(pnet_receive(&packet)||local_receive(&packet))){
   if(!strcmp(packet.command,"js")||!strcmp(packet.command,"epfsa")){profile_epf_reset(&profile);epfQuery=true;}
   if(lobby_apply(&packet)){pnet_message_free(&packet);continue;}
   if(!strcmp(packet.command,"gpstaff")){staffCount=0;for(unsigned i=0;i<packet.count&&staffCount<64;i++){const char *colon=strchr(packet.args[i],':');unsigned id=(unsigned)strtoul(packet.args[i],NULL,10);if(colon&&id){staffIds[staffCount]=id;staffRole[staffCount++]=colon[1];}}pnet_message_free(&packet);continue;}
   if(!strcmp(packet.command,"gpmsg")&&packet.count>=3){
    if(!strcmp(packet.args[0],"update")){gp_update_check();pnet_message_free(&packet);continue;}
    bool kick=!strcmp(packet.args[0],"kick"),ban=!strcmp(packet.args[0],"ban"),moved=!strcmp(packet.args[0],"moved");char notice[1024];
    snprintf(notice,sizeof notice,"%s\n%s%s",packet.args[1],packet.args[2],kick?"\nYou can come back in one minute.":"");
    if(kick||ban||moved){kickStop=true;kickResume=kick?osGetTime()+60000:0;}
    modal_message(notice);pnet_message_free(&packet);continue;
   }
   if(game&&game_port_packet(game,packet.command,packet.count,packet.args)){
#ifdef GP_LAN_ONLY
    FILE *gameTrace=fopen(DATA_DIR "/test-state.log","a");if(gameTrace){fprintf(gameTrace,"game command=%s first=%lu\n",packet.command,packet.count?strtoul(packet.args[0],NULL,10):0);fclose(gameTrace);}
#endif
    pnet_message_free(&packet);continue;
   }
   bool iglooPacket=igloo_apply(&igloo,&packet);
   if(waitingIgloo&&iglooPacket&&!strcmp(packet.command,"gm")&&igloo.layout.owner==wantedIglooOwner){waitingIgloo=false;wantedIgloo=true;}
   if(!(iglooPacket&&!strcmp(packet.command,"go")))social_apply(&social,&packet);
   if(!strcmp(packet.command,"mr")){pnet_send("l#mst",0,NULL);pnet_send("l#mg",1,(const char *[]){"0"});}
   if(!strcmp(packet.command,"ms")&&packet.count>=2){modal_message(!strcmp(packet.args[1],"1")?"Your postcard has been sent.":!strcmp(packet.args[1],"2")?"You need more coins.":"This penguin mailbox is full.");if(!strcmp(packet.args[1],"1"))book.close=true;}
   if(packet.count&&social_ignored(&social,(unsigned)strtoul(packet.args[0],NULL,10))&&(!strcmp(packet.command,"sm")||!strcmp(packet.command,"se")||!strcmp(packet.command,"ss")||!strcmp(packet.command,"sj")||!strcmp(packet.command,"sl"))){pnet_message_free(&packet);continue;}
   if(!strcmp(packet.command,"ba"))pnet_send("b#gb",0,NULL);
   if(!strcmp(packet.command,"bf")&&social.foundRoom){const IslandRoom *room=island_room(social.foundRoom);if(room){char prompt[140];snprintf(prompt,sizeof prompt,"Your buddy is in %s. Go there?",room->title);modal_message(prompt);modal.confirm=true;modal.room=room->id;}else modal_message("Your buddy is in an igloo.");}
   unsigned room=world_apply(&demo,&packet,pnet_id());profile_apply(&profile,&packet,pnet_id());
   if(!strcmp(packet.command,"jr")&&room){
    bool chooser=(lobbyUI.open&&!lobby.needsLeave&&lobby.state!=LOBBY_LAUNCHED)||lobbyUI.routeRoom==room;
    lobby_stop();lobby_init(&lobby,game_send,NULL);lobbyUI.leavePending=false;
    lobbyUI.routeRoom=chooser?room:0;lobbyUI.cancelledWaddle=0;
    if(room==currentRoomId)lobby_room_enter(room);
   }
   if(!strcmp(packet.command,"jr")||!strcmp(packet.command,"ap")||!strcmp(packet.command,"rp")||!strcmp(packet.command,"upa"))lobbyUI.artDirty=true;
   if(packet.count){
    Actor *from=world_actor(&demo,(unsigned)strtoul(packet.args[0],NULL,10));
    if(from){
     bool spoken=!strcmp(packet.command,"sm")&&packet.count>=2;
     if(!strcmp(packet.command,"ss")&&packet.count>=2){SafeMessage safe;if(client_safe(clientData,(unsigned)strtoul(packet.args[1],NULL,10),&safe)){actor_say(from,safe.name);spoken=true;}}
     else if(!strcmp(packet.command,"sj")&&packet.count>=2){const char *joke=client_joke(clientData,(unsigned)strtoul(packet.args[1],NULL,10));if(*joke){actor_say(from,joke);spoken=true;}}
     else if(!strcmp(packet.command,"sl")){actor_say(from,client_tour(clientData,currentRoomId));spoken=true;}
     else if(!strcmp(packet.command,"sb")&&packet.count>=3&&from->id!=pnet_id())throw_to(from,(float)strtoul(packet.args[1],NULL,10),(float)strtoul(packet.args[2],NULL,10));
     if(spoken)chat_log_append(&chatLog,from->id,from->name,from->chat,currentRoomId,osGetTime());
    }
   }
   if(!strcmp(packet.command,"ai")&&packet.count){Product product;if(client_product(clientData,0,(unsigned)strtoul(packet.args[0],NULL,10),&product)){char added[180];snprintf(added,sizeof added,"%.120s\nhas been added to your inventory.",product.name);modal_message(added);}}
#ifdef GP_LAN_ONLY
   FILE *trace=loadTesting?NULL:fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"%s args=%u mine=%u first=%lu room=%u players=%u name=%s\n",packet.command,packet.count,pnet_id(),packet.count?strtoul(packet.args[0],NULL,10):0,room,demo.count,demo.actors[0].name);if(!strcmp(packet.command,"gm"))fprintf(trace,"igloo applied=%u dirty=%u pending=%u verified=%u failed=%u items=%u expected=%lu received=%lu\n",iglooPacket,igloo.dirty,igloo.awaitingSave,igloo.saveVerified,igloo.saveFailed,igloo.layout.count,(unsigned long)strlen(igloo.expected),(unsigned long)(packet.count>=5?strlen(packet.args[4]):0));fclose(trace);}
#endif
   features_packet(&packet,room);
   if(room&&room!=currentRoomId){serverRoom=true;if(room>2000)open_igloo(room-2000);else{wantedRoom=room;wantedX=demo.actors[0].x;wantedY=demo.actors[0].y;}}pnet_message_free(&packet);}
  GPUpdateView releaseUpdate=gp_update_view();
  if(networkState==PNET_ONLINE&&now>=updateCheckAt&&releaseUpdate.state!=GP_UPD_DOWNLOADING&&releaseUpdate.state!=GP_UPD_INSTALLED){gp_update_check();updateCheckAt=now+300000;}
  if(releaseUpdate.state==GP_UPD_AVAILABLE&&strcmp(offeredUpdate,releaseUpdate.version)&&!game&&!gameQueued&&!modal.open&&!book.open&&!chatKeyboard.active&&!chatHistory.open&&!igloo.editing){snprintf(offeredUpdate,sizeof offeredUpdate,"%s",releaseUpdate.version);modal_message("A new GoPenguin version is ready. Open Updates? Your penguin is kept.");modal.confirm=true;snprintf(modal.command,sizeof modal.command,"open-update");}
  // Removed by a moderator: stop reconnecting (a kick returns after one minute).
  if(kickStop){kickStop=false;pnet_stop();}
  if(kickResume&&osGetTime()>=kickResume){kickResume=0;if(settings.network.api[0]&&settings.network.world[0]&&settings.session[0])pnet_start(&settings.network,settings.session,"","",false);}
  rockhopper_sync();
  if(igloo.editing){if(igloo.saveVerified){igloo.saveVerified=false;igloo.editing=editor.inventory=false;editor.selected=-1;message("Your igloo has been saved.");}else if(igloo.saveFailed||(igloo.awaitingSave&&now-editor.saveAt>15000)){igloo.awaitingSave=igloo.saveFailed=false;message("The igloo could not be verified. Try Save again.");}}
  networkState=pnet_state();
  if(networkState==PNET_ONLINE&&demo.actors[0].name[0]&&strcmp(settings.name,demo.actors[0].name)){snprintf(settings.name,sizeof settings.name,"%s",demo.actors[0].name);settings_save(&settings,SETTINGS_FILE);}
  if(epfQuery&&networkState==PNET_ONLINE&&pnet_send("f#epfga",0,NULL))epfQuery=false;
  if(!game)epf_access_role(&epfAccess,profile.epfReady,profile.epfAgent,&epfHooks);
  if(networkState==PNET_ONLINE&&puffles.mine!=pnet_id()){puffles_init(&puffles,pets_send,NULL);petRoom=0;pets_enter(roomBank?igloo.layout.owner:0);}
  else if((networkState==PNET_OFFLINE||networkState==PNET_RETRYING||networkState==PNET_SIGN_IN)&&puffles.mine){puffles_init(&puffles,pets_send,NULL);petRoom=wantedPetOwner=0;}
  if(wantedPetOwner&&networkState==PNET_ONLINE&&!puffles.snapshotPending&&puffles_request(&puffles,wantedPetOwner))wantedPetOwner=0;
  puffles_step(&puffles,&demo,dt);if(*puffles.error){modal_message(puffles.error);puffles.error[0]=0;}
  if(igloo.awaitingStyle&&now-styles.started>15000){igloo.awaitingStyle=false;igloo.slotsReady=igloo.activeKnown=false;modal_message("The igloo change timed out. Refresh to check the server state before retrying.");}
  if(igloo.awaitingMusic&&now-musicStarted>15000){igloo.awaitingMusic=false;modal_message("The music change timed out. Reopen your igloo to check its saved music.");}
  if(*profile.error){if(game)message(profile.error);else modal_message(profile.error);profile.error[0]=0;}
  if(!game){if(profile.inventoryRevision!=roomInventoryStamp)room_inventory();room_hover(held);}gp_audio_update();lobby_update(dt);
  if(room_tube_locked(&roomTube)&&(lobby_locked()||game||gameQueued||book.open||modal.open||demo.tab!=0||wantedRoom||waitingIgloo||wantedIgloo||(pnet_id()&&networkState!=PNET_ONLINE)))room_tube_cancel(&roomTube,&tubeHooks);
  circlePosition pad;hidCircleRead(&pad);if(chatKeyboard.active||keyboardInput||chatHistory.open||historyInput){memset(&pad,0,sizeof pad);down=held=0;}float dx=pad.dx/150.0f,dy=-pad.dy/150.0f;if(held&KEY_DLEFT)dx=-1;if(held&KEY_DRIGHT)dx=1;if(held&KEY_DUP)dy=-1;if(held&KEY_DDOWN)dy=1;if(book.open&&book.zoom&&!modal.open){SWLMatrix view=book_view(400,240,0,true);const float stage[]={0,0,760,480},*b=book.resourceId&&book.boundsReady?book.bounds:stage;float halfX=fminf(200/view.a,(b[2]-b[0])/2),halfY=fminf(120/view.d,(b[3]-b[1])/2);book.x=fmaxf(b[0]+halfX,fminf(b[2]-halfX,book.x+dx*240*dt));book.y=fmaxf(b[1]+halfY,fminf(b[3]-halfY,book.y+dy*240*dt));}if(lobby_locked()||room_tube_locked(&roomTube)||game||gameQueued||modal.open||book.open||igloo.editing||waitingIgloo||wantedIgloo)dx=dy=0;demo_step(&demo,dt,dx,dy);if(!game)lobby_progress(dt);if(roomBank)for(unsigned i=0;i<igloo.layout.count;i++)furnitureTime[i]+=dt;overlay_step(&overlay,dt);if(noticeTime>0)noticeTime-=dt;
  if(game||gameQueued){dx=dy=0;demo.actors[0].walking=false;}
  Actor *me=&demo.actors[0];bool moving=me->walking||hypotf(dx,dy)>.15f;
  if(actionPosition){actionPosition=false;lastX=me->x;lastY=me->y;wasMoving=false;lastMove=now;}
  if(networkState==PNET_ONLINE&&!lobby_locked()&&((moving&&now-lastMove>=100)||(!moving&&wasMoving))&&(fabsf(me->x-lastX)>.5f||fabsf(me->y-lastY)>.5f)){char x[20],y[20];float tx=me->walking?me->tx:me->x,ty=me->walking?me->ty:me->y;snprintf(x,sizeof x,"%d",(int)lroundf(tx));snprintf(y,sizeof y,"%d",(int)lroundf(ty));pnet_send("u#sp",2,(const char *[]){x,y});lastMove=now;lastX=me->x;lastY=me->y;}wasMoving=moving&&!lobby_locked();
  triggerDelay=fmaxf(0,triggerDelay-dt);IslandTrigger trigger;bool atTrigger=navigation&&epf_access_trigger(&epfAccess,(int)lroundf(demo.actors[0].x),(int)lroundf(demo.actors[0].y),&trigger);if(!atTrigger)activeTrigger=~0u;
  bool approachingNpc=false;
#ifdef GP_NINJA_GAMES
  approachingNpc=room_npc_approaching(&roomNpc,me->x,me->y,me->walking,hypotf(dx,dy)>.15f||demo.tab!=0||game||gameQueued||book.open||modal.open);
#endif
  bool tubeTrigger=atTrigger&&room_tube_supported(currentRoomId)&&!strcmp(trigger.framePath,"tubecontrol_mc");
  unsigned tubeMine=pnet_id()?pnet_id():1;
  bool tubeArmed=false;
  if(!lobby_locked()&&!game&&!gameQueued&&!book.open&&!modal.open&&!igloo.editing&&demo.tab==0&&!wantedRoom){
   if(!tubeTrigger||!moving)tubeArmed=room_tube_observe(&roomTube,tubeMine,tubeMine,tubeTrigger,(unsigned)me->colour,&tubeHooks);
   if(tubeArmed){me->walking=false;me->tx=me->x;me->ty=me->y;}
  }
  if(!approachingNpc&&!lobby_locked()&&!game&&!gameQueued&&!modal.open&&!book.open&&!igloo.editing&&!triggerDelay&&atTrigger&&trigger.index!=activeTrigger){
   unsigned frame=tubeTrigger?room_tube_frame(&roomTube):*trigger.framePath?swl_frame(scene,trigger.framePath):0;bool allowed=(!*trigger.framePath||(frame>=trigger.minFrame&&frame<=trigger.maxFrame))&&(!trigger.item||profile_has(&profile,trigger.item));
   if(allowed&&trigger.room&&trigger.room!=island_id(navigation)){
    if(tubeTrigger){RoomTubeDestination destination;if(room_tube_take(&roomTube,&destination)){wantedRoom=destination.room;wantedX=destination.x;wantedY=destination.y;activeTrigger=trigger.index;}}
    else{wantedRoom=trigger.room;wantedX=trigger.x;wantedY=trigger.y;activeTrigger=trigger.index;}
   }else if(*trigger.otherwise&&((!trigger.room&&allowed)||(trigger.item&&!profile_has(&profile,trigger.item)))){content(trigger.otherwise);triggerDelay=2;activeTrigger=trigger.index;}
  }
  if(book.bank){char coins[48];snprintf(coins,sizeof coins,"YOUR COINS: %u",profile.coins);movie_bank_text(book.bank,".coinTotal.coins_txt",coins);book.clock+=dt;while(book.clock>=1.0f/swl_rate(movie_bank_scene(book.bank))){swl_tick(movie_bank_scene(book.bank));book.clock-=1.0f/swl_rate(movie_bank_scene(book.bank));}}
  if(styles.preparing||igloo.awaitingStyle){demo.actors[0].walking=false;demo.actors[0].tx=demo.actors[0].x;demo.actors[0].ty=demo.actors[0].y;}
  if(!game){bool holdTube=tubeArmed;sceneClock+=dt;while(sceneClock>=1/swl_rate(scene)){epf_access_tick(&epfAccess,&epfHooks);if(holdTube)holdTube=false;else room_tube_tick(&roomTube,&tubeHooks);sceneOK=swl_tick(scene)&&sceneOK;sceneClock-=1/swl_rate(scene);}interpolation=sceneClock*swl_rate(scene);swl_begin(scene);}
  if(!game){
  wear_begin();if(roomBank&&!book.open){if(igloo.layout.floor)wear_want_pose(asset_key(ASSET_FLOOR,igloo.layout.floor),iglooGeometry->floorFrame,1);for(unsigned i=0;i<igloo.layout.count;i++){IglooFurniture *f=&igloo.layout.items[i];wear_want_pose(asset_key(ASSET_FURNITURE,f->id),f->rotation,f->frame);}}if(igloo.editing&&editor.inventory){editor_inventory();for(unsigned n=editor.page*6;n<editor.count&&n<(editor.page+1)*6;n++)wear_want(asset_key(ASSET_FURNITURE_ICON,editor.items[n]));}if(!book.open&&demo.tab==7){Actor *a=selected_actor();if(a){if(a->look[7])wear_want(asset_key(ASSET_PHOTO,a->look[7]));for(unsigned i=0;i<6;i++)wear_want(asset_key(ASSET_PAPER,a->look[i]));if(a->look[6])wear_want(asset_key(ASSET_ICON,a->look[6]));}}if(!book.open&&demo.tab==2)inventory_prepare(&inventoryUI,clientData,&profile,&demo.actors[0]);for(unsigned i=0;!book.open&&i<demo.count;i++){const unsigned slots[]={5,3,2,4,1,0};for(unsigned slot=0;slot<6;slot++)wear_want(demo.actors[i].look[slots[slot]]);}
  if(!book.open)for(unsigned i=0;i<puffles.count;i++)if(puffles_visible(&puffles,&puffles.pets[i],&demo)||(demo.tab==13&&puffles.pets[i].owner==pnet_id()&&puffles.pets[i].id==petSelected))wear_want_pose(asset_key(ASSET_PUFFLE,puffles.pets[i].type+1),puffles.pets[i].frame,1);
  }
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);game_transition();
  if(chatGlyphs)C2D_TextBufClear(chatGlyphs);
  if(game){GameInput input=game_input(down,held,&pad);game_port_pause(game,gameSuspended||(queuedGame.online&&networkState!=PNET_ONLINE));game_port_update(game,dt,&input);}
  else{
   if(wantedRoom&&book.open)book.close=true;
   book_update();styles_update();
   if(wantedRoom){waitingIgloo=wantedIgloo=false;if(!enter_room(wantedRoom,wantedX,wantedY))room_tube_cancel(&roomTube,&tubeHooks);wantedRoom=0;sceneClock=0;}else if(wantedIgloo&&enter_pending_igloo())sceneClock=0;
   if(demo.tab==1&&!mapList){if((!islandMap.loaded&&!load(&islandMap,"map-top"))||(!islandMapLow.loaded&&!load(&islandMapLow,"map-bottom")))message("Could not load the island map.");}else{if(islandMap.loaded){C3D_TexDelete(&islandMap.tex);islandMap.loaded=false;}if(islandMapLow.loaded){C3D_TexDelete(&islandMapLow.tex);islandMapLow.loaded=false;}}
   if(!book.open)wear_update();
   floor_update();
  }
  swl_gpu_frame();u64 drawStart=osGetTime();C2D_TargetClear(topTarget,rgb(0x003366));C2D_SceneBegin(topTarget);top();C2D_TargetClear(bottomTarget,rgb(0xdaf2ff));C2D_SceneBegin(bottomTarget);bottom();C3D_FrameEnd(0);frameMS=osGetTime()-drawStart;
  if(!sceneOK){message("Scene rendering failed. Please rebuild the scene assets.");ok=false;}
  frames++;if(now-sample>=1000){
   fps=frames*1000.0f/(now-sample);frames=0;sample=now;
#ifdef GP_LAN_ONLY
   unsigned visible=0;for(unsigned i=0;i<puffles.count;i++)if(puffles_visible(&puffles,&puffles.pets[i],&demo))visible++;
   FILE *trace=fopen(DATA_DIR "/test-state.log","a");if(trace){fprintf(trace,"load players=%u puffles=%u fps=%.2f draw=%u linear=%u wear=%u\n",demo.count,visible,fps,(unsigned)frameMS,(unsigned)linearSpaceFree(),(unsigned)wear_bytes());fclose(trace);}
#endif
  }
 }
 done:;
 pnet_request_stop();asset_cache_request_stop();gp_update_request_stop();
 u64 exitBegin=now_ms(),exitPhase=exitBegin;
 FILE *exitTrace=fopen(DATA_DIR "/last-exit.log","w");
 if(exitTrace){fputs("begin client cleanup\n",exitTrace);fflush(exitTrace);}
 #define EXIT_PHASE(name) do{u64 stamp=now_ms();if(exitTrace){fprintf(exitTrace,"%s ms=%llu total_ms=%llu\n",name,(unsigned long long)(stamp-exitPhase),(unsigned long long)(stamp-exitBegin));fflush(exitTrace);}exitPhase=stamp;}while(0)
 aptUnhook(&audioHook);gp_audio_shutdown();EXIT_PHASE("audio");
 // Bounded: a worker stuck in a system call must not keep the console on "Closing software".
 bool stranded=!pnet_stop_within(1500);EXIT_PHASE(stranded?"network-stranded":"network");
 if(!gp_update_stop_within(1500)){stranded=true;EXIT_PHASE("updater-stranded");}else EXIT_PHASE("updater");
 if(!asset_cache_stop_within(1500)){stranded=true;EXIT_PHASE("downloads-stranded");}else EXIT_PHASE("downloads");
 // No C3D_FrameSync here: after HOME > Close the app no longer owns the GPU and VBlank events
 // stop arriving, so a frame sync can wait forever. The last frame's commands finish well within this.
 svcSleepThread(50*1000000LL);EXIT_PHASE("gpu-idle");
 game_port_free(game);movie_bank_free(book.bank);wear_free();if(roomBank)movie_bank_free(roomBank);else swl_free(scene);igloo_geometry_free(iglooGeometry);island_free(navigation);Texture *all[]={&icons,&emotes,&islandMap,&islandMapLow,&fontTexture,&paperBody,&paperDetail,&minimap,&bookCovers};for(unsigned i=0;i<sizeof all/sizeof all[0];i++)if(all[i]->loaded)C3D_TexDelete(&all[i]->tex);for(int i=0;i<PENGUIN_PAGES;i++){if(body[i].loaded)C3D_TexDelete(&body[i].tex);if(detail[i].loaded)C3D_TexDelete(&detail[i].tex);}for(unsigned i=0;i<SWL_MAX_PAGES;i++)if(sceneAtlas[i].loaded)C3D_TexDelete(&sceneAtlas[i].tex);
 client_data_free(clientData);if(shellText)json_decref(shellText);EXIT_PHASE("assets");
 chat_keyboard_cancel(&chatKeyboard);if(chatGlyphs)C2D_TextBufDelete(chatGlyphs);if(chatMeasure)C2D_TextBufDelete(chatMeasure);
 swl_gpu_free();C2D_Fini();C3D_Fini();EXIT_PHASE("gpu");
 romfsExit();gfxExit();EXIT_PHASE("client-cleanup-complete");
 if(exitTrace)fclose(exitTrace);
 #undef EXIT_PHASE
 // A stranded worker still holds system sessions; end the process (and every thread) directly.
 if(stranded)svcExitProcess();
 return ok?0:1;
}
