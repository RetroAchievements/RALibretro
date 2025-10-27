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

#include "Core.h"

#include "Application.h"
#include "Util.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WINDOWS
#include <RA_Interface.h>
#include <io.h>
#endif

/* PPSSPP pushes 512 frames per packet, and relies on the mixer being called for every packet within
 * core.run() to throttle the framerate, so this value cannot exceed 1024 (512 frames * 2 channels).
 */
#define SAMPLE_COUNT 1024

#define TAG "[COR] "

/* These are RetroArch specific callbacks. Some cores expect at least minimal support for them */
#define RETRO_ENVIRONMENT_RETROARCH_START_BLOCK 0x800000
#define RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND (2 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
#define RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB (3 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
#define RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE (4 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)

/* Some libretro callbacks need access to the other frontend elements that are stored
 * in the core instance (like _input). Since we can't pass pointers through libretro,
 * keep a global pointer to the current core object (there should be only one).
 */
static libretro::Core* s_instance = NULL;

extern Application app;

namespace
{
  // Dummy components
  class DummyLogger: public libretro::LoggerComponent
  {
  public:
    virtual void log(enum retro_log_level level, const char* line, size_t length) override
    {
      (void)level;
      (void)line;
      (void)length;
    }
  };

  class DummyConfig: public libretro::ConfigComponent
  {
  public:
    virtual const char* getCoreAssetsDirectory() override
    {
      return "./";
    }

    virtual const char* getSaveDirectory() override
    {
      return "./";
    }

    virtual const char* getSystemPath() override
    {
      return "./";
    }

    virtual void setVariables(const struct retro_variable* variables, unsigned count) override
    {
      (void)variables;
      (void)count;
    }

    virtual void setVariables(const struct retro_core_option_definition* options, unsigned count) override
    {
      (void)options;
      (void)count;
    }

    virtual void setVariables(const struct retro_core_option_v2_definition* options, unsigned count,
      const struct retro_core_option_v2_category* categories, unsigned category_count) override
    {
      (void)options;
      (void)count;
      (void)categories;
      (void)category_count;
    }

    virtual void setVariableDisplay(const struct retro_core_option_display* display) override
    {
      (void)display;
    }

    virtual bool varUpdated() override
    {
      return false;
    }

    virtual const char* getVariable(const char* variable) override
    {
      (void)variable;
      return NULL;
    }

    virtual bool getBackgroundInput() override
    {
      return false;
    }

    virtual void setBackgroundInput(bool value) override
    {
      (void)value;
    }

    virtual bool getFastForwarding() override
    {
      return false;
    }

    virtual void setFastForwarding(bool value) override
    {
      (void)value;
    }

    virtual bool getAudioWhileFastForwarding() override
    {
      return false;
    }

    virtual int getFastForwardRatio() override
    {
      return 5;
    }

    virtual bool getShowSpeedIndicator() override
    {
      return false;
    }

    virtual void setShowSpeedIndicator(bool value) override
    {
      (void)value;
    }

    virtual bool getGameFocusCaptureMouse() override
    {
      return false;
    }
  };

  class DummyVideoContext : public libretro::VideoContextComponent
  {
  public:
    virtual void enableCoreContext(bool enable) override
    {
    }
    virtual void resetCoreContext() override
    {
    }
    virtual void swapBuffers() override
    {
    }
  };

  class DummyVideo: public libretro::VideoComponent
  {
  public:
    virtual void setEnabled(bool enabled) override
    {
      (void)enabled;
    }

    virtual bool setGeometry(unsigned width, unsigned height, unsigned maxWidth, unsigned maxHeight, float aspect, enum retro_pixel_format pixelFormat, const struct retro_hw_render_callback* hwRenderCallback) override
    {
      (void)width;
      (void)height;
      (void)maxWidth;
      (void)maxHeight;
      (void)pixelFormat;
      (void)hwRenderCallback;
      return false;
    }

    virtual void refresh(const void* data, unsigned width, unsigned height, size_t pitch) override
    {
      (void)data;
      (void)width;
      (void)height;
      (void)pitch;
    }

    virtual void reset() override
    {
    }

    virtual bool supportsContext(enum retro_hw_context_type type) override
    {
      (void)type;
      return false;
    }

    virtual uintptr_t getCurrentFramebuffer() override
    {
      return 0;
    }

    virtual retro_proc_address_t getProcAddress(const char* symbol) override
    {
      (void)symbol;
      return NULL;
    }

    virtual void showMessage(const char* msg, unsigned frames) override
    {
      (void)msg;
      (void)frames;
    }

    virtual void showSpeedIndicator(Speed visibleIndicator) override
    {
      (void)visibleIndicator;
    }

    virtual void setRotation(Rotation rotation) override
    {
      (void)rotation;
    }

    Rotation getRotation() const override
    {
      return Rotation::None;
    }
  };

  class DummyAudio: public libretro::AudioComponent
  {
  public:
    virtual bool setRate(double rate) override
    {
      (void)rate;
      return false;
    }

    virtual void mix(const int16_t* samples, size_t frames) override
    {
      (void)samples;
      (void)frames;
    }
  };

  class DummyMicrophone : public libretro::MicrophoneComponent
  {
  public:
    virtual retro_microphone_t* openMic(const retro_microphone_params_t* params) override
    {
      (void)params;
      return NULL;
    }

    virtual void closeMic(retro_microphone_t* microphone) override
    {
      (void)microphone;
    }

    virtual bool getMicParams(const retro_microphone_t* microphone, retro_microphone_params_t* params) override
    {
      (void)microphone;
      (void)params;
      return false;
    }

    virtual bool getMicState(const retro_microphone_t* microphone) override
    {
      (void)microphone;
      return false;
    }

    virtual bool setMicState(retro_microphone_t* microphone, bool state) override
    {
      (void)microphone;
      (void)state;
      return false;
    }

    virtual int readMic(retro_microphone_t* microphone, int16_t* frames, size_t num_frames) override
    {
      (void)microphone;
      (void)frames;
      (void)num_frames;
      return 0;
    }
  };

  class DummyInput: public libretro::InputComponent
  {
  public:
    virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) override
    {
      (void)descs;
      (void)count;
    }

    virtual void setKeyboardCallback(const struct retro_keyboard_callback* data) override
    {
      (void)data;
    }

    virtual void setControllerInfo(const struct retro_controller_info* info, unsigned count) override
    {
      (void)info;
      (void)count;
    }

    virtual bool ctrlUpdated() override
    {
      return false;
    }

    virtual unsigned getController(unsigned port) override
    {
      (void)port;
      return RETRO_DEVICE_NONE;
    }

    virtual bool setRumble(unsigned port, retro_rumble_effect effect, uint16_t strength) override
    {
      (void)port;
      (void)effect;
      (void)strength;
      return false;
    }

    virtual void poll() override {}

    virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) override
    {
      (void)port;
      (void)device;
      (void)index;
      (void)id;
      return 0;
    }
  };

  class DummyAllocator: public libretro::AllocatorComponent
  {
  public:
    virtual void reset() override {}

    virtual void* allocate(size_t alignment, size_t numBytes) override
    {
      (void)alignment;
      (void)numBytes;
      return NULL;
    }
  };
}

// Instances of the dummy components to use in CoreWrap instances by default
static DummyLogger       s_logger;
static DummyConfig       s_config;
static DummyVideoContext s_videoContext;
static DummyVideo        s_video;
static DummyAudio        s_audio;
static DummyMicrophone   s_microphone;
static DummyInput        s_input;
static DummyAllocator    s_allocator;

bool libretro::Core::init(const Components* components)
{
  _logger       = &s_logger;
  _config       = &s_config;
  _videoContext = &s_videoContext;
  _video        = &s_video;
  _audio        = &s_audio;
  _microphone   = &s_microphone;
  _input        = &s_input;
  _allocator    = &s_allocator;

  s_instance    = this;

  if (components != NULL)
  {
    if (components->logger != NULL)       _logger       = components->logger;
    if (components->config != NULL)       _config       = components->config;
    if (components->videoContext != NULL) _videoContext = components->videoContext;
    if (components->video != NULL)        _video        = components->video;
    if (components->audio != NULL)        _audio        = components->audio;
    if (components->microphone != NULL)   _microphone   = components->microphone;
    if (components->input != NULL)        _input        = components->input;
    if (components->allocator != NULL)    _allocator    = components->allocator;
  }

  reset();
  return true;
}

