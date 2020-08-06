/*
Copyright (C) 2018 Andre Leiradella

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
along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Audio.h"

#include <SDL_timer.h>
#include <string.h>
#include <math.h>

#define TAG "[AUD] "

bool Fifo::init(size_t size)
{
  _mutex = SDL_CreateMutex();
  
  if (!_mutex)
  {
    return false;
  }
  
  _buffer = (uint8_t*)malloc(size);
  
  if (_buffer == NULL)
  {
    SDL_DestroyMutex(_mutex);
    return false;
  }
  
  _size = _avail = size;
  _first = _last = 0;
  return true;
}

void Fifo::destroy()
{
  ::free(_buffer);
  SDL_DestroyMutex(_mutex);
}

void Fifo::reset()
{
  _avail = _size;
  _first = _last = 0;
}

void Fifo::read(void* data, size_t size)
{
  SDL_LockMutex(_mutex);

  size_t first = size;
  size_t second = 0;
  
  if (first > _size - _first)
  {
    first = _size - _first;
    second = size - first;
  }
  
  uint8_t* src = _buffer + _first;
  memcpy(data, src, first);
  memcpy((uint8_t*)data + first, _buffer, second);
  
  _first = (_first + size) % _size;
  _avail += size;

  SDL_UnlockMutex(_mutex);
}

void Fifo::write(const void* data, size_t size)
{
  SDL_LockMutex(_mutex);

  size_t first = size;
  size_t second = 0;
  
  if (first > _size - _last)
  {
    first = _size - _last;
    second = size - first;
  }
  
  uint8_t* dest = _buffer + _last;
  memcpy(dest, data, first);
  memcpy(_buffer, (uint8_t*)data + first, second);
  
  _last = (_last + size) % _size;
  _avail -= size;

  SDL_UnlockMutex(_mutex);
}

size_t Fifo::occupied()
{
  size_t avail;

  SDL_LockMutex(_mutex);
  avail = _size - _avail;
  SDL_UnlockMutex(_mutex);

  return avail;
}

size_t Fifo::free()
{
  size_t avail;

  SDL_LockMutex(_mutex);
  avail = _avail;
  SDL_UnlockMutex(_mutex);

  return avail;
}

bool Audio::init(libretro::LoggerComponent* logger, double sample_rate, int channels, Fifo* fifo)
{
  _coreRate = 0;
  _resamplerLeft = NULL;
  _resamplerRight = NULL;

  _logger = logger;
  _sampleRate = sample_rate;
  _channels = channels;

  _currentRatio = 0.0;
  _originalRatio = 0.0;

  _fifo = fifo;
  return true;
}

void Audio::destroy()
{
  if (_resamplerLeft != NULL)
  {
    speex_resampler_destroy(_resamplerLeft);
    _resamplerLeft = NULL;
  }

  if (_resamplerRight != NULL)
  {
    speex_resampler_destroy(_resamplerRight);
    _resamplerRight = NULL;
  }
}

bool Audio::setRate(double rate)
{
  destroy();

  _coreRate = rate;
  _currentRatio = _originalRatio = _sampleRate / _coreRate;

  if (_sampleRate == _coreRate)
  {
    _logger->info(TAG "Resampler not needed to convert from %f to %f", _coreRate, _sampleRate);
  }
  else
  {
    int error;
    _resamplerLeft = speex_resampler_init(1, _coreRate, _sampleRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);

    if (_resamplerLeft == NULL)
    {
      _logger->error(TAG "speex_resampler_init: %s", speex_resampler_strerror(error));
      return false;
    }

    _resamplerRight = speex_resampler_init(1, _coreRate, _sampleRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);
    if (_resamplerRight == NULL)
    {
      _logger->error(TAG "speex_resampler_init: %s", speex_resampler_strerror(error));
      return false;
    }

    _logger->info(TAG "Resampler initialized to convert from %f to %f", _coreRate, _sampleRate);

    /* mimic interleaved support */
    speex_resampler_set_input_stride(_resamplerLeft, 2);
    speex_resampler_set_input_stride(_resamplerRight, 2);
    speex_resampler_set_output_stride(_resamplerLeft, 2);
    speex_resampler_set_output_stride(_resamplerRight, 2);
  }

  return true;
}

