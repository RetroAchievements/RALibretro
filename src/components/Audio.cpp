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

bool Audio::init(libretro::LoggerComponent* logger, double sample_rate, Fifo* fifo)
{
  _coreRate = 0;
  _resampler = NULL;

  _logger = logger;
  _sampleRate = sample_rate;

  _rateControlDelta = 0.005;
  _currentRatio = 0.0;
  _originalRatio = 0.0;

  _fifo = fifo;
  return true;
}

void Audio::destroy()
{
  if (_resampler != NULL)
  {
    speex_resampler_destroy(_resampler);
  }
}

bool Audio::setRate(double rate)
{
  if (_resampler != NULL)
  {
    speex_resampler_destroy(_resampler);
  }

  _coreRate = rate;
  _currentRatio = _originalRatio = _sampleRate / _coreRate;

  int error;
  _resampler = speex_resampler_init(2, _coreRate, _sampleRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &error);

  if (_resampler == NULL)
  {
    _logger->error(TAG "speex_resampler_init: %s", speex_resampler_strerror(error));
    return false;
  }
  else
  {
    _logger->info(TAG "Resampler initialized to convert from %f to %f", _coreRate, _sampleRate);
  }

  return true;
}

void Audio::consume(const int16_t* samples, size_t frames)
{
  _logger->debug(TAG "Consuming %zu audio frames", frames);

  size_t bytes = frames * 2 * sizeof(int16_t);
  size_t avail = _fifo->free();

  while (bytes > avail)
  {
    _logger->debug(TAG "FIFO has only %zu bytes, sleeping", avail);
    SDL_Delay(1);
    avail = _fifo->free();
  }

  _fifo->write(samples, bytes);
  _logger->debug(TAG "Wrote %zu bytes to the FIFO", bytes);
}

void Audio::produce(int16_t* samples, size_t frames)
{
  _logger->debug(TAG "Producing %zu audio frames", frames);

  size_t out_bytes = frames * 2 * sizeof(int16_t);
  size_t avail = _fifo->free();

  /* Readjust the audio input rate. */
  int    half_size = (int)_fifo->size() / 2;
  int    delta_mid = (int)avail - half_size;
  double direction = (double)delta_mid / (double)half_size;
  double adjust = 1.0 + _rateControlDelta * direction;

  _currentRatio = _originalRatio * adjust;

  _logger->debug(TAG "Original ratio %f adjusted by %f to %f", _originalRatio, adjust, _currentRatio);

  spx_uint32_t out_samples = frames * 2;
  spx_uint32_t in_samples = (spx_uint32_t)(out_samples / _currentRatio);
  in_samples += in_samples & 1; // request an even number of samples (stereo)
  size_t in_bytes = in_samples * sizeof(int16_t);
  int16_t* input = (int16_t*)alloca(in_bytes);

  if (input == NULL)
  {
    memset(samples, 0, out_bytes);
    _logger->error(TAG "Error allocating input buffer, sending silence");
    return;
  }

  while (in_bytes > avail)
  {
    _logger->debug(TAG "Requested %u bytes but only %zu available, sleeping", in_bytes, avail);
    SDL_Delay(1);
    avail = _fifo->free();
  }

  _fifo->read(input, in_bytes);

  _logger->debug(TAG "Resampling %u samples to %u", in_samples, out_samples);
  int error = speex_resampler_process_int(_resampler, 0, input, &in_samples, samples, &out_samples);
  _logger->debug(TAG "Resampled  %u samples to %u", in_samples, out_samples);

  if (error != RESAMPLER_ERR_SUCCESS)
  {
    memset(samples, 0, out_bytes);
    _logger->error(TAG "speex_resampler_process_int: %s, sending silence", speex_resampler_strerror(error));
    return;
  }
}