bool libretro::Core::loadCore(const char* core_path)
{
  _allocator->reset();

  _libretroPath = strdup(core_path);
  _samples = alloc<int16_t>(SAMPLE_COUNT);
  _samplesCount = 0;

  if (_libretroPath == NULL || _samples == NULL)
  {
    return false;
  }
  
  if (!_core.load(_logger, core_path))
  {
    return false;
  }

  return true;
}

class CoreErrorLogger : public libretro::LoggerComponent
{
public:
  CoreErrorLogger(libretro::LoggerComponent* logger, std::string* errorBuffer)
    : _logger(logger), _errorBuffer(errorBuffer)
  {
  }

  void log(enum retro_log_level level, const char* line, size_t length) override
  {
    _logger->log(level, line, length);

    if ((level == RETRO_LOG_ERROR || level == RETRO_LOG_WARN) && _errorBuffer)
    {
      if (!_errorBuffer->empty())
        _errorBuffer->push_back('\n');

      _errorBuffer->append(line, length);
    }
  }

private:
  libretro::LoggerComponent* _logger;
  std::string* _errorBuffer;
};

bool libretro::Core::loadGame(const char* game_path, void* data, size_t size, std::string* errorBuffer)
{
  if (game_path == NULL)
  {
    _logger->error(TAG "Can't load game data without a ROM path");
    goto error;
  }
  
  retro_game_info game;
  game.path = game_path;
  game.data = data;
  game.size = size;
  game.meta = NULL;

  // use extended logger to capture load error
  bool result;
  {
    CoreErrorLogger errorLogger(_logger, errorBuffer);
    libretro::LoggerComponent* oldLogger = _logger;
    _logger = &errorLogger;
    result = _core.loadGame(&game);
    _logger = oldLogger;
  }

  if (!result)
  {
    _logger->error(TAG "Error loading content");
    goto error;
  }

  _logger->info(TAG "Content \"%s\" loaded", game_path);

  if (!initAV())
  {
    _core.unloadGame();
    goto error;
  }

  {
    unsigned count;
    const struct retro_controller_info* info = getControllerInfo(&count);

    for (unsigned i = 0; i < count; i++, info++)
    {
      _core.setControllerPortDevice(i, RETRO_DEVICE_NONE);
      _ports[i] = RETRO_DEVICE_NONE;
    }
  }
  
  _gameLoaded = true;
  return true;
  
error:
  _core.deinit();
  _core.destroy();
  reset();
  return false;
}

void libretro::Core::destroy()
{
  if (_needsHardwareRender && _hardwareRenderCallback.context_destroy) {
    _hardwareRenderCallback.context_destroy();
  }

  if (_gameLoaded)
  {
    _core.unloadGame();
  }

  memset(&_diskControlInterface, 0, sizeof(_diskControlInterface));
  _input->setKeyboardCallback(nullptr);

  _core.deinit();
  _core.destroy();
  reset();
}

void libretro::Core::step(bool generateVideo, bool generateAudio)
{
  if (_input->ctrlUpdated())
  {
    for (unsigned i = 0; i < _controllerInfoCount; i++)
    {
      unsigned device = _input->getController(i);

      if (device != _ports[i])
      {
        _ports[i] = device;
        _core.setControllerPortDevice(i, device);
      }
    }
  }

  _video->setEnabled(generateVideo);

  _generateAudio = generateAudio;

  _core.run();

  /* if any audio was buffered, flush it now */
  if (_samplesCount > 0)
  {
    _audio->mix(_samples, _samplesCount / 2);
    _samplesCount = 0;
  }
}

unsigned libretro::Core::getApiVersion()
{
  return _core.apiVersion();
}

unsigned libretro::Core::getRegion()
{
  return _core.getRegion();
}

void* libretro::Core::getMemoryData(unsigned id)
{
  return _core.getMemoryData(id);
}

size_t libretro::Core::getMemorySize(unsigned id)
{
  return _core.getMemorySize(id);
}

void libretro::Core::resetGame()
{
  _core.reset();
}

size_t libretro::Core::serializeSize()
{
  return _core.serializeSize();
}

bool libretro::Core::serialize(void* data, size_t size)
{
  return _core.serialize(data, size);
}

bool libretro::Core::unserialize(const void* data, size_t size, std::string* errorBuffer)
{
  bool result;

  CoreErrorLogger errorLogger(_logger, errorBuffer);
  libretro::LoggerComponent* oldLogger = _logger;
  _logger = &errorLogger;
  result = _core.unserialize(data, size);
  _logger = oldLogger;

  return result;
}

bool libretro::Core::initCore()
{
  struct retro_system_info system_info;
  _core.getSystemInfo(&system_info);

  _systemInfo.library_name     = strdup(system_info.library_name);
  _systemInfo.library_version  = strdup(system_info.library_version);
  _systemInfo.valid_extensions = system_info.valid_extensions != NULL ? strdup(system_info.valid_extensions) : NULL;
  _systemInfo.need_fullpath    = system_info.need_fullpath;
  _systemInfo.block_extract    = system_info.block_extract;

  if (_systemInfo.library_name == NULL || _systemInfo.library_version == NULL || (system_info.valid_extensions != NULL && _systemInfo.valid_extensions == NULL))
  {
    _core.destroy();
    return false;
  }

  _logger->debug(TAG "retro_system_info");
  _logger->debug(TAG "  library_name:     %s", _systemInfo.library_name);
  _logger->debug(TAG "  library_version:  %s", _systemInfo.library_version);
  _logger->debug(TAG "  valid_extensions: %s", _systemInfo.valid_extensions);
  _logger->debug(TAG "  need_fullpath:    %s", _systemInfo.need_fullpath ? "true" : "false");
  _logger->debug(TAG "  block_extract:    %s", _systemInfo.block_extract ? "true" : "false");

  _core.setEnvironment(s_environmentCallback);
  _core.init();
  return true;
}

bool libretro::Core::initAV()
{
  _core.setVideoRefresh(s_videoRefreshCallback);
  _core.setAudioSampleBatch(s_audioSampleBatchCallback);
  _core.setAudioSample(s_audioSampleCallback);
  _core.setInputState(s_inputStateCallback);
  _core.setInputPoll(s_inputPollCallback);
  
  _core.getSystemAVInfo(&_systemAVInfo);
  if (!handleSystemAVInfoChanged())
    goto error;
  
  return true;
  
error:
  _core.unloadGame();
  _core.deinit();
  _core.destroy();
  return false;
}

void libretro::Core::reset()
{
  _gameLoaded = false;
  _samples = NULL;
  _samplesCount = 0;
  _libretroPath = NULL;
  _performanceLevel = 0;
  _pixelFormat = RETRO_PIXEL_FORMAT_UNKNOWN;
  _supportsNoGame = false;
  _supportAchievements = false;
  _fastForwarding = false;
  _inputDescriptorsCount = 0;
  _inputDescriptors = NULL;
  memset(&_hardwareRenderCallback, 0, sizeof(_hardwareRenderCallback));
  _needsHardwareRender = false;
  memset(&_systemInfo, 0, sizeof(_systemInfo));
  memset(&_systemAVInfo, 0, sizeof(_systemAVInfo));
  _subsystemInfoCount = 0;
  _subsystemInfo = NULL;
  _contentInfoOverride = NULL;
  _controllerInfoCount = 0;
  _controllerInfo = NULL;
  _ports = NULL;
  _input->setKeyboardCallback(nullptr);
  memset(&_diskControlInterface, 0, sizeof(_diskControlInterface));
  memset(&_memoryMap, 0, sizeof(_memoryMap));
  memset(&_calls, 0, sizeof(_calls));
}

