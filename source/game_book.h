#pragma once
#include "game_book_data.h"
#include <stdbool.h>
const BookData *game_book_data(void *game);
unsigned game_book_page(void *game);
unsigned game_book_revealed(void *game);
unsigned game_book_errors(void *game);
/** Types one letter on the current page; returns whether it was the expected letter. */
bool game_book_type(void *game,char c);
