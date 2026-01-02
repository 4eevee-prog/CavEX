#pragma once
#include <stdbool.h>

bool audio_init(void);
void audio_update(void);
void audio_shutdown(void);

typedef enum {
  SFX_GUI_CLICK = 0,
  SFX_PLACE,
  SFX_BREAK,
  SFX_JUMP,
  SFX_HIT,
  SFX_ITEM_SWITCH,
  SFX_COUNT
} sfx_id;

void audio_play_sfx(sfx_id id);
void audio_play_ambient(const char* wav_path, int volume_0_255); // loop
void audio_stop_ambient(void);