#pragma once
#include "common.h"
typedef enum {PNET_OFFLINE,PNET_CONNECTING,PNET_AUTHENTICATING,PNET_JOINING,PNET_ONLINE,PNET_RETRYING,PNET_SIGN_IN} PNetState;
typedef struct {char api[160],world[160];unsigned room;int x,y;} PNetConfig;
typedef struct {char command[20];unsigned count;char **args,*small[16],*storage;} PNetMessage;
bool pnet_start(const PNetConfig *config,const char *session,const char *name,const char *password,bool create);
/** Starts online play with a persistent guest; reconnects reuse the returned session. */
bool pnet_start_guest(const PNetConfig *config);
void pnet_stop(void);
/** Exit path: waits at most `ms` for the worker; false leaves it running (the process is about to end). */
bool pnet_stop_within(unsigned ms);
void pnet_request_stop(void);
PNetState pnet_state(void);
void pnet_status(char *out,size_t cap);
unsigned pnet_id(void);
bool pnet_take_session(char *out,size_t cap);
bool pnet_send(const char *command,unsigned count,const char *const *args);
bool pnet_send_to(char extension,const char *command,unsigned count,const char *const *args);
bool pnet_receive(PNetMessage *message);
void pnet_message_free(PNetMessage *message);
bool pnet_join(unsigned room,int x,int y);
/* Account requests (B4) run off the render thread: start one, then poll and take the result. */
typedef enum {PNET_ACCOUNT_IDLE,PNET_ACCOUNT_BUSY,PNET_ACCOUNT_OK,PNET_ACCOUNT_ERROR} PNetAccountState;
/** Exchange a TWEEBBO- access code (or a GoPenguin session) for a usable session. */
bool pnet_account_recover(const PNetConfig *config,const char *code);
/** Create a new penguin without a password, like the web guest flow. */
bool pnet_account_guest(const PNetConfig *config);
/** Rename the penguin behind `session`; identity, coins and inventory stay. */
/** Captain Rockhopper's logbook; the result arrives through pnet_account_take with kind 4. */
bool pnet_suggest(const PNetConfig *config,const char *session,const char *category,const char *message,const char *request_id);
bool pnet_account_rename(const PNetConfig *config,const char *session,const char *name);
PNetAccountState pnet_account_state(void);
/** Consume OK/ERROR and return to IDLE; `kind` is 1 for recover and 2 for rename. */
PNetAccountState pnet_account_take(int *kind,char *session,size_t session_cap,char *name,size_t name_cap,char *error,size_t error_cap);
/* The active game sends its room join after authentication, including reconnects. */
void pnet_manual_room(bool manual);
bool pnet_parse(char *text,PNetMessage *message);
bool pnet_packet(char *out,size_t cap,const char *command,unsigned count,const char *const *args);
bool pnet_packet_to(char *out,size_t cap,char extension,const char *command,unsigned count,const char *const *args);
