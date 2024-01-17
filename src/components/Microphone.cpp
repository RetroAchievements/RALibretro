/*
Copyright (C) 2023 Brian Weiss

This file is part of RALibretro.

RALibretro is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RALibretro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Microphone.h"

#include "Application.h"

#include <SDL_audio.h>
#include "speex/speex_resampler.h"

#include <math.h>

#define TAG "[MIC] "

struct Microphone::SDLData
{
  SDL_AudioDeviceID deviceId;
  SDL_AudioSpec deviceSpec;
  unsigned coreRate;
  bool enabled;
  Fifo fifo;
  libretro::LoggerComponent* logger;
  SpeexResamplerState* resampler;
};

bool Microphone::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  return true;
}

void Microphone::destroy()
{
  if (_data)
    closeMic((retro_microphone_t*)_data);
}

void Microphone::recordCallback(void* data, Uint8* stream, int len)
{
  SDLData* sdlData = (SDLData*)data;

  if (!sdlData->enabled) /* don't fill buffer when mic is not active, nothing is going to read the buffer */
    return;

  extern Application app;
  if (app.isPaused()) /* don't fill buffer while paused, nothing is running to empty buffer */
    return;

  int frames = len / sizeof(int16_t);
  int16_t* samples = (int16_t*)stream;

  size_t output_size;
  int16_t* output;

  if (sdlData->deviceSpec.freq == (int)sdlData->coreRate)
  {
    /* no resampling needed */
    output = (int16_t*)samples;
    output_size = frames * sizeof(int16_t);
  }
  else
  {
    /* allocate output buffer */
    spx_uint32_t out_len = (spx_uint32_t)ceil(frames * sdlData->coreRate / sdlData->deviceSpec.freq);
    output_size = out_len * sizeof(int16_t);
    output = (int16_t*)alloca(output_size);

    if (output == NULL)
    {
      sdlData->logger->error(TAG "Error allocating audio resampling output buffer");
      return;
    }

    /* do the resampling */
    int error;
    if (!sdlData->resampler)
    {
      sdlData->resampler = speex_resampler_init(1, sdlData->deviceSpec.freq, sdlData->coreRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);

      if (!sdlData->resampler)
      {
        sdlData->logger->error(TAG "speex_resampler_init: %s", speex_resampler_strerror(error));
        SDL_PauseAudioDevice(sdlData->deviceId, true);
        return;
      }
    }

    spx_uint32_t in_frames = frames;
    spx_uint32_t out_frames = out_len;
    sdlData->logger->debug(TAG "Resampling %u samples to %u", in_frames, out_frames);
    error = speex_resampler_process_int(sdlData->resampler, 0, (const int16_t*)samples, &in_frames, output, &out_frames);
    if (error == RESAMPLER_ERR_SUCCESS)
    {
      /* update with the actual number of frames created */
      output_size = out_frames * sizeof(int16_t);
    }
    else
    {
      memset(output, 0, output_size);
      sdlData->logger->error(TAG "speex_resampler_process_int: %s", speex_resampler_strerror(error));
    }
  }

  /* flush to FIFO queue */
  size_t avail = sdlData->fifo.free();
  if (avail < output_size)
  {
    sdlData->logger->debug(TAG "Waiting for FIFO (need %zu bytes but only %zu available), sleeping", output_size, avail);

    const int MAX_WAIT = 250;
    int tries = MAX_WAIT;
    do
    {
      SDL_Delay(1);
      if (!sdlData->enabled)
        return;

      avail = sdlData->fifo.free();
      if (avail >= output_size)
        break;

      /* prevent infinite loop if fifo full */
      if (--tries == 0)
      {
        sdlData->logger->debug(TAG "FIFO still full after %dms, flushing", MAX_WAIT);
        sdlData->fifo.reset();
        avail = sdlData->fifo.free();
        break;
      }
    } while (true);

    if (avail < output_size)
      output_size = avail;
  }

  sdlData->fifo.write(output, output_size);
}

