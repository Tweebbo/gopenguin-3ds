#pragma once
#include "update_manifest.h"
typedef enum {GP_UPD_IDLE,GP_UPD_CHECKING,GP_UPD_CURRENT,GP_UPD_AVAILABLE,GP_UPD_DOWNLOADING,GP_UPD_VERIFYING,GP_UPD_RECONSTRUCTING,GP_UPD_INSTALLING,GP_UPD_FINALIZING,GP_UPD_INSTALLED,GP_UPD_ERROR,GP_UPD_RESOURCES,GP_UPD_READY} GPUpdateState;
typedef struct {GPUpdateState state;char version[24],message[200];unsigned done,total,elapsed,downloadBytes,fullBytes;bool small,fullFallback,resourceFullFallback,resourcesOnly,background,paused;} GPUpdateView;
void gp_update_init(const char *launch);
const char *gp_update_current(void);
bool gp_update_check(void);
bool gp_update_start(void);
bool gp_update_prepare(void);
bool gp_update_use_full(bool full);
void gp_update_pause(bool paused);
void gp_update_activity(unsigned frame_ms,bool loading,bool suspended,bool saver,bool new_model);
bool gp_update_blocks_game(GPUpdateView view);
bool gp_update_repair_resources(void);
unsigned gp_update_resource_bytes(void);
GPUpdateView gp_update_view(void);
bool gp_update_busy(GPUpdateState state);
void gp_update_request_stop(void);
bool gp_update_stop_within(unsigned milliseconds);
