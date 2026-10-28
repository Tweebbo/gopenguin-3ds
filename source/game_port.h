#pragma once
// Native minigame adapter. main.c owns one GamePort while a game is open.
// The port, its controllers and models are independent of libctru so they run in
// host tests; only game_gpu.c (bank textures and top-screen drawing) needs citro3d.
// Every host callback is optional except send/leave for online games.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "swl.h"
#include "movie_meta.h"

#define GAME_PORT_RATE 24u
#define GAME_PORT_BANKS 3
#define GAME_PORT_TEXTURE_LIMIT (8u*1024u*1024u)

/** Physical controls, mapped by the host from hidKeys*(). */
enum {
 GAME_KEY_A=1u<<0,GAME_KEY_B=1u<<1,GAME_KEY_X=1u<<2,GAME_KEY_Y=1u<<3,GAME_KEY_L=1u<<4,GAME_KEY_R=1u<<5,
 GAME_KEY_START=1u<<6,GAME_KEY_SELECT=1u<<7,GAME_KEY_LEFT=1u<<8,GAME_KEY_RIGHT=1u<<9,GAME_KEY_UP=1u<<10,GAME_KEY_DOWN=1u<<11,
};
typedef struct {uint32_t down,held;float stickX,stickY;} GameInput; // stick in -1..1, +y up
typedef enum {GAME_TOUCH_DOWN,GAME_TOUCH_MOVE,GAME_TOUCH_UP} GameTouch;
typedef enum {GAME_RESOURCE_READY,GAME_RESOURCE_PENDING,GAME_RESOURCE_FAILED} GameResource;
typedef enum {GAME_ALIGN_LEFT,GAME_ALIGN_CENTER,GAME_ALIGN_RIGHT} GameAlign;

/** Immutable snapshot captured by the host when the player enters the game. */
typedef struct {
 char key[16];          // web game key: hydro, roundup, beans, cart, thinice, fish, four, mancala, sled
 unsigned returnRoom;   // room (or owner+2000 igloo) to rejoin when the game closes
 int returnX,returnY;
 unsigned player;       // own penguin id
 unsigned colour;       // penguin colour id
 uint32_t colourRGB;    // 0xRRGGBB of that colour
 unsigned hand;         // equipped hand item
 bool online;           // false: offline preview, no packets and no payout
 // Room tables (Find Four, Mancala): the host handled gt/jt and the seating; the
 // port starts with z#gz. seat is jt's seat id (1-based, 99 = watching).
 unsigned table;int seat;
 // Waddles (Sled Racing): the host handled jw and received jg; the port is already in the game.
 unsigned waddle;
 char name[40];         // own penguin name (Sled Racing finds its racer by it)
} GameDescriptor;

typedef struct {
 void *user;
 /** Lower-screen notices; the game keeps its own retry controls visible. */
 void (*notice)(void *user,const char *text);
 /** A server-confirmed coin total (never a local estimate). */
 void (*coins)(void *user,unsigned total);
 /** Resolve a bank name such as "games/hydro.gpk" to a readable local path (RomFS or verified cache). */
 GameResource (*resource)(void *user,const char *name,char *path,size_t cap);
 /** XT packet to the Waddle world, e.g. extension 'z', command "zo". Returns false if it was not queued. */
 bool (*send)(void *user,char extension,const char *command,unsigned count,const char *const *args);
 /** The game finished (payout settled or offline): join this room, then call game_port_free().
  *  For a table game (descriptor.table != 0) the player stays in the room: send s#lt and walk to
  *  the table's done point instead of joining. */
 void (*leave)(void *user,unsigned room,int x,int y);
 /** Original game sounds by export name; volume 0..100; loop restarts until stopped. */
 void (*sound)(void *user,const char *game,const char *name,unsigned volume,bool loop);
 void (*stop_sounds)(void *user,const char *game);
 /** Lower-screen drawing in 320x240 coordinates with the client's native font and buttons. */
 void (*text)(void *user,float x,float y,float size,uint32_t rgb,GameAlign align,const char *text);
 void (*rect)(void *user,float x,float y,float w,float h,uint32_t rgb);
 void (*button)(void *user,float x,float y,float w,float h,const char *label,bool pressed,bool enabled);
} GameHost;

/** Immediate-mode drawing the game requests between bank layers (device: citro2d; tests: recorded). */
typedef struct {
 void *user;
 void (*line)(void *user,float x0,float y0,float x1,float y1,float width,uint32_t rgb);
} GameCanvas;

typedef struct GamePort GamePort;

/** Recognised web game keys, in the order of the web's game list. */
bool game_port_supported(const char *key);
/** NULL for an unsupported key or out of memory. The descriptor and host are copied. */
GamePort *game_port_open(const GameDescriptor *descriptor,const GameHost *host);
bool game_port_active(const GamePort *port);
/** Fixed 24 Hz logic: elapsed real seconds are accumulated and consumed in ticks. */
void game_port_update(GamePort *port,float seconds,const GameInput *input);
void game_port_touch(GamePort *port,GameTouch phase,float x,float y);
/** Offer a world packet; true when the game consumed it (jg, zo and the game's own commands). */
bool game_port_packet(GamePort *port,const char *command,unsigned count,char *const *args);
void game_port_pause(GamePort *port,bool paused);
/**
 * World connection transitions. false when the socket/session drops; true once the
 * session is authenticated again. While a port is active the host must not join a
 * room on reconnection: the port rejoins its game room (j#jr/jg) itself, holds the
 * payout until then, and never resends a zo whose reply was lost.
 */
void game_port_connection(GamePort *port,bool online);
/** Close request (Start, HOME return, host navigation): goes through results and the single payout. */
void game_port_close(GamePort *port);
void game_port_free(GamePort *port);
/** Lower screen, through the host's text/rect/button callbacks. */
void game_port_draw_bottom(GamePort *port);
/** Fraction of the next logic tick, for drawing between ticks. */
float game_port_interpolation(const GamePort *port);
/** Texture bytes resident for this game, for the budget check and statistics. */
size_t game_port_texture_bytes(const GamePort *port);

// Bank access for controllers and the drawing layer. Banks are loaded by the
// platform layer (game_gpu.c on the console, a scene-only loader in host tests).
typedef struct GameBank GameBank;
GameBank *game_bank_load(const char *path,size_t textureLimit,const char **error);
void game_bank_free(GameBank *bank);
SWLScene *game_bank_scene(GameBank *bank);
MovieMeta *game_bank_meta(GameBank *bank);
size_t game_bank_texture_bytes(const GameBank *bank);
bool game_bank_text(GameBank *bank,const char *path,const char *value);

/** Title screens: covers the lower screen with the stage rectangle x,y,w,h of the original title art. */
SWLMatrix game_port_art_view(float x,float y,float w,float h);
/** Title screens: the button band over that art, from y=GAME_TITLE_BAND to the bottom. */
void game_port_title_band(GamePort *port);
#define GAME_TITLE_BAND 190

#ifdef __3DS__
#include <citro3d.h>
/** Top screen: draws the active banks with the game's overlay callbacks. */
bool game_port_draw_top(GamePort *port,C3D_RenderTarget *target);
/** Lower screen, dual-screen games only: draws the scene through the controller's lower_view
 *  before game_port_draw_bottom adds the HUD. Returns false when the game has no lower scene. */
bool game_port_draw_lower(GamePort *port,C3D_RenderTarget *target);
#endif