void Audio::mix(const int16_t* samples, size_t frames)
{
  _logger->debug(TAG "Processing %zu audio frames", frames);

  size_t avail = _fifo->free();
  size_t output_size;
  int16_t* output;
  spx_uint32_t out_len;

  if (_sampleRate == _coreRate)
  {
    /* no resampling needed */
    output = (int16_t*)samples;
    out_len = frames * 2;
  }
  else
  {
#if 0 /* adjusting the sample rate between resampling calls can cause clicking if sufficient data is still in the buffer from the previous resample */
    /* adjust the audio input rate. */
    const int    half_size = (int)_fifo->size() / 2;
    const int    delta_mid = (int)avail - half_size;
    const double direction = (double)delta_mid / (double)half_size;
    const double rateControlDelta = 0.005;
    const double adjust = 1.0 + rateControlDelta * direction;

    _currentRatio = _originalRatio * adjust;

    if (_currentRatio != _originalRatio)
      _logger->debug(TAG "Original ratio %f adjusted by %f to %f", _originalRatio, adjust, _currentRatio);
#endif

    /* allocate output buffer */
    out_len = (spx_uint32_t)ceil(frames * 2 * _currentRatio);
    out_len += (out_len & 1);  /* don't send incomplete frames */

    output_size = out_len * sizeof(int16_t);
    output = (int16_t*)alloca(output_size);

    if (output == NULL)
    {
      _logger->error(TAG "Error allocating audio resampling output buffer");
      return;
    }

    /* do the resampling */
    spx_uint32_t in_frames = frames;
    spx_uint32_t out_frames = out_len / 2;
    _logger->debug(TAG "Resampling %u samples to %u", in_frames * 2, out_frames * 2);

    {
      /* speex seems to have issues upsampling SNES (32KHz) properly without introducing static.
       * This seems to be caused by whatever state is maintained between resampling calls. The
       * interleaved resampler only remembers state for the last channel - using one resampler
       * and calling speex_resampler_process_interleaved_int was creating static in the left
       * channel. To address that, we use two resamplers (one for each channel) and let each keep
       * their own state.
       */
      int error = speex_resampler_process_int(_resamplerLeft, 0, samples, &in_frames, output, &out_frames);
      if (error == RESAMPLER_ERR_SUCCESS)
      {
        error = speex_resampler_process_int(_resamplerRight, 0, samples + 1, &in_frames, output + 1, &out_frames);
        if (error == RESAMPLER_ERR_SUCCESS)
        {
          /* update with the actual number of frames created */
          out_len = out_frames * 2;
        }
      }

      if (error != RESAMPLER_ERR_SUCCESS)
      {
        memset(output, 0, output_size);
        _logger->error(TAG "speex_resampler_process_interleaved_int: %s", speex_resampler_strerror(error));
      }
    }
  }

  /* flush to FIFO queue */
  output_size = out_len * sizeof(int16_t);
  const size_t needed = out_len * _channels;
  if (avail < needed)
  {
    _logger->debug(TAG "Waiting for FIFO (need %zu bytes but only %zu available), sleeping", needed, avail);

    const int MAX_WAIT = 250;
    int tries = MAX_WAIT;
    do {
      SDL_Delay(1);
      avail = _fifo->free();
      if (avail >= needed)
        break;

      /* prevent infinite loop if fifo full */
      if (--tries == 0)
      {
        _logger->warn(TAG "FIFO still full after %dms, flushing", MAX_WAIT);
        _fifo->reset();
        break;
      }
    } while (true);
  }

  if (_channels == 2)
  {
    /* input is 2 channels, output is 2 channels, just flush it */
    _fifo->write(output, output_size);
  }
  else
  {
    /* input is 2 channels, have to add silence for other channels */
    int16_t* read = output;
    int16_t* stop = output + out_len;
    const size_t flush_frames = 64;
    int16_t* expanded = (int16_t*)alloca(flush_frames * _channels * sizeof(int16_t));
    int16_t* write = expanded;
    int count = 0;

    if (expanded == NULL)
    {
      _logger->error(TAG "Error allocating audio channel expansion buffer");
      return;
    }

    while (read < stop)
    {
      *write++ = *read++;
      if (_channels > 1)
      {
        *write++ = *read++;

        for (int i = 2; i < _channels; ++i)
          *write++ = 0;
      }
      else
      {
        ++read;
      }

      if (++count == flush_frames)
      {
        _fifo->write(expanded, flush_frames * _channels * sizeof(int16_t));
        write = expanded;
        count = 0;
      }
    }

    if (count > 0)
      _fifo->write(expanded, count * _channels * sizeof(int16_t));
  }

  _logger->debug(TAG "Wrote %zu bytes to the FIFO", needed);
}
