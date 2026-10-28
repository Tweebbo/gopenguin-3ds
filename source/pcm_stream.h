#pragma once
#include "gp_audio.h"

typedef struct GPPCM GPPCM;
/* WAV or independently compressed8KiB PCM blocks; no whole-track allocation. */
GPPCM *gp_pcm_open(const char *path,GPAudioWav *info);
size_t gp_pcm_read(GPPCM *stream,int16_t *samples,size_t count);
bool gp_pcm_rewind(GPPCM *stream);
void gp_pcm_close(GPPCM *stream);
