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

#pragma once

#include "libretro/Components.h"

#include "speex/speex_resampler.h"

#include <SDL_mutex.h>

class Fifo
{
public:
  bool init(size_t size);
  void destroy();
  void reset();

  void read(void* data, size_t size);
  void write(const void* data, size_t size);

  inline size_t size() { return _size; }

  size_t occupied();
  size_t free();

protected:
  SDL_mutex* _mutex;
  uint8_t*   _buffer;
  size_t     _size;
  size_t     _avail;
  size_t     _first;
  size_t     _last;
};

class Audio: public libretro::AudioComponent
{
public:
  bool init(libretro::LoggerComponent* logger, double sample_rate, Fifo* fifo);
  void destroy();

  virtual bool setRate(double rate) override;
  virtual void mix(const int16_t* samples, size_t frames) override;

protected:
  libretro::LoggerComponent* _logger;

  double _sampleRate;
  double _coreRate;

  double _rateControlDelta;
  double _currentRatio;
  double _originalRatio;
  SpeexResamplerState* _resampler;

  Fifo* _fifo;
};