const char* libretro::Core::getLibretroPath() const
{
  return _libretroPath;
}

char* libretro::Core::strdup(const char* str)
{
  size_t len = strlen(str) + 1;
  char*  dup = (char*)_allocator->allocate(1, len);

  if (dup != NULL)
  {
    memcpy(dup, str, len);
  }

  return dup;
}

bool libretro::Core::setRotation(unsigned data)
{
  _video->setRotation((VideoComponent::Rotation)data);
  return true;
}

bool libretro::Core::getOverscan(bool* data) const
{
  *data = false;
  return true;
}

bool libretro::Core::getCanDupe(bool* data) const
{
  *data = true;
  return true;
}

bool libretro::Core::setMessage(const struct retro_message* data)
{
  _video->showMessage(data->msg, data->frames);
  return true;
}

void libretro::Core::setTrayOpen(bool open)
{
  if (_diskControlInterface.set_eject_state)
    _diskControlInterface.set_eject_state(open);
}

void libretro::Core::setCurrentDiscIndex(unsigned index)
{
  if (_diskControlInterface.set_image_index)
    _diskControlInterface.set_image_index(index);
}

bool libretro::Core::setPerformanceLevel(unsigned data)
{
  _logger->info(TAG "Performance level %u reported", data);
  _performanceLevel = data;
  return true;
}

bool libretro::Core::getSystemDirectory(const char** data) const
{
  *data = _config->getSystemPath();
  return true;
}

bool libretro::Core::setPixelFormat(enum retro_pixel_format data)
{
  switch (data)
  {
  case RETRO_PIXEL_FORMAT_0RGB1555:
    _logger->warn(TAG "Pixel format 0RGB1555 is deprecated");
    // fallthrough
  case RETRO_PIXEL_FORMAT_XRGB8888:
  case RETRO_PIXEL_FORMAT_RGB565:
     _pixelFormat = data;
    return true;
  
  case RETRO_PIXEL_FORMAT_UNKNOWN:
  default:
    _logger->error(TAG "Unsupported pixel format %u", data);
    return false;
  }
}

bool libretro::Core::setInputDescriptors(const struct retro_input_descriptor* data)
{
  struct retro_input_descriptor* desc;

  for (desc = (struct retro_input_descriptor*)data; desc->description != NULL; desc++)
  {
    // Just count.
  }

  _inputDescriptorsCount = desc - data;
  _inputDescriptors = alloc<struct retro_input_descriptor>(_inputDescriptorsCount);

  if (_inputDescriptors == NULL)
  {
    return false;
  }

  memcpy(_inputDescriptors, data, _inputDescriptorsCount * sizeof(_inputDescriptors[0]));
  desc = _inputDescriptors;

  for (unsigned i = 0; i < _inputDescriptorsCount; i++, desc++)
  {
    desc->description = strdup(desc->description);

    if (desc->description == NULL)
    {
      return false;
    }
  }

  _logger->debug(TAG "retro_input_descriptor");
  _logger->debug(TAG "  port device index id description");

  desc = _inputDescriptors;

  for (unsigned i = 0; i < _inputDescriptorsCount; i++, desc++)
  {
    _logger->debug(TAG "  %4u %6u %5u %2u %s", desc->port, desc->device, desc->index, desc->id, desc->description);
  }

  _input->setInputDescriptors(_inputDescriptors, _inputDescriptorsCount);
  return true;
}

bool libretro::Core::setKeyboardCallback(const struct retro_keyboard_callback* data)
{
  _input->setKeyboardCallback(data);
  return true;
}

bool libretro::Core::getDiskControlInterfaceVersion(unsigned* data)
{
  *data = 1;
  return true;
}

bool libretro::Core::setDiskControlInterface(const struct retro_disk_control_callback* data)
{
  memset(&_diskControlInterface, 0, sizeof(_diskControlInterface));
  if (data)
  {
    _diskControlInterface.set_eject_state = data->set_eject_state;
    _diskControlInterface.get_eject_state = data->get_eject_state;
    _diskControlInterface.get_image_index = data->get_image_index;
    _diskControlInterface.set_image_index = data->set_image_index;
    _diskControlInterface.get_num_images = data->get_num_images;
    _diskControlInterface.replace_image_index = data->replace_image_index;
    _diskControlInterface.add_image_index = data->add_image_index;
  }
  return true;
}

bool libretro::Core::setDiskControlExtInterface(const struct retro_disk_control_ext_callback* data)
{
  if (data)
    memcpy(&_diskControlInterface, data, sizeof(_diskControlInterface));
  else
    memset(&_diskControlInterface, 0, sizeof(_diskControlInterface));
  return true;
}

bool libretro::Core::getDiscLabel(unsigned index, std::string& label) const
{
  if (_diskControlInterface.get_image_label)
  {
    char buffer[256];
    if (_diskControlInterface.get_image_label(index, buffer, sizeof(buffer)))
    {
      label = buffer;
      return true;
    }
  }

  return false;
}

bool libretro::Core::getDiscPath(unsigned index, std::string& path) const
{
  if (_diskControlInterface.get_image_path)
  {
    char buffer[1024];
    if (_diskControlInterface.get_image_path(index, buffer, sizeof(buffer)))
    {
      path = buffer;
      return true;
    }
  }

  return false;
}

#ifndef _WINDOWS

bool libretro::Core::getVfsInterface(struct retro_vfs_interface_info* data)
{
  _logger->debug(TAG "Unimplemented env call: %s", name);
  return false;
}

#else

typedef struct retro_vfs_file_handle
{
  FILE* fp;
  char* orig_path;
} retro_vfs_file_handle;

static int retro_vfs_file_close_impl(retro_vfs_file_handle* stream)
{
  if (!stream)
    return -1;

  if (stream->fp)
    fclose(stream->fp);

  if (stream->orig_path)
    free(stream->orig_path);

  free(stream);
  return 0;
}

static retro_vfs_file_handle* retro_vfs_file_open_impl(const char* path, unsigned mode, unsigned hints)
{
  retro_vfs_file_handle* handle = (retro_vfs_file_handle*)calloc(1, sizeof(retro_vfs_file_handle));
  handle->orig_path = strdup(path);

  switch (mode)
  {
    case RETRO_VFS_FILE_ACCESS_READ:
      handle->fp = util::openFile(&app.logger(), path, "rb");
      break;

    case RETRO_VFS_FILE_ACCESS_WRITE:
      handle->fp = util::openFile(&app.logger(), path, "wb");
      break;

    case RETRO_VFS_FILE_ACCESS_READ_WRITE:
      handle->fp = util::openFile(&app.logger(), path, "w+b");
      break;

    case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
    case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
      handle->fp = util::openFile(&app.logger(), path, "r+b");
      break;
  }

  if (!handle->fp)
  {
    retro_vfs_file_close_impl(handle);
    return nullptr;
  }

  return handle;
}

static const char* retro_vfs_file_get_path_impl(retro_vfs_file_handle* stream)
{
  return (stream) ? stream->orig_path : nullptr;
}

static int64_t retro_vfs_file_tell_impl(retro_vfs_file_handle* stream)
{
  if (stream && stream->fp)
    return _ftelli64(stream->fp);

  return 0;
}

static int64_t retro_vfs_file_seek_impl(retro_vfs_file_handle* stream, int64_t offset, int seek_position)
{
  if (!stream)
    return -1;

  int whence = -1;
  switch (seek_position)
  {
    case RETRO_VFS_SEEK_POSITION_START:
      whence = SEEK_SET;
      break;
    case RETRO_VFS_SEEK_POSITION_CURRENT:
      whence = SEEK_CUR;
      break;
    case RETRO_VFS_SEEK_POSITION_END:
      whence = SEEK_END;
      break;
  }

  return _fseeki64(stream->fp, offset, whence);
}

