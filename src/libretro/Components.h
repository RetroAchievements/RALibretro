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

#ifndef LOG_MAX_LINE_SIZE
#define LOG_MAX_LINE_SIZE 1024
#endif

namespace libretro
{
  /**
   * A logger component for Core instances.
   */
  class LoggerComponent
  {
  public:
    virtual void log(enum retro_log_level level, const char* line, size_t length) = 0;

    void vprintf(enum retro_log_level level, const char* fmt, va_list args)
    {
      char line[LOG_MAX_LINE_SIZE];
      size_t length = vsnprintf(line, sizeof(line), fmt, args);

      if (length >= sizeof(line))
      {
        length = sizeof(line) - 1;
        line[length - 1] = line[length - 2] = line[length - 3] = '.';
      }

      while (length > 0 && line[length - 1] == '\n')
      {
        line[--length] = 0;
      }

      log(level, line, length);
    }

    void setLogLevel(enum retro_log_level level) noexcept
    {
      _level = level;
    }

    bool logLevel(enum retro_log_level level) const noexcept
    {
      return (_level <= level);
    }

    void printf(enum retro_log_level level, const char* fmt, ...)
    {
      va_list args;
      va_start(args, fmt);
      vprintf(level, fmt, args);
      va_end(args);
    }

#ifdef NDEBUG
    void debug(const char* fmt, ...) {}
#else
    void debug(const char* fmt, ...)
    {
      if (_level > RETRO_LOG_DEBUG)
        return;

      va_list args;
      va_start(args, fmt);
      vprintf(RETRO_LOG_DEBUG, fmt, args);
      va_end(args);
    }
#endif

    void info(const char* fmt, ...)
    {
      if (_level > RETRO_LOG_INFO)
        return;

      va_list args;
      va_start(args, fmt);
      vprintf(RETRO_LOG_INFO, fmt, args);
      va_end(args);
    }

    void warn(const char* fmt, ...)
    {
      if (_level > RETRO_LOG_WARN)
        return;

      va_list args;
      va_start(args, fmt);
      vprintf(RETRO_LOG_WARN, fmt, args);
      va_end(args);
    }

    void error(const char* fmt, ...)
    {
      va_list args;
      va_start(args, fmt);
      vprintf(RETRO_LOG_ERROR, fmt, args);
      va_end(args);
    }

  private:
    enum retro_log_level _level = RETRO_LOG_INFO;
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
    virtual void setVariables(const struct retro_core_option_definition* options, unsigned count) = 0;
    virtual void setVariables(const struct retro_core_option_v2_definition* options, unsigned count,
      const struct retro_core_option_v2_category* categories, unsigned category_count) = 0;
    virtual void setVariableDisplay(const struct retro_core_option_display* display) = 0;
    virtual bool varUpdated() = 0;
    virtual const char* getVariable(const char* variable) = 0;

    virtual bool getBackgroundInput() = 0;
    virtual void setBackgroundInput(bool value) = 0;

    virtual bool getFastForwarding() = 0;
    virtual void setFastForwarding(bool value) = 0;

    virtual bool getAudioWhileFastForwarding() = 0;
    virtual int getFastForwardRatio() = 0;

    virtual bool getShowSpeedIndicator() = 0;
    virtual void setShowSpeedIndicator(bool value) = 0;

    virtual bool getGameFocusCaptureMouse() = 0;
  };

  /**
   * A component that provides access to the video context.
   */
  class VideoContextComponent
  {
  public:
    virtual void enableCoreContext(bool enable) = 0;
    virtual void resetCoreContext() = 0;
    virtual void swapBuffers() = 0;
  };

  /**
   * A Video component.
   */
  class VideoComponent
  {
  public:
    virtual void setEnabled(bool enabled) = 0;

    virtual bool setGeometry(unsigned width, unsigned height, unsigned maxWidth, unsigned maxHeight, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback) = 0;
    virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) = 0;
    virtual void reset() = 0;

    virtual bool                 supportsContext(enum retro_hw_context_type type) = 0;
    virtual uintptr_t            getCurrentFramebuffer() = 0;
    virtual retro_proc_address_t getProcAddress(const char* symbol) = 0;

    virtual void showMessage(const char* msg, unsigned frames) = 0;

    enum class Speed
    {
      None,
      Paused,
      FastForwarding,
    };
    virtual void showSpeedIndicator(Speed visibleIndicator) = 0;

    /* NOTE: these are counter-clockwise rotations per libretro.h */
    enum class Rotation
    {
      None = 0,
      Ninety = 1,
      OneEighty = 2,
      TwoSeventy = 3
    };

    virtual void setRotation(Rotation rotation) = 0;
    virtual Rotation getRotation() const = 0;
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
   * A microphone component.
   */
  class MicrophoneComponent
  {
  public:
    virtual retro_microphone_t* openMic(const retro_microphone_params_t* params) = 0;
    virtual void closeMic(retro_microphone_t* microphone) = 0;
    virtual bool getMicParams(const retro_microphone_t* microphone, retro_microphone_params_t* params) = 0;
    virtual bool getMicState(const retro_microphone_t* microphone) = 0;
    virtual bool setMicState(retro_microphone_t* microphone, bool state) = 0;
    virtual int readMic(retro_microphone_t* microphone, int16_t* frames, size_t num_frames) = 0;
  };

  /**
   * A component that provides input state to CoreWrap instances.
   */
  class InputComponent
  {
  public:
    virtual void     setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) = 0;
    virtual void     setKeyboardCallback(const struct retro_keyboard_callback* data) = 0;

    virtual void     setControllerInfo(const struct retro_controller_info* info, unsigned count) = 0;
    virtual bool     ctrlUpdated() = 0;
    virtual unsigned getController(unsigned port) = 0;

    virtual bool     setRumble(unsigned port, retro_rumble_effect effect, uint16_t strength) = 0;

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
    LoggerComponent*       logger;
    ConfigComponent*       config;
    VideoContextComponent* videoContext;
    VideoComponent*        video;
    AudioComponent*        audio;
    MicrophoneComponent*   microphone;
    InputComponent*        input;
    AllocatorComponent*    allocator;
  };
}
