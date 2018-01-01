#pragma once

#include "libretro/Core.h"

template<size_t N>
class Allocator: public libretro::AllocatorComponent
{
public:
  inline bool init(libretro::LoggerComponent* logger)
  {
    _logger = logger;
    _head = 0;
    return true;
  }

  inline void destroy() {}

  virtual void  reset() override
  {
    _head = 0;
  }

  virtual void* allocate(size_t alignment, size_t numBytes) override
  {
    size_t align_m1 = alignment - 1;
    size_t ptr = (_head + align_m1) & ~align_m1;

    if (numBytes <= (kBufferSize - ptr))
    {
      _head = ptr + numBytes;
      return (void*)(_buffer + ptr);
    }

    _logger->printf(RETRO_LOG_ERROR, "Out of memory allocating %u bytes", numBytes);
    return NULL;
  }

protected:
  enum
  {
    kBufferSize = N
  };

  libretro::LoggerComponent* _logger;
  uint8_t        _buffer[N];
  size_t         _head;
};
