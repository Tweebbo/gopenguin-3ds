#pragma once
#include <stdbool.h>
/* Card-Jitsu deck shown when the Starter Deck (award 821) is touched in the inventory.
 * The original client opened awards/821.swf ("Loading Starter Deck") with the player's cards. */
#define DECK_VIEW_ITEM 821u
#define DECK_VIEW_LIMIT 600u
#define DECK_VIEW_ROWS 5u
typedef struct {unsigned id,count,value,power;char element,colour;const char *name;} DeckCard;
typedef struct {DeckCard cards[DECK_VIEW_LIMIT];unsigned kinds,total,fire,water,snow,page;bool known,pending;} DeckView;
void deck_view_reset(DeckView *view);
/* Parses the server's "gcd" reply: one argument of "id,count|id,count|...". Unknown ids are kept
 * with a generic name. Returns false and leaves the view unchanged on malformed input. */
bool deck_view_parse(DeckView *view,unsigned count,char *const *args);
unsigned deck_view_pages(const DeckView *view);
const char *deck_view_element(char element);
const char *deck_view_colour(char colour);
