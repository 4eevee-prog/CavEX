#include "audio.h"

#ifdef PLATFORM_WII
bool audio_init(void);
void audio_update(void);
void audio_shutdown(void);
void audio_play_sfx(sfx_id id);
void audio_play_ambient(const char* wav_path, int volume_0_255);
void audio_stop_ambient(void);
#else
// PC/otros: stubs para no romper build
bool audio_init(void){ return true; }
void audio_update(void){}
void audio_shutdown(void){}
void audio_play_sfx(sfx_id id){ (void)id; }
void audio_play_ambient(const char* wav_path, int volume_0_255){ (void)wav_path; (void)volume_0_255; }
void audio_stop_ambient(void){}
#endif