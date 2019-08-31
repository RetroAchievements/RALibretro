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

#include "BareCore.h"

#include "Util.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define TAG "[BCR] "

#define CORE_DLSYM(prop, name) \
  do { \
    void* sym = dynlib_symbol(_handle, name); \
    if (!sym) goto error; \
    memcpy(&prop, &sym, sizeof(prop)); \
    _logger->info(TAG "Found libretro API %s", name); \
  } while (0)

bool libretro::BareCore::load(libretro::LoggerComponent* logger, const char* path)
{
  _logger = logger;

  _logger->info(TAG "Loading core %s", path);

  _handle = dynlib_open(path);

  if (!_handle)
  {
    const std::wstring unicodePath = util::utf8ToUChar(path);
    _handle = LoadLibraryW(unicodePath.c_str());
  }

  if (_handle == NULL)
  {
  error:
    _logger->error(TAG "Error loading core: %s", dynlib_error());
    
    if (_handle != NULL)
    {
      dynlib_close(_handle);
    }

    return false;
  }

  CORE_DLSYM(_init, "retro_init");
  CORE_DLSYM(_deinit, "retro_deinit");
  CORE_DLSYM(_apiVersion, "retro_api_version");
  CORE_DLSYM(_getSystemInfo, "retro_get_system_info");
  CORE_DLSYM(_getSystemAVInfo, "retro_get_system_av_info");
  CORE_DLSYM(_setEnvironment, "retro_set_environment");
  CORE_DLSYM(_setVideoRefresh, "retro_set_video_refresh");
  CORE_DLSYM(_setAudioSample, "retro_set_audio_sample");
  CORE_DLSYM(_setAudioSampleBatch, "retro_set_audio_sample_batch");
  CORE_DLSYM(_setInputPoll, "retro_set_input_poll");
  CORE_DLSYM(_setInputState, "retro_set_input_state");
  CORE_DLSYM(_setControllerPortDevice, "retro_set_controller_port_device");
  CORE_DLSYM(_reset, "retro_reset");
  CORE_DLSYM(_run, "retro_run");
  CORE_DLSYM(_serializeSize, "retro_serialize_size");
  CORE_DLSYM(_serialize, "retro_serialize");
  CORE_DLSYM(_unserialize, "retro_unserialize");
  CORE_DLSYM(_cheatReset, "retro_cheat_reset");
  CORE_DLSYM(_cheatSet, "retro_cheat_set");
  CORE_DLSYM(_loadGame, "retro_load_game");
  CORE_DLSYM(_loadGameSpecial, "retro_load_game_special");
  CORE_DLSYM(_unloadGame, "retro_unload_game");
  CORE_DLSYM(_getRegion, "retro_get_region");
  CORE_DLSYM(_getMemoryData, "retro_get_memory_data");
  CORE_DLSYM(_getMemorySize, "retro_get_memory_size");

  _logger->info(TAG "Core successfully loaded");
  return true;
}

#undef CORE_DLSYM

#define CORE_DLSYM(prop) \
  do { \
    void* sym = NULL; \
    memcpy(&prop, &sym, sizeof(prop)); \
  } while (0)

void libretro::BareCore::destroy()
{
  CORE_DLSYM(_init);
  CORE_DLSYM(_deinit);
  CORE_DLSYM(_apiVersion);
  CORE_DLSYM(_getSystemInfo);
  CORE_DLSYM(_getSystemAVInfo);
  CORE_DLSYM(_setEnvironment);
  CORE_DLSYM(_setVideoRefresh);
  CORE_DLSYM(_setAudioSample);
  CORE_DLSYM(_setAudioSampleBatch);
  CORE_DLSYM(_setInputPoll);
  CORE_DLSYM(_setInputState);
  CORE_DLSYM(_setControllerPortDevice);
  CORE_DLSYM(_reset);
  CORE_DLSYM(_run);
  CORE_DLSYM(_serializeSize);
  CORE_DLSYM(_serialize);
  CORE_DLSYM(_unserialize);
  CORE_DLSYM(_cheatReset);
  CORE_DLSYM(_cheatSet);
  CORE_DLSYM(_loadGame);
  CORE_DLSYM(_loadGameSpecial);
  CORE_DLSYM(_unloadGame);
  CORE_DLSYM(_getRegion);
  CORE_DLSYM(_getMemoryData);
  CORE_DLSYM(_getMemorySize);

  dynlib_close(_handle);

  _logger->info(TAG "Core destroyed");
}

