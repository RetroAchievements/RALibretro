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
