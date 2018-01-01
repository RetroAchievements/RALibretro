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