static int64_t retro_vfs_file_size_impl(retro_vfs_file_handle* stream)
{
  if (stream && stream->fp)
  {
    const int64_t pos = _ftelli64(stream->fp);
    _fseeki64(stream->fp, 0, SEEK_END);

    const int64_t size = _ftelli64(stream->fp);

    _fseeki64(stream->fp, pos, SEEK_SET);
    return size;
  }

  return 0;
}
static int64_t retro_vfs_file_read_impl(retro_vfs_file_handle* stream, void* buffer, uint64_t len)
{
  if (!stream || !buffer)
    return -1;

  return fread(buffer, 1, len, stream->fp);
}

static int64_t retro_vfs_file_write_impl(retro_vfs_file_handle* stream, const void* buffer, uint64_t len)
{
  if (!stream || !buffer)
    return -1;

  return fwrite(buffer, 1, len, stream->fp);
}

static int retro_vfs_file_flush_impl(retro_vfs_file_handle* stream)
{
  if (!stream || fflush(stream->fp) != 0)
    return -1;

  return 0;
}

static int retro_vfs_file_remove_impl(const char* path)
{
  util::deleteFile(path);
  return 0;
}

static int retro_vfs_file_rename_impl(const char* old_path, const char* new_path)
{
  std::wstring unicodeOldPath = util::utf8ToUChar(old_path);
  std::wstring unicodeNewPath = util::utf8ToUChar(new_path);
  return (_wrename(unicodeOldPath.c_str(), unicodeNewPath.c_str()) == 0) ? 0 : -1;
}

int64_t retro_vfs_file_truncate_impl(retro_vfs_file_handle* stream, int64_t length)
{
  return (stream && _chsize_s(_fileno(stream->fp), length) == 0) ? 0 : -1;
}

bool libretro::Core::getVfsInterface(struct retro_vfs_interface_info* data)
{
  const uint32_t supported_vfs_version = 2;
  static struct retro_vfs_interface vfs_iface =
  {
    /* VFS API v1 */
    retro_vfs_file_get_path_impl,
    retro_vfs_file_open_impl,
    retro_vfs_file_close_impl,
    retro_vfs_file_size_impl,
    retro_vfs_file_tell_impl,
    retro_vfs_file_seek_impl,
    retro_vfs_file_read_impl,
    retro_vfs_file_write_impl,
    retro_vfs_file_flush_impl,
    retro_vfs_file_remove_impl,
    retro_vfs_file_rename_impl,
    /* VFS API v2 */
    retro_vfs_file_truncate_impl,
    /* VFS API v3 */
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
  };

  if (data->required_interface_version > supported_vfs_version)
  {
    _logger->warn(TAG "%s: requested version %d, only have version %d",
      "GET_VFS_INTERFACE", data->required_interface_version, supported_vfs_version);
    return false;
  }

  _logger->info(TAG "%s: requested version %d, providing version %d",
    "GET_VFS_INTERFACE", data->required_interface_version, supported_vfs_version);
  data->required_interface_version = supported_vfs_version;
  data->iface = &vfs_iface;
  return true;
}

#endif

bool libretro::Core::getLEDInterface(struct retro_led_interface* data)
{
  data->set_led_state = &s_setLEDState;
  return true;
}

void libretro::Core::setLEDState(int led, int state)
{
}

bool libretro::Core::getMicrophoneInterface(struct retro_microphone_interface* data)
{
  if (data->interface_version != 1)
    return false;

  memset(data, 0, sizeof(*data));
  data->interface_version = 1;
  data->open_mic = s_openMic;
  data->close_mic = s_closeMic;
  data->get_params = s_getMicParams;
  data->set_mic_state = s_setMicState;
  data->get_mic_state = s_getMicState;
  data->read_mic = s_readMic;
  return true;
}

bool libretro::Core::setContentInfoOverride(const struct retro_system_content_info_override* data)
{
  _contentInfoOverride = alloc<struct retro_system_content_info_override>(1);
  if (!_contentInfoOverride)
    return false;

  _logger->info(TAG "retro_system_content_info_override");
  _logger->info(TAG "  %s need_fullpath=%d persistent_data=%d", data->extensions, data->need_fullpath, data->persistent_data);

  memcpy(_contentInfoOverride, data, sizeof(*_contentInfoOverride));
  _contentInfoOverride->extensions = strdup(data->extensions);
  return true;
}

static bool extensionMatches(const char* extensions, const std::string& extension)
{
  const char* ptr = extensions;
  const char* start = ptr;

  while (*ptr)
  {
    while (*ptr && *ptr != '|')
      ++ptr;

    if ((size_t)(ptr - start) == extension.length() && strnicmp(start, extension.c_str(), extension.length()) == 0)
      return true;

    if (*ptr == '|')
      start = ++ptr;
  }

  return false;
}

bool libretro::Core::getNeedsFullPath(const std::string& extension) const
{
  if (_contentInfoOverride && extensionMatches(_contentInfoOverride->extensions, extension))
    return _contentInfoOverride->need_fullpath;

  /* never pass a buffered m3u to the core, even if the core suggests that's okay. */
  if (extensionMatches("m3u", extension))
    return true;

  return _systemInfo.need_fullpath;
}

bool libretro::Core::getPersistData(const std::string& extension) const
{
  if (_contentInfoOverride && extensionMatches(_contentInfoOverride->extensions, extension))
    return _contentInfoOverride->persistent_data;

  return false;
}

bool libretro::Core::getPreferredHWRender(unsigned* data)
{
  *data = RETRO_HW_CONTEXT_OPENGL_CORE;
  return true;
}

bool libretro::Core::setHWRender(struct retro_hw_render_callback* data)
{
  static const char* context_type_desc[] =
  {
    "RETRO_HW_CONTEXT_NONE",
    "RETRO_HW_CONTEXT_OPENGL",
    "RETRO_HW_CONTEXT_OPENGLES2",
    "RETRO_HW_CONTEXT_OPENGL_CORE",
    "RETRO_HW_CONTEXT_OPENGLES3",
    "RETRO_HW_CONTEXT_OPENGLES_VERSION",
    "RETRO_HW_CONTEXT_VULKAN",
  };
  
  const char* context_type = "?";
  
  if (data->context_type >= 0 && data->context_type < sizeof(context_type_desc) / sizeof(context_type_desc[0]))
  {
    context_type = context_type_desc[data->context_type];
  }
  
  if (!_video->supportsContext(data->context_type))
  {
    _logger->error(TAG "Context type not supported: %s", context_type);
    return false;
  }

  data->get_current_framebuffer = s_getCurrentFramebuffer;
  data->get_proc_address = s_getProcAddress;
  
  _logger->debug(TAG "retro_hw_render_callback");
  _logger->debug(TAG "  context_type:            %s", context_type);
  _logger->debug(TAG "  context_reset:           %p", data->context_reset);
  _logger->debug(TAG "  get_current_framebuffer: %p", data->get_current_framebuffer);
  _logger->debug(TAG "  get_proc_address:        %p", data->get_proc_address);
  _logger->debug(TAG "  depth:                   %s", data->depth ? "true" : "false");
  _logger->debug(TAG "  stencil:                 %s", data->stencil ? "true" : "false");
  _logger->debug(TAG "  bottom_left_origin:      %s", data->bottom_left_origin ? "true" : "false");
  _logger->debug(TAG "  version_major:           %u", data->version_major);
  _logger->debug(TAG "  version_minor:           %u", data->version_minor);
  _logger->debug(TAG "  cache_context:           %s", data->cache_context ? "true" : "false");
  _logger->debug(TAG "  context_destroy:         %p", data->context_destroy);
  _logger->debug(TAG "  debug_context:           %s", data->debug_context ? "true" : "false");
  
  _hardwareRenderCallback = *data;
  _needsHardwareRender = true;
  return true;
}

bool libretro::Core::getVariable(struct retro_variable* data)
{
  data->value = _config->getVariable(data->key);
  return data->value != NULL;
}

bool libretro::Core::setVariables(const struct retro_variable* data)
{
  const struct retro_variable* var;
  unsigned count = 0;

  _logger->debug(TAG "retro_variable");
  for (var = data; var->key != NULL; var++)
    _logger->debug(TAG "  %s: %s", var->key, var->value);

  count = var - data;
  _config->setVariables(data, count);

  return true;
}

