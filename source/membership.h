#pragma once
#include "penguin_net.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {MEMBERSHIP_IDLE,MEMBERSHIP_LOADING,MEMBERSHIP_READY,MEMBERSHIP_PAID,MEMBERSHIP_ERROR} MembershipState;

/** Create a short-lived link for the current session; the phone/PC opens it and pays. */
void membership_link_open(const PNetConfig *config,const char *session);
/** Start a status poll if the link is ready and four seconds have passed. */
void membership_tick(u64 now);
void membership_close(void);
MembershipState membership_state(void);
const char *membership_message(void);
const char *membership_url(void);
int membership_price_cents(void);
bool membership_configured(void);
bool membership_member(void);
const char *membership_items(void);
unsigned membership_coins(void);
/** QR modules per side (0 when there is no link yet) and whether a module is dark. */
int membership_qr_size(void);
bool membership_qr_dark(int x,int y);
/** True once after a payment was confirmed; the caller reconnects to refresh the penguin. */
bool membership_take_paid(void);
