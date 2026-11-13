#pragma once
#include "movie_meta.h"
#include "island.h"
typedef struct {IslandNav *navigation;unsigned floorFrame;unsigned char *floor,*wall,*trash;} IglooGeometry;
IglooGeometry *igloo_geometry_load(const MovieMeta *meta,const char **error);
void igloo_geometry_free(IglooGeometry *geometry);
bool igloo_geometry_legal(const IglooGeometry *geometry,unsigned type,int x,int y);
bool igloo_geometry_trash(const IglooGeometry *geometry,int x,int y);
/** Nearest legal point for this furniture type within radius (spiral search). */
bool igloo_geometry_nearest(const IglooGeometry *geometry,unsigned type,int x,int y,int radius,int *outX,int *outY);
/** Legal point closest to the middle of the floor (or wall, type 2) area. */
bool igloo_geometry_center(const IglooGeometry *geometry,unsigned type,int *outX,int *outY);
/** Spawn point for a new piece: legal, near (anchorX, anchorY), avoiding the occupied points
 *  of pieces already placed. occupied holds count x,y pairs. */
bool igloo_geometry_spawn(const IglooGeometry *geometry,unsigned type,int anchorX,int anchorY,const int *occupied,unsigned count,int *outX,int *outY);