bool libretro::Core::getVariableUpdate(bool* data)
{
  *data = _config->varUpdated();
  return true;
}

bool libretro::Core::setSupportNoGame(bool data)
{
  _supportsNoGame = data;
  return true;
}

bool libretro::Core::getLibretroPath(const char** data) const
{
  *data = getLibretroPath();
  return true;
}

bool libretro::Core::getRumbleInterface(struct retro_rumble_interface* data) const
{
  data->set_rumble_state = s_setRumbleCallback;
  return true;
}

bool libretro::Core::getInputDeviceCapabilities(uint64_t* data) const
{
  *data = (
    (1 << RETRO_DEVICE_JOYPAD)
    | (1 << RETRO_DEVICE_ANALOG)
    | (1 << RETRO_DEVICE_MOUSE)
    | (1 << RETRO_DEVICE_KEYBOARD)
  );
  return false;
}

bool libretro::Core::getLogInterface(struct retro_log_callback* data) const
{
  data->log = s_logCallback;
  return true;
}

bool libretro::Core::getCoreAssetsDirectory(const char** data) const
{
  *data = _config->getCoreAssetsDirectory();
  return true;
}

bool libretro::Core::getSaveDirectory(const char** data) const
{
  *data = _config->getSaveDirectory();
  util::ensureDirectoryExists(*data);
  return true;
}

bool libretro::Core::setSystemAVInfo(const struct retro_system_av_info* data)
{
  _systemAVInfo = *data;

  return handleSystemAVInfoChanged();
}

bool libretro::Core::handleSystemAVInfoChanged()
{
  _logger->debug(TAG "retro_system_av_info");
  _logger->debug(TAG "  base_width   = %u", _systemAVInfo.geometry.base_width);
  _logger->debug(TAG "  base_height  = %u", _systemAVInfo.geometry.base_height);
  _logger->debug(TAG "  max_width    = %u", _systemAVInfo.geometry.max_width);
  _logger->debug(TAG "  max_height   = %u", _systemAVInfo.geometry.max_height);
  _logger->debug(TAG "  aspect_ratio = %f", _systemAVInfo.geometry.aspect_ratio);
  _logger->debug(TAG "  fps          = %f", _systemAVInfo.timing.fps);
  _logger->debug(TAG "  sample_rate  = %f", _systemAVInfo.timing.sample_rate);

  if (_systemAVInfo.geometry.aspect_ratio <= 0.0f)
  {
    _systemAVInfo.geometry.aspect_ratio = (float)_systemAVInfo.geometry.base_width / (float)_systemAVInfo.geometry.base_height;
  }

  const struct retro_hw_render_callback* cb = _needsHardwareRender ? &_hardwareRenderCallback : NULL;

  if (!_video->setGeometry(_systemAVInfo.geometry.base_width, _systemAVInfo.geometry.base_height, _systemAVInfo.geometry.max_width, _systemAVInfo.geometry.max_height, _systemAVInfo.geometry.aspect_ratio, _pixelFormat, cb))
  {
    return false;
  }

  if (_needsHardwareRender)
  {
    _hardwareRenderCallback.context_reset();
  }

  if (!_audio->setRate(_systemAVInfo.timing.sample_rate))
  {
    return false;
  }

  resetVsync();

  return true;
}

void libretro::Core::resetVsync()
{
  SDL_DisplayMode displayMode;
  int monitorRefreshRate = 60; // assume 60Hz if we can't get an actual value
  if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0 && displayMode.refresh_rate > 0)
  {
    monitorRefreshRate = displayMode.refresh_rate;
  }

  // give two fps of leniency so things like 60.1 NES will run on 59.97 monitors
  if (_systemAVInfo.timing.fps > monitorRefreshRate + 2)
  {
    // requested refresh rate is greater than monitor refresh rate, disable vsync
    SDL_GL_SetSwapInterval(0);
  }
  else
  {
    // requested refresh rate is greater than monitor refresh rate, disable vsync
    SDL_GL_SetSwapInterval(1);
  }
}

bool libretro::Core::setSubsystemInfo(const struct retro_subsystem_info* data)
{
  struct retro_subsystem_info* info;
  
  for (info = (struct retro_subsystem_info*)data; info->desc != NULL; info++)
  {
    // Just count.
  }

  _subsystemInfoCount = info - data;
  _subsystemInfo = alloc<struct retro_subsystem_info>(_subsystemInfoCount);

  if (_subsystemInfo == NULL)
  {
    return false;
  }

  memcpy(_subsystemInfo, data, _subsystemInfoCount * sizeof(*_subsystemInfo));
  info = _subsystemInfo;

  for (unsigned i = 0; i < _subsystemInfoCount; i++, info++)
  {
    struct retro_subsystem_rom_info* roms = alloc<struct retro_subsystem_rom_info>(info->num_roms);
    
    if (roms == NULL)
    {
      return false;
    }
    
    memcpy(roms, info->roms, info->num_roms * sizeof(*roms));

    info->desc = strdup(info->desc);
    info->ident = strdup(info->ident);
    info->roms = roms;

    if (info->desc == NULL || info->ident == NULL)
    {
      return false;
    }

    for (unsigned j = 0; j < info->num_roms; j++, roms++)
    {
      struct retro_subsystem_memory_info* memory = alloc<struct retro_subsystem_memory_info>(roms->num_memory);
      
      if (memory == NULL)
      {
        return false;
      }
      
      memcpy(memory, roms->memory, roms->num_memory * sizeof(*memory));

      roms->desc = strdup(roms->desc);
      roms->valid_extensions = strdup(roms->valid_extensions);
      roms->memory = memory;

      if (roms->desc == NULL || roms->valid_extensions == NULL)
      {
        return false;
      }

      for (unsigned k = 0; k < roms->num_memory; k++, memory++)
      {
        memory->extension = strdup(memory->extension);

        if (memory->extension == NULL)
        {
          return false;
        }
      }
    }
  }

  _logger->debug(TAG "retro_subsystem_info");

  info = (struct retro_subsystem_info*)data;

  for (unsigned i = 0; i < _subsystemInfoCount; i++, info++)
  {
    _logger->debug(TAG "  desc  = %s", info->desc);
    _logger->debug(TAG "  ident = %s", info->ident);
    _logger->debug(TAG "  id    = %u", info->id);
    
    const struct retro_subsystem_rom_info* roms = info->roms;

    for (unsigned j = 0; j < info->num_roms; j++, roms++)
    {
      _logger->debug(TAG "    roms[%u].desc             = %s", j, roms->desc);
      _logger->debug(TAG "    roms[%u].valid_extensions = %s", j, roms->valid_extensions);
      _logger->debug(TAG "    roms[%u].need_fullpath    = %d", j, roms->need_fullpath);
      _logger->debug(TAG "    roms[%u].block_extract    = %d", j, roms->block_extract);
      _logger->debug(TAG "    roms[%u].required         = %d", j, roms->required);
      
      const struct retro_subsystem_memory_info* memory = roms->memory;

      for (unsigned k = 0; k < info->roms[j].num_memory; k++, memory++)
      {
        _logger->debug(TAG "      memory[%u].type         = %u", k, memory->type);
        _logger->debug(TAG "      memory[%u].extension    = %s", k, memory->extension);
      }
    }
  }

  return true;
}

