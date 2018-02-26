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

#include "Logger.h"

#include <SDL_log.h>
#include <windows.h>

extern HWND g_mainWindow;

bool Logger::init()
{
  // Do compile-time checks for the line and buffer sizes.
  char check_line_size[RING_LOG_MAX_LINE_SIZE <= 65535 ? 1 : -1];
  char check_buffer_size[RING_LOG_MAX_BUFFER_SIZE >= RING_LOG_MAX_LINE_SIZE + 3 ? 1 : -1];
  
  (void)check_line_size;
  (void)check_buffer_size;

  _avail = RING_LOG_MAX_BUFFER_SIZE;
  _first = _last = 0;
  return true;
}

void Logger::vprintf(enum retro_log_level level, const char* fmt, va_list args)
{
  char line[RING_LOG_MAX_LINE_SIZE];
  size_t length = vsnprintf(line, sizeof(line), fmt, args);
  
  if (length > sizeof(line))
  {
    // Line size too small, truncated. Consider increasing the array size.
    length = sizeof(line);
  }
  
  length += 3; // Add one byte for the level and two bytes for the length.
  unsigned char meta[3];
  
  if (length > _avail)
  {
    do
    {
      // Remove content until we have enough space.
      read(meta, 3);
      skip(meta[1] | meta[2] << 8); // Little endian.
    }
    while (length > _avail);
  }
  
  length -= 3;
  
  meta[0] = level;
  meta[1] = length & 0xff;
  meta[2] = length >> 8;
  
  write(meta, 3);
  write(line, length);

  ::printf("%s\n", line);
  fflush(stdout);
}

void Logger::debug(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(RETRO_LOG_DEBUG, format, args);
  va_end(args);
}

void Logger::info(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(RETRO_LOG_INFO, format, args);
  va_end(args);
}

void Logger::warn(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(RETRO_LOG_WARN, format, args);
  va_end(args);
}

void Logger::error(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(RETRO_LOG_ERROR, format, args);
  va_end(args);
}

std::string Logger::contents() const
{
  struct Iterator
  {
    static bool iterator(enum retro_log_level level, const char* line, void* ud)
    {
      std::string* buffer = (std::string*)ud;

      switch (level)
      {
        case RETRO_LOG_DEBUG: *buffer += "[DEBUG] "; break;
        case RETRO_LOG_INFO:  *buffer += "[INFO] "; break;
        case RETRO_LOG_WARN:  *buffer += "[WARN] "; break;
        case RETRO_LOG_ERROR: *buffer += "[ERROR] "; break;
        case RETRO_LOG_DUMMY: *buffer += "[DUMMY] "; break;
      }

      *buffer += line;
      *buffer += "\r\n";

      return true;
    }
  };

  std::string buffer;
  iterate(Iterator::iterator, &buffer);
  return buffer;
}

void Logger::iterate(Iterator iterator, void* ud) const
{
  size_t pos = _first;
  
  while (pos != _last)
  {
    char line[RING_LOG_MAX_LINE_SIZE + 1];
    unsigned char meta[3];
    pos = peek(pos, meta, 3);
    
    enum retro_log_level level = (enum retro_log_level)meta[0];
    size_t length = meta[1] | meta[2] << 8;
    pos = peek(pos, line, length);
    line[length] = 0;
    
    if (!iterator(level, line, ud))
    {
      return;
    }
  }
}

void Logger::write(const void* data, size_t size)
{
  size_t first = size;
  size_t second = 0;
  
  if (first > RING_LOG_MAX_BUFFER_SIZE - _last)
  {
    first = RING_LOG_MAX_BUFFER_SIZE - _last;
    second = size - first;
  }
  
  char* dest = _buffer + _last;
  memcpy(dest, data, first);
  memcpy(_buffer, (char*)data + first, second);
  
  _last = (_last + size) % RING_LOG_MAX_BUFFER_SIZE;
  _avail -= size;
}

void Logger::read(void* data, size_t size)
{
  size_t first = size;
  size_t second = 0;
  
  if (first > RING_LOG_MAX_BUFFER_SIZE - _first)
  {
    first = RING_LOG_MAX_BUFFER_SIZE - _first;
    second = size - first;
  }
  
  char* src = _buffer + _first;
  memcpy(data, src, first);
  memcpy((char*)data + first, _buffer, second);
  
  _first = (_first + size) % RING_LOG_MAX_BUFFER_SIZE;
  _avail += size;
}

size_t Logger::peek(size_t pos, void* data, size_t size) const
{
  size_t first = size;
  size_t second = 0;
  
  if (first > RING_LOG_MAX_BUFFER_SIZE - pos)
  {
    first = RING_LOG_MAX_BUFFER_SIZE - pos;
    second = size - first;
  }
  
  const char* src = _buffer + pos;
  memcpy(data, src, first);
  memcpy((char*)data + first, _buffer, second);
  
  return (pos + size) % RING_LOG_MAX_BUFFER_SIZE;
}
