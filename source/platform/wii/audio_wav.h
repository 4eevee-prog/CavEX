#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint32_t sample_rate;
  uint16_t channels;
  uint16_t bits_per_sample; // esperamos 16
  uint32_t data_size;
  uint8_t* data;            // PCM little-endian
} wav_pcm_t;

bool wav_load_pcm16(const char* path, wav_pcm_t* out);
void wav_free(wav_pcm_t* w);