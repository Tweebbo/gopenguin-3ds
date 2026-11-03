#pragma once
#include <stdbool.h>

#define DJ3K_MAX_SOUNDS 16
enum {DJ3K_DRUM,DJ3K_BASS,DJ3K_MELODY};

typedef struct {
 int drum,bass;
 bool melody[5];
 bool key[13];
 unsigned loopRevision;
 const char *sounds[DJ3K_MAX_SOUNDS];
 unsigned soundCount;
} Dj3kModel;

void dj3k_model_init(Dj3kModel *model);
void dj3k_toggle_loop(Dj3kModel *model,int group,int index);
void dj3k_effect(Dj3kModel *model,int index);
void dj3k_speaker(Dj3kModel *model,int index);
void dj3k_note(Dj3kModel *model,int index);
const char *dj3k_drum_name(int index);
const char *dj3k_bass_name(int index);
const char *dj3k_melody_name(int index);
const char *dj3k_effect_name(int index);
const char *dj3k_speaker_name(int index);
const char *dj3k_note_name(int index);
