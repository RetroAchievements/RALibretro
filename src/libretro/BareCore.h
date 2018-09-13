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

#include "libretro.h"
#include "Components.h"
#include "dynlib/dynlib.h"

namespace libretro
{
  /**
   * Core represents a libretro core loaded from the file system, i.e.
   * stella_libretro.so. Its methods map 1:1 to the ones from the libretro
   * API, not couting load, that loads a core, and destroy, which unloads the
   * core and destroys the object.
   *
   * It's the caller's responsibility to ensure that the core is always in a
   * consistent state.
   */
  class BareCore
  {
  public:
    // Loads a core specified by its file path.
    bool load(libretro::LoggerComponent* logger, const char* path);
    
    // Unloads the core from memory.
    void destroy();

    // All the remaining methods map 1:1 to the libretro API.
    void     init() const;
    void     deinit() const;
    unsigned apiVersion() const;
    void     getSystemInfo(struct retro_system_info* info) const;
    void     getSystemAVInfo(struct retro_system_av_info* info) const;
    void     setEnvironment(retro_environment_t cb) const;
    void     setVideoRefresh(retro_video_refresh_t cb) const;
    void     setAudioSample(retro_audio_sample_t cb) const;
    void     setAudioSampleBatch(retro_audio_sample_batch_t cb) const;
    void     setInputPoll(retro_input_poll_t cb) const;
    void     setInputState(retro_input_state_t cb) const;
    void     setControllerPortDevice(unsigned port, unsigned device) const;
    void     reset() const;
    void     run() const;
    size_t   serializeSize() const;
    bool     serialize(void* data, size_t size) const;
    bool     unserialize(const void* data, size_t size) const;
    void     cheatReset() const;
    void     cheatSet(unsigned index, bool enabled, const char* code) const;
    bool     loadGame(const struct retro_game_info* game) const;
    bool     loadGameSpecial(unsigned game_type, const struct retro_game_info* info, size_t num_info) const;
    void     unloadGame() const;
    unsigned getRegion() const;
    void*    getMemoryData(unsigned id) const;
    size_t   getMemorySize(unsigned id) const;

  protected:
    libretro::LoggerComponent* _logger;
    dynlib_t _handle;

    void     (*_init)();
    void     (*_deinit)();
    unsigned (*_apiVersion)();
    void     (*_getSystemInfo)(struct retro_system_info*);
    void     (*_getSystemAVInfo)(struct retro_system_av_info*);
    void     (*_setEnvironment)(retro_environment_t);
    void     (*_setVideoRefresh)(retro_video_refresh_t);
    void     (*_setAudioSample)(retro_audio_sample_t);
    void     (*_setAudioSampleBatch)(retro_audio_sample_batch_t);
    void     (*_setInputPoll)(retro_input_poll_t);
    void     (*_setInputState)(retro_input_state_t);
    void     (*_setControllerPortDevice)(unsigned, unsigned);
    void     (*_reset)();
    void     (*_run)();
    size_t   (*_serializeSize)();
    bool     (*_serialize)(void*, size_t);
    bool     (*_unserialize)(const void*, size_t);
    void     (*_cheatReset)();
    void     (*_cheatSet)(unsigned, bool, const char*);
    bool     (*_loadGame)(const struct retro_game_info*);
    bool     (*_loadGameSpecial)(unsigned, const struct retro_game_info*, size_t);
    void     (*_unloadGame)();
    unsigned (*_getRegion)();
    void*    (*_getMemoryData)(unsigned);
    size_t   (*_getMemorySize)(unsigned);
  };
}
