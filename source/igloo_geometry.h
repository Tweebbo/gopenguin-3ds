#pragma once
#include "movie_meta.h"
#include "island.h"
typedef struct {IslandNav *navigation;unsigned floorFrame;unsigned char *floor,*wall,*trash;} IglooGeometry;
IglooGeometry *igloo_geometry_load(const MovieMeta *meta,const char **error);
void igloo_geometry_free(IglooGeometry *geometry);
bool igloo_geometry_legal(const IglooGeometry *geometry,unsigned type,int x,int y);
bool igloo_geometry_trash(const IglooGeometry *geometry,int x,int y);