bool libretro::Core::setControllerInfo(const struct retro_controller_info* data)
{
  struct retro_controller_info* info;

  for (info = (struct retro_controller_info*)data; info->types != NULL; info++)
  {
    // Just count.
  }

  _controllerInfoCount = info - data;
  _controllerInfo = alloc<struct retro_controller_info>(_controllerInfoCount);
  _ports = alloc<unsigned>(_controllerInfoCount);

  if (_controllerInfo == NULL || _ports == NULL)
  {
    return false;
  }

  memcpy(_controllerInfo, data, _controllerInfoCount * sizeof(*_controllerInfo));
  info = _controllerInfo;

  for (unsigned i = 0; i < _controllerInfoCount; i++, info++)
  {
    struct retro_controller_description* types = alloc<struct retro_controller_description>(info->num_types);

    if (types == NULL)
    {
      return false;
    }

    memcpy(types, info->types, info->num_types * sizeof(*types));

    struct retro_controller_description* type = types;
    const struct retro_controller_description* end = type + info->num_types;

    for (; type < end && type->desc != NULL; type++)
    {
      type->desc = strdup(type->desc);

      if (type->desc == NULL)
      {
        return false;
      }
    }

    info->num_types = type - types;
    _ports[i] = RETRO_DEVICE_NONE;
  }

  _logger->debug(TAG "retro_controller_info");
  _logger->debug(TAG "  port id   desc");

  info = _controllerInfo;

  for (unsigned i = 0; i < _controllerInfoCount; i++, info++)
  {
    const struct retro_controller_description* type = (struct retro_controller_description*)info->types;
    const struct retro_controller_description* end = type + info->num_types;

    for (; type < end; type++)
    {
      _logger->debug(TAG "  %4u %04x %s", i, type->id, type->desc);
    }
  }

  _input->setControllerInfo(_controllerInfo, _controllerInfoCount);
  return true;
}

static size_t fill_bits(size_t n)
{
  /* ensures all bits in a value after the first set bit are 1. i.e. 00101000 -> 00111111 */
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 16 >> 16; /* double shift to avoid warning on 32-bit compilers */
  return n;
}