#undef CORE_DLSYM

void libretro::BareCore::init() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  _init();
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::deinit() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  if (_deinit)
    _deinit();
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

unsigned libretro::BareCore::apiVersion() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  unsigned x = _apiVersion();
  _logger->debug(TAG "Called  %s -> %u", __FUNCTION__, x);
  return x;
}

void libretro::BareCore::getSystemInfo(struct retro_system_info* info) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, info);
  _getSystemInfo(info);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::getSystemAVInfo(struct retro_system_av_info* info) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, info);
  _getSystemAVInfo(info);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setEnvironment(retro_environment_t cb) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, cb);
  _setEnvironment(cb);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setVideoRefresh(retro_video_refresh_t cb) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, cb);
  _setVideoRefresh(cb);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setAudioSample(retro_audio_sample_t cb) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, cb);
  _setAudioSample(cb);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setAudioSampleBatch(retro_audio_sample_batch_t cb) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, cb);
  _setAudioSampleBatch(cb);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setInputPoll(retro_input_poll_t cb) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, cb);
  _setInputPoll(cb);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setInputState(retro_input_state_t cb) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, cb);
  _setInputState(cb);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::setControllerPortDevice(unsigned port, unsigned device) const
{
  _logger->debug(TAG "Calling %s(%u, %u)", __FUNCTION__, port, device);
  _setControllerPortDevice(port, device);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::reset() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  _reset();
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::run() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  _run();
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

size_t libretro::BareCore::serializeSize() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  size_t x = _serializeSize();
  _logger->debug(TAG "Called  %s -> %zu", __FUNCTION__, x);
  return x;
}

bool libretro::BareCore::serialize(void* data, size_t size) const
{
  _logger->debug(TAG "Calling %(%p, %zu)", __FUNCTION__, data, size);
  bool x = _serialize(data, size);
  _logger->debug(TAG "Called  %s -> %d", __FUNCTION__, x);
  return x;
}

bool libretro::BareCore::unserialize(const void* data, size_t size) const
{
  _logger->debug(TAG "Calling %s(%p, %zu)", __FUNCTION__, data, size);
  bool x = _unserialize(data, size);
  _logger->debug(TAG "Called  %s -> %d", __FUNCTION__, x);
  return x;
}

void libretro::BareCore::cheatReset() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  return _cheatReset();
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

void libretro::BareCore::cheatSet(unsigned index, bool enabled, const char* code) const
{
  _logger->debug(TAG "Calling %s(%u, %d, \"%s\")", __FUNCTION__, index, enabled, code);
  _cheatSet(index, enabled, code);
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

bool libretro::BareCore::loadGame(const struct retro_game_info* game) const
{
  _logger->debug(TAG "Calling %s(%p)", __FUNCTION__, game);
  bool x = _loadGame(game);
  _logger->debug(TAG "Called  %s -> %d", __FUNCTION__, x);
  return x;
}

bool libretro::BareCore::loadGameSpecial(unsigned game_type, const struct retro_game_info* info, size_t num_info) const
{
  _logger->debug(TAG "Calling %s(%u, %p, %zu)", __FUNCTION__, game_type, info, num_info);
  bool x = _loadGameSpecial(game_type, info, num_info);
  _logger->debug(TAG "Called  %s -> %d", __FUNCTION__, x);
  return x;
}

void libretro::BareCore::unloadGame() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  _unloadGame();
  _logger->debug(TAG "Called  %s", __FUNCTION__);
}

unsigned libretro::BareCore::getRegion() const
{
  _logger->debug(TAG "Calling %s", __FUNCTION__);
  unsigned x = _getRegion();
  _logger->debug(TAG "Called  %s -> %u", __FUNCTION__, x);
  return x;
}

void* libretro::BareCore::getMemoryData(unsigned id) const
{
  _logger->debug(TAG "Calling %s(%u)", __FUNCTION__, id);
  void* x = _getMemoryData(id);
  _logger->debug(TAG "Called  %s -> %p", __FUNCTION__, x);
  return x;
}

size_t libretro::BareCore::getMemorySize(unsigned id) const
{
  _logger->debug(TAG "Calling %s(%u)", __FUNCTION__, id);
  size_t x = _getMemorySize(id);
  _logger->debug(TAG "Called  %s -> %zu", __FUNCTION__, x);
  return x;
}
