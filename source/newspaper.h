#pragma once
#include <stdbool.h>

#define NEWSPAPER_PAGES 10
#define NEWSPAPER_FRAMES 85
#define NEWSPAPER_RESOURCE_BASE 600000

typedef struct {bool open;unsigned page,frame;} Newspaper;
void newspaper_open(Newspaper *paper);
void newspaper_close(Newspaper *paper);
bool newspaper_goto(Newspaper *paper,unsigned frame);
bool newspaper_page(Newspaper *paper,unsigned page);
unsigned newspaper_resource(const Newspaper *paper);
unsigned newspaper_page_first(unsigned page);
unsigned newspaper_page_last(unsigned page);
const char *newspaper_page_label(unsigned page);
const char *newspaper_notice(unsigned originalControl);
