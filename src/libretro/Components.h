/*
The MIT License (MIT)

Copyright (c) 2016 Andre Leiradella

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <stdarg.h>
#include <string>
#include <vector>

#include "libretro.h"

namespace libretro
{
  /**
   * A logger component for Core instances.
   */
  class LoggerComponent
  {
  public:
    virtual void vprintf(enum retro_log_level level, const char* fmt, va_list args) = 0;

    inline void printf(enum retro_log_level level, const char* fmt, ...)
    {
      va_list args;
      va_start(args, fmt);
      vprintf(level, fmt, args);
      va_end(args);
    }
  };

  /**
   * A component that returns configuration values for CoreWrap instances.
   */
  class ConfigComponent
  {
  public:
    virtual const char* getCoreAssetsDirectory() = 0;
    virtual const char* getSaveDirectory() = 0;
    virtual const char* getSystemPath() = 0;

    virtual void setVariables(const struct retro_variable* variables, unsigned count) = 0;
    virtual bool varUpdated() = 0;
    virtual const char* getVariable(const char* variable) = 0;
  };

  /**
   * A Video component.
   */
  class VideoComponent
  {
  public:
    virtual bool setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat, bool needsHardwareRender) = 0;
    virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) = 0;

    virtual bool                 supportsContext(enum retro_hw_context_type type) = 0;
    virtual uintptr_t            getCurrentFramebuffer() = 0;
    virtual retro_proc_address_t getProcAddress(const char* symbol) = 0;

    virtual void showMessage(const char* msg, unsigned frames) = 0;
  };

  /**
   * An audio component.
   */
  class AudioComponent
  {
  public:
    virtual bool setRate(double rate) = 0;
    virtual void mix(const int16_t* samples, size_t frames) = 0;
  };

  /**
   * A component that provides input state to CoreWrap instances.
   */
  class InputComponent
  {
  public:
    virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) = 0;

    virtual void     setControllerInfo(const struct retro_controller_info* info, unsigned count) = 0;
    virtual bool     ctrlUpdated() = 0;
    virtual unsigned getController(unsigned port) = 0;

    virtual void    poll() = 0;
    virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) = 0;
  };

  /**
   * A component that returns memory chunks. Memory is never freed, so
   * provide an implementation that frees all the allocated memory at once
   * when the component is destroyed.
   */
  class AllocatorComponent
  {
  public:
    virtual void  reset() = 0;
    virtual void* allocate(size_t alignment, size_t numBytes) = 0;
  };

  struct Components
  {
    LoggerComponent*    _logger;
    ConfigComponent*    _config;
    VideoComponent*     _video;
    AudioComponent*     _audio;
    InputComponent*     _input;
    AllocatorComponent* _allocator;
  };
}
