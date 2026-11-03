#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "incremental.h"
#define GP_RELEASE_TITLE_ID "0004000007E90200"
#define GP_RELEASE_VERSION "1.0.0"
#define GP_UPDATE_MAX_BYTES (512u*1024u*1024u)
typedef struct {char url[512],sha[65];unsigned bytes,titleVersion;bool checkTitleVersion;} GPUpdateArtifact;
typedef struct {bool present;GPUpdateArtifact index,bundle;struct {char from[24];GPUpdateArtifact bundle;} deltas[4];unsigned count;} GPResourceOffer;
typedef struct {char version[24],notes[400];GPUpdateArtifact cia,homebrew;IncrementalOffer smallCia,smallHomebrew;GPResourceOffer resources;} GPUpdateManifest;
bool gp_resource_offer(const char *json,size_t bytes,GPResourceOffer *out);
bool gp_version_valid(const char *value);
int gp_version_compare(const char *a,const char *b);
bool gp_update_manifest(const char *json,size_t bytes,GPUpdateManifest *out);
bool gp_update_manifest_for_version(const char *json,size_t bytes,const char *current,GPUpdateManifest *out);
bool gp_update_file(const char *path,const GPUpdateArtifact *artifact,bool homebrew);
bool gp_update_file_cancel(const char *path,const GPUpdateArtifact *artifact,bool homebrew,bool (*cancel)(void *),void *user);
bool gp_update_file_progress(const char *path,const GPUpdateArtifact *artifact,bool homebrew,bool (*cancel)(void *),void (*progress)(unsigned,void *),void *user);