retro_microphone_t* Microphone::openMic(const retro_microphone_params_t* params)
{
  if (_data != NULL) /* only one microphone allowed at a time*/
    return NULL;

  SDLData* data = new SDLData();
  if (data == NULL)
    return NULL;

  data->logger = _logger;
  data->resampler = nullptr;
  data->coreRate = params->rate;
  data->enabled = false;

  SDL_AudioSpec desired_spec {};
  desired_spec.freq = params->rate;
  desired_spec.format = AUDIO_S16SYS;
  desired_spec.channels = 1; /* Microphones only usually provide input in mono */
  desired_spec.samples = 2048;
  desired_spec.userdata = data;
  desired_spec.callback = recordCallback;

  data->deviceId = SDL_OpenAudioDevice(NULL, true, &desired_spec, &data->deviceSpec,
      SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

  if (data->deviceId == 0)
  {
    _logger->error(TAG "Failed to open SDL audio input device: %s", SDL_GetError());
    delete data;
    return NULL;
  }

  _logger->info(TAG "Opened microphone with ID %u (frequency %u [requested %u])", data->deviceId, data->deviceSpec.freq, desired_spec.freq);
  _logger->info(TAG "Microphone audio format: %u-bit %s %s %s endian\n",
                SDL_AUDIO_BITSIZE(data->deviceSpec.format),
                SDL_AUDIO_ISSIGNED(data->deviceSpec.format) ? "signed" : "unsigned",
                SDL_AUDIO_ISFLOAT(data->deviceSpec.format) ? "floating-point" : "integer",
                SDL_AUDIO_ISBIGENDIAN(data->deviceSpec.format) ? "big" : "little");

  data->fifo.init(data->deviceSpec.size * 4);
  _data = data;
  return (retro_microphone_t*)data;
}

void Microphone::closeMic(retro_microphone_t* microphone)
{
  if ((SDLData*)microphone == _data)
  {
    SDL_CloseAudioDevice(_data->deviceId);
    _data->fifo.destroy();

    if (_data->resampler != NULL)
      speex_resampler_destroy(_data->resampler);

    delete _data;
    _data = NULL;
  }
}

bool Microphone::getMicParams(const retro_microphone_t* microphone, retro_microphone_params_t* params)
{
  if (!_data)
    return false;

  params->rate = _data->deviceSpec.freq;
  return true;
}

bool Microphone::getMicState(const retro_microphone_t* microphone)
{
  return _data && _data->enabled;
}

bool Microphone::setMicState(retro_microphone_t* microphone, bool state)
{
  if (!_data)
    return false;

  SDL_PauseAudioDevice(_data->deviceId, !state);
  if (state)
  {
    if (SDL_GetAudioDeviceStatus(_data->deviceId) != SDL_AUDIO_PLAYING)
    {
      _logger->warn(TAG "Failed to start microphone %u: %s", _data->deviceId, SDL_GetError());
      return false;
    }
  }
  else
  {
    switch (SDL_GetAudioDeviceStatus(_data->deviceId))
    {
    case SDL_AUDIO_PLAYING:
      _logger->warn(TAG "Failed to pause microphone %u: %s", _data->deviceId, SDL_GetError());
      return false;
    case SDL_AUDIO_STOPPED:
      _logger->warn(TAG "Microphone %u has been stopped and may not start again: %s", _data->deviceId, SDL_GetError());
      return false;
    default:
      break;
    }

    _data->fifo.reset();
  }

  _data->enabled = state;
  return true;
}

int Microphone::readMic(retro_microphone_t* microphone, int16_t* frames, size_t num_frames)
{
  if (!_data || !_data->enabled)
    return 0;

  size_t num_read = std::min(num_frames, _data->fifo.occupied() / sizeof(int16_t));
  _data->fifo.read(frames, num_read * sizeof(int16_t));
  return (int)num_read;
}


