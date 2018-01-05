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
