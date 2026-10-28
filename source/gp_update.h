#pragma once
#include "update_manifest.h"
typedef enum {GP_UPD_IDLE,GP_UPD_CHECKING,GP_UPD_CURRENT,GP_UPD_AVAILABLE,GP_UPD_DOWNLOADING,GP_UPD_INSTALLED,GP_UPD_ERROR} GPUpdateState;
typedef struct {GPUpdateState state;char version[24],message[200];unsigned done,total;} GPUpdateView;
void gp_update_init(const char *launch);
const char *gp_update_current(void);
bool gp_update_check(void);
bool gp_update_start(void);
GPUpdateView gp_update_view(void);
void gp_update_request_stop(void);
bool gp_update_stop_within(unsigned milliseconds);