bool libretro::Core::setMemoryMaps(const struct retro_memory_map* data)
{
  _memoryMap.num_descriptors = data->num_descriptors;
  struct retro_memory_descriptor* descriptors = alloc<struct retro_memory_descriptor>(data->num_descriptors);

  if (descriptors == NULL)
  {
    return false;
  }

  _memoryMap.descriptors = descriptors;
  memcpy(descriptors, data->descriptors, _memoryMap.num_descriptors * sizeof(*descriptors));
  struct retro_memory_descriptor* desc = descriptors;

  for (size_t i = 0; i < _memoryMap.num_descriptors; i++, desc++)
  {
    if (desc->addrspace != NULL)
    {
      desc->addrspace = strdup(desc->addrspace);

      if (desc->addrspace == NULL)
      {
        return false;
      }
    }
    else
    {
      desc->addrspace = NULL;
    }

    if (desc->ptr && desc->len == 0 && desc->select) {
      /* pointer was provided, but length was not specified. calculate length. */
      const size_t top_addr = fill_bits(desc->select);
      const size_t live_bits = top_addr & ~desc->select;

      /* remove any bits from the reduced_address that correspond to the bits in the disconnect
       * mask and collapse the remaining bits. this code was copied from the mmap_reduce function
       * in RetroArch. i'm not exactly sure how it works, but it does. */
      size_t reduced_address = live_bits;
      size_t disconnect_mask = desc->disconnect;
      while (disconnect_mask) {
        const size_t tmp = (disconnect_mask - 1) & ~disconnect_mask;
        reduced_address = (reduced_address & tmp) | ((reduced_address >> 1) & ~tmp);
        disconnect_mask = (disconnect_mask & (disconnect_mask - 1)) >> 1;
      }

      desc->len = reduced_address + 1;
    }
  }

  _logger->debug(TAG "retro_memory_map");
  _logger->debug(TAG "  ndx flags  ptr      offset   start    select   disconn  len      addrspace");

  const struct retro_memory_descriptor* end = descriptors + _memoryMap.num_descriptors;

  for (unsigned i = 1; descriptors < end; i++, descriptors++)
  {
    char flags[7];

    flags[0] = 'M';

    if ((descriptors->flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
    {
      flags[1] = '8';
    }
    else if ((descriptors->flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
    {
      flags[1] = '4';
    }
    else if ((descriptors->flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
    {
      flags[1] = '2';
    }
    else
    {
      flags[1] = '1';
    }

    flags[2] = 'A';

    if ((descriptors->flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
    {
      flags[3] = '8';
    }
    else if ((descriptors->flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
    {
      flags[3] = '4';
    }
    else if ((descriptors->flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
    {
      flags[3] = '2';
    }
    else
    {
      flags[3] = '1';
    }

    flags[4] = (descriptors->flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
    flags[5] = (descriptors->flags & RETRO_MEMDESC_CONST) ? 'C' : 'c';
    flags[6] = 0;

    _logger->debug(TAG "  %3u %s %p %08X %08X %08X %08X %08X %s", i, flags, descriptors->ptr, descriptors->offset, descriptors->start, descriptors->select, descriptors->disconnect, descriptors->len, descriptors->addrspace ? descriptors->addrspace : "");
  }

  if (app.isGameActive())
    app.refreshMemoryMap();

  return true;
}

bool libretro::Core::setGeometry(const struct retro_game_geometry* data)
{
  _systemAVInfo.geometry.base_width = data->base_width;
  _systemAVInfo.geometry.base_height = data->base_height;
  _systemAVInfo.geometry.aspect_ratio = data->aspect_ratio;

  _logger->debug(TAG "retro_game_geometry");
  _logger->debug(TAG "  base_width   = %u", data->base_width);
  _logger->debug(TAG "  base_height  = %u", data->base_height);
  _logger->debug(TAG "  aspect_ratio = %f", data->aspect_ratio);

  if (_systemAVInfo.geometry.aspect_ratio <= 0.0f)
  {
    _systemAVInfo.geometry.aspect_ratio = (float)_systemAVInfo.geometry.base_width / _systemAVInfo.geometry.base_height;
  }

  const struct retro_hw_render_callback* cb = _needsHardwareRender ? &_hardwareRenderCallback : NULL;

  if (!_video->setGeometry(_systemAVInfo.geometry.base_width, _systemAVInfo.geometry.base_height, _systemAVInfo.geometry.max_width, _systemAVInfo.geometry.max_height, _systemAVInfo.geometry.aspect_ratio, _pixelFormat, cb))
  {
    return false;
  }

  return true;
}

bool libretro::Core::getUsername(const char** data) const
{
#ifdef _WINDOWS
  *data = RA_UserName();
  return true;
#else
  return false;
#endif
}

bool libretro::Core::getLanguage(unsigned* data) const
{
  *data = RETRO_LANGUAGE_ENGLISH;
  return true;
}

bool libretro::Core::setSupportAchievements(bool data)
{
  _supportAchievements = data;
  return true;
}

bool libretro::Core::getFastForwarding(bool* data)
{
  *data = _config->getFastForwarding();
  return true;
}

bool libretro::Core::getInputBitmasks(bool* data)
{
  // The documentation says the parameter is a [bool * that indicates whether or not the
  // frontend supports input bitmasks being returned by retro_input_state_t], but in
  // practice, the parameter is ignored and the core just looks at the return value.
  (void)data;
  return true;
}

bool libretro::Core::getCoreOptionsVersion(unsigned* data) const
{
  *data = 2;
  return true;
}

bool libretro::Core::setCoreOptions(const struct retro_core_option_definition* data)
{
  const struct retro_core_option_definition* option;
  unsigned count;

  _logger->debug(TAG "retro_options");
  for (option = data; option->key != NULL; option++)
    _logger->debug(TAG "  %s: %s", option->key, option->desc);

  count = option - data;
  _config->setVariables(data, count);

  return true;
}

bool libretro::Core::setCoreOptionsIntl(const struct retro_core_options_intl* data)
{
  return setCoreOptions(data->us);
}

bool libretro::Core::setCoreOptionsV2(const struct retro_core_options_v2* data)
{
  const struct retro_core_option_v2_definition* option;
  const struct retro_core_option_v2_category* category;
  unsigned count, category_count;

  for (category = data->categories; category->key != NULL; category++)
    ;
  category_count = category - data->categories;

  _logger->debug(TAG "retro_options_v2");
  for (option = data->definitions; option->key != NULL; option++)
    _logger->debug(TAG "  %s: %s", option->key, option->desc);

  count = option - data->definitions;

  _config->setVariables(data->definitions, count, data->categories, category_count);

  return true;
}

bool libretro::Core::setCoreOptionsV2Intl(const struct retro_core_options_v2_intl* data)
{
  return setCoreOptionsV2(data->us);
}

bool libretro::Core::setCoreOptionsDisplay(const struct retro_core_option_display* data)
{
  _config->setVariableDisplay(data);
  return true;
}

static bool clearAllWaitThreadsCallback(unsigned clear_threads, void* data)
{
  return true;
}

static void getEnvName(char* name, size_t size, unsigned cmd)
{
  static const char* names[] =
  {
    "0",
    "SET_ROTATION",
    "GET_OVERSCAN",
    "GET_CAN_DUPE",
    "4",
    "5",
    "SET_MESSAGE",
    "SHUTDOWN",
    "SET_PERFORMANCE_LEVEL",
    "GET_SYSTEM_DIRECTORY",
    "SET_PIXEL_FORMAT",                        // 10
    "SET_INPUT_DESCRIPTORS",
    "SET_KEYBOARD_CALLBACK",
    "SET_DISK_CONTROL_INTERFACE",
    "SET_HW_RENDER",
    "GET_VARIABLE",
    "SET_VARIABLES",
    "GET_VARIABLE_UPDATE",
    "SET_SUPPORT_NO_GAME",
    "GET_LIBRETRO_PATH",
    "20",                                      // 20
    "SET_FRAME_TIME_CALLBACK",
    "SET_AUDIO_CALLBACK",
    "GET_RUMBLE_INTERFACE",
    "GET_INPUT_DEVICE_CAPABILITIES",
    "GET_SENSOR_INTERFACE",
    "GET_CAMERA_INTERFACE",
    "GET_LOG_INTERFACE",
    "GET_PERF_INTERFACE",
    "GET_LOCATION_INTERFACE",
    "GET_CORE_ASSETS_DIRECTORY",               // 30
    "GET_SAVE_DIRECTORY",
    "SET_SYSTEM_AV_INFO",
    "SET_PROC_ADDRESS_CALLBACK",
    "SET_SUBSYSTEM_INFO",
    "SET_CONTROLLER_INFO",
    "SET_MEMORY_MAPS",
    "SET_GEOMETRY",
    "GET_USERNAME",
    "GET_LANGUAGE",
    "GET_CURRENT_SOFTWARE_FRAMEBUFFER",        // 40
    "GET_HW_RENDER_INTERFACE",
    "SET_SUPPORT_ACHIEVEMENTS",
    "SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE",
    "SET_SERIALIZATION_QUIRKS",
    "GET_VFS_INTERFACE",
    "GET_LED_INTERFACE",
    "GET_AUDIO_VIDEO_ENABLE",
    "GET_MIDI_INTERFACE",
    "GET_FASTFORWARDING",
    "GET_TARGET_REFRESH_RATE",                 // 50
    "GET_INPUT_BITMASKS",
    "GET_CORE_OPTIONS_VERSION",
    "SET_CORE_OPTIONS",
    "SET_CORE_OPTIONS_INTL",
    "SET_CORE_OPTIONS_DISPLAY",
    "GET_PREFERRED_HW_RENDER",
    "GET_DISK_CONTROL_INTERFACE_VERSION",
    "SET_DISK_CONTROL_EXT_INTERFACE",
    "GET_MESSAGE_INTERFACE_VERSION",
    "SET_MESSAGE_EXT",                         // 60
    "GET_INPUT_MAX_USERS",
    "SET_AUDIO_BUFFER_STATUS_CALLBACK",
    "SET_MINIMUM_AUDIO_LATENCY",
    "SET_FASTFORWARDING_OVERRIDE",
    "SET_CONTENT_INFO_OVERRIDE",
    "GET_GAME_INFO_EXT",
    "SET_CORE_OPTIONS_V2",
    "SET_CORE_OPTIONS_V2_INTL",
    "SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK",
    "SET_VARIABLE",                            // 70
    "GET_THROTTLE_STATE",
    "GET_SAVESTATE_CONTEXT",
    "GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT",
    "GET_JIT_CAPABLE",
    "GET_MICROPHONE_INTERFACE",
    "SET_NETPACKET_INTERFACE",
    "GET_DEVICE_POWER",
  };

  cmd &= ~RETRO_ENVIRONMENT_EXPERIMENTAL;

  if (cmd < sizeof(names) / sizeof(names[0]))
  {
    snprintf(name, size, "%s (%u)", names[cmd], cmd);
  }
  else
  {
    snprintf(name, size, "%u", cmd);
  }
}

bool libretro::Core::environmentCallback(unsigned cmd, void* data)
{
  bool ret;
  char name[128];

  getEnvName(name, sizeof(name), cmd);
  _logger->debug(TAG "Calling %s", name);

  switch (cmd)
  {
  case RETRO_ENVIRONMENT_SET_ROTATION:
    ret = setRotation(*(const unsigned*)data);
    break;

  case RETRO_ENVIRONMENT_GET_OVERSCAN:
    ret = getOverscan((bool*)data);
    break;

  case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    ret = getCanDupe((bool*)data);
    break;

  case RETRO_ENVIRONMENT_SET_MESSAGE:
    ret = setMessage((const struct retro_message*)data);
    break;

  case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    ret = setPerformanceLevel(*(unsigned*)data);
    break;

  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    ret = getSystemDirectory((const char**)data);
    break;

  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    ret = setPixelFormat(*(enum retro_pixel_format*)data);
    break;

  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    ret = setInputDescriptors((const struct retro_input_descriptor*)data);
    break;

  case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
    ret = setKeyboardCallback((const struct retro_keyboard_callback*)data);
    break;

  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    ret = setDiskControlInterface((const struct retro_disk_control_callback*)data);
    break;

  case RETRO_ENVIRONMENT_SET_HW_RENDER:
    ret = setHWRender((struct retro_hw_render_callback*)data);
    break;

  case RETRO_ENVIRONMENT_GET_VARIABLE:
    ret = getVariable((struct retro_variable*)data);
    break;

  case RETRO_ENVIRONMENT_SET_VARIABLES:
    ret = setVariables((const struct retro_variable*)data);
    break;

  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
    ret = getVariableUpdate((bool*)data);
    break;

  case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    ret = setSupportNoGame(*(bool*)data);
    break;

  case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
    ret = getLibretroPath((const char**)data);
    break;

  case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    ret = getRumbleInterface((struct retro_rumble_interface*)data);
    break;

  case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
    ret = getInputDeviceCapabilities((uint64_t*)data);
    break;

  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    ret = getLogInterface((struct retro_log_callback*)data);
    break;

  case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
    ret = getCoreAssetsDirectory((const char**)data);
    break;

  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    ret = getSaveDirectory((const char**)data);
    break;

  case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    ret = setSystemAVInfo((const struct retro_system_av_info*)data);
    break;

  case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    ret = setSubsystemInfo((const struct retro_subsystem_info*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    ret = setControllerInfo((const struct retro_controller_info*)data);
    break;

  case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    ret = setMemoryMaps((const struct retro_memory_map*)data);
    break;

  case RETRO_ENVIRONMENT_SET_GEOMETRY:
    ret = setGeometry((const struct retro_game_geometry*)data);
    break;

  case RETRO_ENVIRONMENT_GET_USERNAME:
    ret = getUsername((const char**)data);
    break;

  case RETRO_ENVIRONMENT_GET_LANGUAGE:
    ret = getLanguage((unsigned*)data);
    break;

  case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    ret = setSupportAchievements(*(bool*)data);
    break;

  case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
    ret = getVfsInterface((struct retro_vfs_interface_info*)data);
    break;

  case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
    ret = getLEDInterface((struct retro_led_interface*)data);
    break;

  case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    ret = getInputBitmasks((bool*)data);
    break;

  case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
    ret = getCoreOptionsVersion((unsigned*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    ret = setCoreOptions((const struct retro_core_option_definition*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    ret = setCoreOptionsIntl((const struct retro_core_options_intl*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    ret = setCoreOptionsDisplay((const struct retro_core_option_display*)data);
    break;

  case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
    ret = getFastForwarding((bool*)data);
    break;

  case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
    ret = getPreferredHWRender((unsigned*)data);
    break;

  case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
    ret = getDiskControlInterfaceVersion((unsigned*)data);
    break;

  case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
    ret = setDiskControlExtInterface((const struct retro_disk_control_ext_callback*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
    ret = setContentInfoOverride((const struct retro_system_content_info_override*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    ret = setCoreOptionsV2((const struct retro_core_options_v2*)data);
    break;

  case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    ret = setCoreOptionsV2Intl((const struct retro_core_options_v2_intl*)data);
    break;

  case RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE:
    ret = getMicrophoneInterface((struct retro_microphone_interface*)data);
    break;

  /* RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK cannot be supported because
   * we don't update the variable values in real time. values are only updated when the config
   * dialog is closed.
   */

  case RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND:
    _logger->warn(TAG "Unimplemented env call: %s", "RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND");
    return false;

  case RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB:
    /* flycast crashes on exit unless this provides a valid callback. This is just a
     * dummy function, so we're still going to log it like it's unimplemented. */
    _logger->warn(TAG "Unimplemented env call: %s", "RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB");
    *(retro_environment_t*)data = clearAllWaitThreadsCallback;
    return false;

  case RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE:
    _logger->warn(TAG "Unimplemented env call: %s", "RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE");
    return false;

  default:
    /* we don't care about private events */
    if (cmd & RETRO_ENVIRONMENT_PRIVATE)
      return false;

    cmd &= ~RETRO_ENVIRONMENT_EXPERIMENTAL;
    if (cmd < sizeof(_calls) * 8)
    {
      /* only log the unimplemented error once per core */
      if ((_calls[cmd / 8] & (1 << (cmd & 7))) == 0)
      {
        _logger->warn(TAG "Unimplemented env call: %s", name);
        _calls[cmd / 8] |= 1 << (cmd & 7);
      }
    }
    else
    {
      _logger->debug(TAG "Unimplemented env call: %s", name);
    }

    return false;
  }

  if (ret)
  {
    _logger->debug(TAG "Called  %s -> %d", name, ret);
  }
  else
  {
    _logger->warn(TAG "Called  %s -> %d", name, ret);
  }

  return ret;
}

void libretro::Core::videoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch)
{
  _video->refresh(data, width, height, pitch);
}

size_t libretro::Core::audioSampleBatchCallback(const int16_t* data, size_t frames)
{
  if (_generateAudio)
  {
    /* RetroArch resamples every packet as it comes in. To minimize the overhead of
     * resampling (and reduce the scratchiness of rounding at boundaries), we buffer
     * up to SAMPLE_COUNT samples (2 samples per frame) before resampling. Normally,
     * fewer than SAMPLE_COUNT samples will be generated per frame and the flush in
     * Core::step() will process a single frame's worth of audio. */
    if (_samplesCount == 0 && frames >= SAMPLE_COUNT / 2)
    {
      /* buffer is empty. more samples were provided than would fit in buffer. just
       * pass the samples directly to the mixer */
      _audio->mix(data, frames);
    }
    else
    {
      size_t framesAvailable = (SAMPLE_COUNT - _samplesCount) / 2;
      if (framesAvailable > frames)
      {
        /* less samples than would fill the buffer, just copy them */
        memcpy(_samples + _samplesCount, data, frames * 2 * sizeof(int16_t));
        _samplesCount += frames * 2;
      }
      else
      {
        /* fill the remaining portion of the buffer and flush it */
        memcpy(_samples + _samplesCount, data, framesAvailable * 2 * sizeof(int16_t));
        _audio->mix(_samples, SAMPLE_COUNT / 2);
        _samplesCount = 0;

        data += framesAvailable * 2;
        const size_t framesRemaining = frames - framesAvailable;
        if (framesRemaining > SAMPLE_COUNT / 2)
        {
          /* remaining frames still don't fit into buffer, send them to the mixer */
          _audio->mix(data, framesRemaining);
        }
        else if (framesRemaining > 0)
        {
          /* capture the remaining frames for later */
          memcpy(_samples, data, framesRemaining * 2 * sizeof(int16_t));
          _samplesCount = framesRemaining * 2;
        }
      }
    }
  }
  else
  {
    /* audio disabled, clear the buffer */
    _samplesCount = 0;
  }

  /* say we processed all provided frames */
  return frames;
}

void libretro::Core::audioSampleCallback(int16_t left, int16_t right)
{
  if (_samplesCount < SAMPLE_COUNT - 1)
  {
    _samples[_samplesCount++] = left;
    _samples[_samplesCount++] = right;
  }
  else
  {
    /* buffer full, flush it */
    _audio->mix(_samples, SAMPLE_COUNT / 2);

    _samples[0] = left;
    _samples[1] = right;
    _samplesCount = 2;
  }
}

int16_t libretro::Core::inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id)
{
  return _input->read(port, device, index, id);
}

void libretro::Core::inputPollCallback()
{
  return _input->poll();
}

uintptr_t libretro::Core::getCurrentFramebuffer()
{
  return _video->getCurrentFramebuffer();
}

retro_proc_address_t libretro::Core::getProcAddress(const char* symbol)
{
  return _video->getProcAddress(symbol);
}

void libretro::Core::logCallback(enum retro_log_level level, const char *fmt, va_list args)
{
  _logger->vprintf(level, fmt, args);
}

bool libretro::Core::s_environmentCallback(unsigned cmd, void* data)
{
  if (s_instance)
    return s_instance->environmentCallback(cmd, data);

  return false;
}

void libretro::Core::s_videoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch)
{
  s_instance->videoRefreshCallback(data, width, height, pitch);
}

size_t libretro::Core::s_audioSampleBatchCallback(const int16_t* data, size_t frames)
{
  return s_instance->audioSampleBatchCallback(data, frames);
}

void libretro::Core::s_audioSampleCallback(int16_t left, int16_t right)
{
  s_instance->audioSampleCallback(left, right);
}

int16_t libretro::Core::s_inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id)
{
  return s_instance->inputStateCallback(port, device, index, id);
}

void libretro::Core::s_inputPollCallback()
{
  s_instance->inputPollCallback();
}

uintptr_t libretro::Core::s_getCurrentFramebuffer()
{
  return s_instance->getCurrentFramebuffer();
}

retro_proc_address_t libretro::Core::s_getProcAddress(const char* symbol)
{
  return s_instance->getProcAddress(symbol);
}

void libretro::Core::s_logCallback(enum retro_log_level level, const char *fmt, ...)
{
  if (!s_instance)
    return;

  if (!s_instance->_logger->logLevel(level))
    return;

  va_list args;
  va_start(args, fmt);
  s_instance->_logger->vprintf(level, fmt, args);
  va_end(args);
}

bool libretro::Core::s_setRumbleCallback(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
  return s_instance->setRumble(port, effect, strength);
}

bool libretro::Core::setRumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
  return _input->setRumble(port, effect, strength);
}

void libretro::Core::s_setLEDState(int led, int state)
{
  s_instance->setLEDState(led, state);
}

retro_microphone_t* libretro::Core::s_openMic(const retro_microphone_params_t* params)
{
  return s_instance->_microphone->openMic(params);
}

void libretro::Core::s_closeMic(retro_microphone_t* microphone)
{
  s_instance->_microphone->closeMic(microphone);
}

bool libretro::Core::s_getMicParams(const retro_microphone_t* microphone, retro_microphone_params_t* params)
{
  return s_instance->_microphone->getMicParams(microphone, params);
}

bool libretro::Core::s_getMicState(const retro_microphone_t* microphone)
{
  return s_instance->_microphone->getMicState(microphone);
}

bool libretro::Core::s_setMicState(retro_microphone_t* microphone, bool state)
{
  return s_instance->_microphone->setMicState(microphone, state);
}

int libretro::Core::s_readMic(retro_microphone_t* microphone, int16_t* frames, size_t num_frames)
{
  return s_instance->_microphone->readMic(microphone, frames, num_frames);
}
