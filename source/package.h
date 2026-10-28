#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#define GP_PACKAGE_MAX (24u*1024u*1024u)
typedef struct {char path[256];unsigned count,offset[10],bytes[10];} GPPackage;
bool package_open(GPPackage *package,const char *path);
FILE *package_part(const GPPackage *package,unsigned index);
void *package_read(const GPPackage *package,unsigned index,size_t limit,size_t *bytes);
