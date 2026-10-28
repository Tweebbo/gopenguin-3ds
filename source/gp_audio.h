#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define GP_AUDIO_RATE 22050u
#define GP_AUDIO_SFX_BYTES (128u*1024u)
typedef struct {uint32_t offset,bytes,rate;} GPAudioWav;

/* Exported audio is mono PCM16 at 22,050 Hz. The reader checks RIFF chunk bounds. */
bool gp_audio_wav_info(FILE *file,GPAudioWav *out);
bool gp_audio_init(const char *directory,bool enabled);
bool gp_audio_available(void);
bool gp_audio_enabled(void);
void gp_audio_set_enabled(bool enabled);
void gp_audio_pause(bool paused);
void gp_audio_music(unsigned id);
void gp_audio_room(const char *key);
void gp_audio_game(const char *key);
unsigned gp_audio_igloo_tracks(void);
/* Names remain valid until the next gp_audio_init() or gp_audio_shutdown(). */
bool gp_audio_igloo_track(unsigned index,unsigned *id,const char **name);
/* The bank name matches export-audio --scene BANK=SWF_PACK. Switching stops SFX. */
void gp_audio_scene(const char *bank);
/* Direct SWLSound callback. flags: bit 0 stop, bit 1 noMultiple. */
void gp_audio_sound(void *user,const char *path,unsigned id,unsigned flags,unsigned loops);
/* Resolves original SWF exports; volume is clamped to 0..100, loop lasts until stopped. */
bool gp_audio_named_sound(const char *bank,const char *name,int volume,bool loop);
void gp_audio_update(void);
void gp_audio_stop(void);
void gp_audio_shutdown(void);
