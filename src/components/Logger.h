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

// Must be at most 65535
#ifndef RING_LOG_MAX_LINE_SIZE
#define RING_LOG_MAX_LINE_SIZE 1024
#endif

// Must be at least MAX_LINE_SIZE + 3
#ifndef RING_LOG_MAX_BUFFER_SIZE
#define RING_LOG_MAX_BUFFER_SIZE 65536
#endif

class Logger: public libretro::LoggerComponent
{
public:
  bool init();
  void destroy() {}

  virtual void vprintf(enum retro_log_level level, const char* fmt, va_list args) override;

  void debug(const char* format, ...);
  void info(const char* format, ...);
  void warn(const char* format, ...);
  void error(const char* format, ...);

  std::string contents() const;
  
  typedef bool (*Iterator)(enum retro_log_level level, const char* line, void* ud);
  void iterate(Iterator iterator, void* ud) const;

protected:
  void   write(const void* data, size_t size);
  void   read(void* data, size_t size);
  size_t peek(size_t pos, void* data, size_t size) const;
  
  inline void skip(size_t size)
  {
    _first = (_first + size) % RING_LOG_MAX_BUFFER_SIZE;
    _avail += size;
  }
  
  char   _buffer[RING_LOG_MAX_BUFFER_SIZE];
  size_t _avail;
  size_t _first;
  size_t _last;
};
