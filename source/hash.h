#pragma once
#include <stdbool.h>
#include <stdio.h>
bool hash_file(FILE *file,const unsigned char expected[32]);
bool hash_file_cancel(FILE *file,const unsigned char expected[32],bool (*cancel)(void *),void *user);
