#pragma once
// Interface between game_port.c and the per-game controllers (game_hydro.c, ...).
#include "game_port.h"

typedef enum {GAME_SOLO,GAME_TABLE,GAME_WADDLE,GAME_CARD} GameKind;
typedef struct {
 const char *key;
 GameKind kind;          // solo games join their room and pay with zo; tables and waddles are server-run
 unsigned room;          // Waddle game room joined with j#jr (903 for Hydro)
 unsigned rate;          // logic ticks per second (the movie's frame rate); 0 means 24
 unsigned stageWidth,stageHeight; // movie stage, fitted to the top screen; 0 means 760x480
 uint32_t backgroundRGB; // authored background for custom table compositions; 0 uses the host clear
 const char *banks[GAME_PORT_BANKS]; // bank names resolved through GameHost.resource; NULL if unused
 /** Banks (bit per index) loaded on demand with game_port_require_bank instead of at open. */
 unsigned lazy;
#ifdef GP_NINJA_GAMES
 /** Resource for a lazy debug bank whose content follows the selected card. */
 const char *(*bank_resource)(void *game,unsigned bank);
#endif
 /** Top-screen drawing order of the banks (indices into banks). */
 unsigned char order[GAME_PORT_BANKS];
 void *(*create)(GamePort *port);
 void (*destroy)(void *game);
 /** Called once all banks are loaded. */
 bool (*ready)(void *game);
 void (*frame)(void *game,float seconds,const GameInput *input);
 void (*tick)(void *game);
 void (*touch)(void *game,GameTouch phase,float x,float y);
 bool (*packet)(void *game,const char *command,unsigned count,char *const *args);
 /** Close requested by the host (Start or navigation). */
 void (*close)(void *game);
 void (*draw_bottom)(void *game);
 /** Root-level depth callback while a bank is drawn; FLT_MAX after its last entry. */
 void (*overlay)(void *game,unsigned bank,float depth,const GameCanvas *canvas);
 /** Placement of a bank on the stage (bank 0 is normally identity); false hides it. */
 bool (*bank_view)(void *game,unsigned bank,SWLMatrix *local);
 /** Custom upper framing when the original is a portrait table widget. */
 bool (*top_view)(void *game,SWLMatrix *view);
 /** Dual screen: stage-to-lower-screen matrix when the game also draws its scene on the
  *  touch screen (under draw_bottom's HUD); false keeps the lower screen to draw_bottom. */
 bool (*lower_view)(void *game,SWLMatrix *view);
 /** Optional composition frame for each lower-screen bank; 0 hides that bank. */
 unsigned (*lower_frame)(void *game,unsigned bank);
} GameController;

typedef enum {GAME_SESSION_LOADING,GAME_SESSION_JOINING,GAME_SESSION_PLAYING,GAME_SESSION_PAYING,GAME_SESSION_LEFT,GAME_SESSION_FAILED} GameSessionState;

const GameDescriptor *game_port_descriptor(const GamePort *port);
const GameHost *game_port_host(const GamePort *port);
GameBank *game_port_bank(GamePort *port,unsigned index);
/** Load a lazy bank now (PENDING while the host downloads it; FAILED shows a retry). */
GameResource game_port_require_bank(GamePort *port,unsigned index);
/** Free a bank's scene and textures when its screens are over. */
void game_port_release_bank(GamePort *port,unsigned index);
GameSessionState game_port_session(const GamePort *port);
bool game_port_connected(const GamePort *port);
/** Whole seconds since the port opened, for timeouts. */
float game_port_clock(const GamePort *port);
/** Set while a request is outstanding or failed; shown with a retry button. */
const char *game_port_status(const GamePort *port);
bool game_port_can_retry(const GamePort *port);
void game_port_retry(GamePort *port);
/**
 * Send the final score once and return to the captured room after the server's
 * zo reply. Calling it again never sends a second zo: the payout stays pending
 * until confirmed, and a lost reply can only be abandoned with game_port_leave.
 */
void game_port_finish(GamePort *port,unsigned score);

bool game_port_paid(const GamePort *port);
/** Draw this tick without interpolation (a reused instance path would interpolate from another object). */
void game_port_snap(GamePort *port);
/** Platform drawing helpers. */
const GameController *game_port_controller(const GamePort *port);
void *game_port_game(GamePort *port);
bool game_port_drawable(const GamePort *port);
void game_port_stage(const GamePort *port,float *width,float *height);
/** A game packet to the world (tables and waddles); false offline or when it was not queued. */
bool game_port_send(GamePort *port,char extension,const char *command,unsigned count,const char *const *args);
/** Leave without (or after) payout. */
void game_port_leave(GamePort *port);
void game_port_sound(GamePort *port,const char *name,unsigned volume,bool loop);
void game_port_stop_sounds(GamePort *port);
/** Lower-screen helpers: a button rectangle with hit testing kept in the port. */
bool game_port_button(GamePort *port,unsigned id,float x,float y,float w,float h,const char *label,bool enabled);
/** A held movement pad: controller touch/held events own its input. */
void game_port_pad(GamePort *port,float x,float y,float w,float h,const char *label,bool pressed);
/** The id of the button released this frame under the stylus, or 0. */
unsigned game_port_clicked(GamePort *port);
void game_port_text(GamePort *port,float x,float y,float size,uint32_t rgb,GameAlign align,const char *text);
void game_port_rect(GamePort *port,float x,float y,float w,float h,uint32_t rgb);
