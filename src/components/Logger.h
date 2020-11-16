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

#ifdef LOG_TO_FILE
#include <stdio.h>
#endif

// Must be at least MAX_LINE_SIZE + 3
#ifndef RING_LOG_MAX_BUFFER_SIZE
#define RING_LOG_MAX_BUFFER_SIZE 65536
#endif

class Logger: public libretro::LoggerComponent
{
public:
  bool init();
  void destroy();

  std::string contents() const;
  
  typedef bool (*Iterator)(enum retro_log_level level, const char* line, void* ud);
  void iterate(Iterator iterator, void* ud) const;

protected:
  void   log(enum retro_log_level level, const char* msg, size_t length) override;

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

#ifdef LOG_TO_FILE
  FILE* _file;
#endif
};
