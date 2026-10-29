#pragma once
#include <stdbool.h>
#include <stddef.h>
#define GP_RELEASE_TITLE_ID "0004000007E90200"
#define GP_RELEASE_VERSION "1.0.0"
#define GP_UPDATE_MAX_BYTES (512u*1024u*1024u)
typedef struct {char url[512],sha[65];unsigned bytes,titleVersion;bool checkTitleVersion;} GPUpdateArtifact;
typedef struct {char version[24],notes[400];GPUpdateArtifact cia,homebrew;} GPUpdateManifest;
bool gp_version_valid(const char *value);
int gp_version_compare(const char *a,const char *b);
bool gp_update_manifest(const char *json,size_t bytes,GPUpdateManifest *out);
bool gp_update_file(const char *path,const GPUpdateArtifact *artifact,bool homebrew);
bool gp_update_file_cancel(const char *path,const GPUpdateArtifact *artifact,bool homebrew,bool (*cancel)(void *),void *user);
bool gp_update_file_progress(const char *path,const GPUpdateArtifact *artifact,bool homebrew,bool (*cancel)(void *),void (*progress)(unsigned,void *),void *user);
