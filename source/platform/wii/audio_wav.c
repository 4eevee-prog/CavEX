#include "audio_wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32_le(const uint8_t* p){
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint16_t rd_u16_le(const uint8_t* p){
  return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}

bool wav_load_pcm16(const char* path, wav_pcm_t* out){
  memset(out, 0, sizeof(*out));
  FILE* f = fopen(path, "rb");
  if(!f) return false;

  uint8_t hdr[12];
  if(fread(hdr,1,12,f)!=12){ fclose(f); return false; }
  if(memcmp(hdr,"RIFF",4)!=0 || memcmp(hdr+8,"WAVE",4)!=0){ fclose(f); return false; }

  bool have_fmt=false, have_data=false;
  uint16_t audio_format=0;
  uint32_t data_size=0;
  uint8_t* data=NULL;
  uint16_t channels=0, bps=0;
  uint32_t sample_rate=0;

  while(!have_data){
    uint8_t chdr[8];
    if(fread(chdr,1,8,f)!=8) break;
    uint32_t chunk_sz = rd_u32_le(chdr+4);

    if(memcmp(chdr,"fmt ",4)==0){
      uint8_t* fmt = (uint8_t*)malloc(chunk_sz);
      if(!fmt){ fclose(f); return false; }
      if(fread(fmt,1,chunk_sz,f)!=chunk_sz){ free(fmt); fclose(f); return false; }

      audio_format = rd_u16_le(fmt+0);
      channels     = rd_u16_le(fmt+2);
      sample_rate  = rd_u32_le(fmt+4);
      bps          = rd_u16_le(fmt+14);

      free(fmt);
      have_fmt = true;
    } else if(memcmp(chdr,"data",4)==0){
      data = (uint8_t*)malloc(chunk_sz);
      if(!data){ fclose(f); return false; }
      if(fread(data,1,chunk_sz,f)!=chunk_sz){ free(data); fclose(f); return false; }
      data_size = chunk_sz;
      have_data = true;
    } else {
      fseek(f, chunk_sz, SEEK_CUR);
    }

    // padding
    if(chunk_sz & 1) fseek(f, 1, SEEK_CUR);
  }

  fclose(f);

  if(!have_fmt || !have_data) { free(data); return false; }
  // PCM = 1, 16-bit
  if(audio_format != 1  bps != 16  (channels!=1 && channels!=2)) { free(data); return false; }

  out->channels = channels;
  out->bits_per_sample = bps;
  out->sample_rate = sample_rate;
  out->data_size = data_size;
  out->data = data;
  return true;
}

void wav_free(wav_pcm_t* w){
  if(w && w->data){ free(w->data); }
  if(w) memset(w,0,sizeof(*w));
}
