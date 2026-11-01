#pragma once
#include "garment.h"
#include "sprite.h"
#include "wear_texture.h"

typedef struct {unsigned id,rotation,variation;} WearLoadRequest;
enum {WEAR_LOAD_FAILED,WEAR_LOAD_WAITING,WEAR_LOAD_TEXTURE,WEAR_LOAD_MOVIE};
typedef struct {
 WearLoadRequest request;
 unsigned status,page;
 char path[256];
 Garment garment;
 Sprite *sprite;
 WearTexture texture;
} WearLoadResult;

bool wear_loader_start(void);
bool wear_loader_busy(void);
/** 1 for the optional system-core test, -2 for default affinity, -3 when stopped. */
int wear_loader_core(void);
bool wear_loader_request(WearLoadRequest request);
bool wear_loader_take(WearLoadResult *result);
void wear_loader_cancel(void);
void wear_loader_request_stop(void);
bool wear_loader_stop_within(unsigned ms);
void wear_loader_prepare(WearLoadResult *result,WearLoadRequest request,bool (*cancel)(void *),void *user);
void wear_loader_result_free(WearLoadResult *result);
