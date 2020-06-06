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

#include <stdlib.h>
#include <string.h>

#ifdef _WINDOWS
#include <RA_Interface.h>
#endif

#define SAMPLE_COUNT 8192

#define TAG "[COR] "

/**
 * Unavoidable because the libretro API don't have an userdata pointer to
 * allow us pass and receive back the core instance :/
 */
static thread_local libretro::Core* s_instance;

namespace
{
  // Helper class to set and unset the s_instance thread local.
  class InstanceSetter
  {
  public:
    inline InstanceSetter(libretro::Core* instance)
    {
      _previous = s_instance;
      s_instance = instance;
    }
    
    inline ~InstanceSetter()
    {
      s_instance = _previous;
    }

  protected:
    libretro::Core* _previous;
  };

  // Dummy components
  class Logger: public libretro::LoggerComponent
  {
  public:
    virtual void vprintf(enum retro_log_level level, const char* fmt, va_list args) override
    {
      (void)level;
      (void)fmt;
      (void)args;
    }
  };

  class Config: public libretro::ConfigComponent
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

    virtual bool varUpdated() override
    {
      return false;
    }

    virtual const char* getVariable(const char* variable) override
    {
      (void)variable;
      return NULL;
    }
  };

  class VideoContext : public libretro::VideoContextComponent
  {
  public:
    virtual void swapBuffers() override
    {
    }
  };

  class Video: public libretro::VideoComponent
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

    virtual void setRotation(Rotation rotation) override
    {
      (void)rotation;
    }

    Rotation getRotation() const override
    {
      return Rotation::None;
    }
  };

  class Audio: public libretro::AudioComponent
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

  class Input: public libretro::InputComponent
  {
  public:
    virtual void setInputDescriptors(const struct retro_input_descriptor* descs, unsigned count) override
    {
      (void)descs;
      (void)count;
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

  class Allocator: public libretro::AllocatorComponent
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
static Logger       s_logger;
static Config       s_config;
static VideoContext s_videoContext;
static Video        s_video;
static Audio        s_audio;
static Input        s_input;
static Allocator    s_allocator;

// Helper function for the memory map interface
static bool preprocessMemoryDescriptors(struct retro_memory_descriptor* descriptors, unsigned num_descriptors);

bool libretro::Core::init(const Components* components)
{
  InstanceSetter instance_setter(this);

  _logger       = &s_logger;
  _config       = &s_config;
  _videoContext = &s_videoContext;
  _video        = &s_video;
  _audio        = &s_audio;
  _input        = &s_input;
  _allocator    = &s_allocator;

  if (components != NULL)
  {
    if (components->logger != NULL)       _logger       = components->logger;
    if (components->config != NULL)       _config       = components->config;
    if (components->videoContext != NULL) _videoContext = components->videoContext;
    if (components->video != NULL)        _video        = components->video;
    if (components->audio != NULL)        _audio        = components->audio;
    if (components->input != NULL)        _input        = components->input;
    if (components->allocator != NULL)    _allocator    = components->allocator;
  }

  reset();
  return true;
}

bool libretro::Core::loadCore(const char* core_path)
{
  InstanceSetter instance_setter(this);

  _allocator->reset();

  _libretroPath = strdup(core_path);
  _samples = alloc<int16_t>(SAMPLE_COUNT);
  
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

bool libretro::Core::loadGame(const char* game_path, void* data, size_t size)
{
  InstanceSetter instance_setter(this);

  if (game_path == NULL)
  {
    _logger->error(TAG "Can't load game data without a ROM path");
    goto error;
  }

  if (_supportsNoGame)
  {
    _logger->error(TAG "Core doesn't take a content to run");
    goto error;
  }
  
  retro_game_info game;
  game.path = game_path;
  game.data = data;
  game.size = size;
  game.meta = NULL;

  if (!_core.loadGame(&game))
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
  InstanceSetter instance_setter(this);

  if (_gameLoaded)
  {
    _core.unloadGame();
  }

  _diskControlInterface = NULL;

  _core.deinit();
  _core.destroy();
  reset();
}

void libretro::Core::step(bool generateVideo, bool generateAudio)
{
  InstanceSetter instance_setter(this);

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

  _samplesCount = 0;

  _core.run();
  
  if (generateAudio && _samplesCount > 0)
  {
    // _samples are 2-channel (stereo)
    _audio->mix(_samples, _samplesCount / 2);
  }
}

unsigned libretro::Core::getApiVersion()
{
  InstanceSetter instance_setter(this);
  return _core.apiVersion();
}

unsigned libretro::Core::getRegion()
{
  InstanceSetter instance_setter(this);
  return _core.getRegion();
}

void* libretro::Core::getMemoryData(unsigned id)
{
  InstanceSetter instance_setter(this);
  return _core.getMemoryData(id);
}

size_t libretro::Core::getMemorySize(unsigned id)
{
  InstanceSetter instance_setter(this);
  return _core.getMemorySize(id);
}

void libretro::Core::resetGame()
{
  InstanceSetter instance_setter(this);
  _core.reset();
}

size_t libretro::Core::serializeSize()
{
  InstanceSetter instance_setter(this);
  return _core.serializeSize();
}

bool libretro::Core::serialize(void* data, size_t size)
{
  InstanceSetter instance_setter(this);
  return _core.serialize(data, size);
}

bool libretro::Core::unserialize(const void* data, size_t size)
{
  InstanceSetter instance_setter(this);
  return _core.unserialize(data, size);
}

bool libretro::Core::initCore()
{
  InstanceSetter instance_setter(this);

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
  InstanceSetter instance_setter(this);

  _core.setVideoRefresh(s_videoRefreshCallback);
  _core.setAudioSampleBatch(s_audioSampleBatchCallback);
  _core.setAudioSample(s_audioSampleCallback);
  _core.setInputState(s_inputStateCallback);
  _core.setInputPoll(s_inputPollCallback);
  
  _core.getSystemAVInfo(&_systemAVInfo);

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
    goto error;
  }
  
  if (!_audio->setRate(_systemAVInfo.timing.sample_rate))
  {
    goto error;
  }

  if (_needsHardwareRender)
  {
    _hardwareRenderCallback.context_reset();
  }
  
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
  _inputDescriptorsCount = 0;
  _inputDescriptors = NULL;
  _variablesCount = 0;
  _variables = 0;
  memset(&_hardwareRenderCallback, 0, sizeof(_hardwareRenderCallback));
  _needsHardwareRender = false;
  memset(&_systemInfo, 0, sizeof(_systemInfo));
  memset(&_systemAVInfo, 0, sizeof(_systemAVInfo));
  _subsystemInfoCount = 0;
  _subsystemInfo = NULL;
  _controllerInfoCount = 0;
  _controllerInfo = NULL;
  _ports = NULL;
  _diskControlInterface = NULL;
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
  _video->setRotation((Video::Rotation)data);
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
  InstanceSetter instance_setter(this);

  if (_diskControlInterface)
    _diskControlInterface->set_eject_state(open);
}

void libretro::Core::setCurrentDiscIndex(unsigned index)
{
  InstanceSetter instance_setter(this);

  if (_diskControlInterface)
    _diskControlInterface->set_image_index(index);
}

bool libretro::Core::shutdown()
{
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
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
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return true;
}

bool libretro::Core::setDiskControlInterface(const struct retro_disk_control_callback* data)
{
  _diskControlInterface = data;
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
  struct retro_variable* var;

  for (var = (struct retro_variable*)data; var->key != NULL; var++)
  {
    // Just count.
  }

  _variablesCount = var - data;
  _variables = alloc<struct retro_variable>(_variablesCount);

  if (_variables == NULL)
  {
    return false;
  }

  memcpy(_variables, data, _variablesCount * sizeof(*_variables));
  var = _variables;

  for (unsigned i = 0; i < _variablesCount; i++, var++)
  {
    var->key = strdup(var->key);
    var->value = strdup(var->value);

    if (var->key == NULL || var->value == NULL)
    {
      return false;
    }
  }

  _logger->debug(TAG "retro_variable");
  
  var = _variables;

  for (unsigned i = 0; i < _variablesCount; i++, var++)
  {
    _logger->debug(TAG "  %s: %s", var->key, var->value);
  }

  _config->setVariables(_variables, _variablesCount);
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

bool libretro::Core::setFrameTimeCallback(const struct retro_frame_time_callback* data)
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::setAudioCallback(const struct retro_audio_callback* data)
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::getRumbleInterface(struct retro_rumble_interface* data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
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

bool libretro::Core::getSensorInterface(struct retro_sensor_interface* data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::getCameraInterface(struct retro_camera_callback* data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::getLogInterface(struct retro_log_callback* data) const
{
  data->log = s_logCallback;
  return true;
}

bool libretro::Core::getPerfInterface(struct retro_perf_callback* data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::getLocationInterface(struct retro_location_callback* data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::getCoreAssetsDirectory(const char** data) const
{
  *data = _config->getCoreAssetsDirectory();
  return true;
}

bool libretro::Core::getSaveDirectory(const char** data) const
{
  *data = _config->getSaveDirectory();
  return true;
}

bool libretro::Core::setSystemAVInfo(const struct retro_system_av_info* data)
{
  _systemAVInfo = *data;

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

  return true;
}

bool libretro::Core::setProcAddressCallback(const struct retro_get_proc_address_interface* data)
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
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
  }

  preprocessMemoryDescriptors(descriptors, _memoryMap.num_descriptors);

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

bool libretro::Core::getCurrentSoftwareFramebuffer(struct retro_framebuffer* data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::getHWRenderInterface(const struct retro_hw_render_interface** data) const
{
  (void)data;
  _logger->error(TAG "%s isn't implemented", __FUNCTION__);
  return false;
}

bool libretro::Core::setSupportAchievements(bool data)
{
  _supportAchievements = data;
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
  };

  cmd &= ~(RETRO_ENVIRONMENT_EXPERIMENTAL | RETRO_ENVIRONMENT_PRIVATE);

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

  case RETRO_ENVIRONMENT_SHUTDOWN:
    ret = shutdown();
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

  case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
    ret = setFrameTimeCallback((const struct retro_frame_time_callback*)data);
    break;

  case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
    ret = setAudioCallback((const struct retro_audio_callback*)data);
    break;

  case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
    ret = getRumbleInterface((struct retro_rumble_interface*)data);
    break;

  case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
    ret = getInputDeviceCapabilities((uint64_t*)data);
    break;

  case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
    ret = getSensorInterface((struct retro_sensor_interface*)data);
    break;

  case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
    ret = getCameraInterface((struct retro_camera_callback*)data);
    break;

  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
    ret = getLogInterface((struct retro_log_callback*)data);
    break;

  case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
    ret = getPerfInterface((struct retro_perf_callback*)data);
    break;

  case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
    ret = getLocationInterface((struct retro_location_callback*)data);
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

  case RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK:
    ret = setProcAddressCallback((const struct retro_get_proc_address_interface*)data);
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

  case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
    ret = getCurrentSoftwareFramebuffer((struct retro_framebuffer*)data);
    break;

  case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
    ret = getHWRenderInterface((const struct retro_hw_render_interface**)data);
    break;

  case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    ret = setSupportAchievements(*(bool*)data);
    break;

  case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    ret = getInputBitmasks((bool*)data);
    break;

  default:
    cmd &= ~(RETRO_ENVIRONMENT_EXPERIMENTAL | RETRO_ENVIRONMENT_PRIVATE);

    if ((_calls[cmd / 8] & (1 << (cmd & 7))) == 0)
    {
      _logger->error(TAG "Unimplemented env call: %s", name, cmd);
      _calls[cmd / 8] |= 1 << (cmd & 7);
    }

    return false;
  }

  if (ret)
  {
    _logger->debug(TAG "Called  %s -> %d", name, ret);
  }
  else
  {
    _logger->error(TAG "Called  %s -> %d", name, ret);
  }

  return ret;
}

void libretro::Core::videoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch)
{
  _video->refresh(data, width, height, pitch);
}

size_t libretro::Core::audioSampleBatchCallback(const int16_t* data, size_t frames)
{
  if (_samplesCount < SAMPLE_COUNT - frames * 2 + 1)
  {
    memcpy(_samples + _samplesCount, data, frames * 2 * sizeof(int16_t));
    _samplesCount += frames * 2;
  }

  return frames;
}

void libretro::Core::audioSampleCallback(int16_t left, int16_t right)
{
  if (_samplesCount < SAMPLE_COUNT - 1)
  {
    _samples[_samplesCount++] = left;
    _samples[_samplesCount++] = right;
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

  va_list args;
  va_start(args, fmt);
  s_instance->_logger->vprintf(level, fmt, args);
  va_end(args);
}

static size_t addBitsDown(size_t n)
{
  n |= n >>  1;
  n |= n >>  2;
  n |= n >>  4;
  n |= n >>  8;
  n |= n >> 16;

  /* double shift to avoid warnings on 32bit (it's dead code, but compilers suck) */
  if (sizeof(size_t) > 4)
  {
    n |= n >> 16 >> 16;
  }

  return n;
}

static size_t inflate(size_t addr, size_t mask)
{
  while (mask)
  {
    size_t tmp = (mask - 1) & ~mask;
    /* to put in an 1 bit instead, OR in tmp+1 */
    addr = ((addr & ~tmp) << 1) | (addr & tmp);
    mask = mask & (mask - 1);
  }

  return addr;
}

static size_t reduce(size_t addr, size_t mask)
{
  while (mask)
  {
    size_t tmp = (mask - 1) & ~mask;
    addr = (addr & tmp) | ((addr >> 1) & ~tmp);
    mask = (mask & (mask - 1)) >> 1;
  }

  return addr;
}

static size_t highestBit(size_t n)
{
   n = addBitsDown(n);
   return n ^ (n >> 1);
}

static bool preprocessMemoryDescriptors(struct retro_memory_descriptor* descriptors, unsigned num_descriptors)
{
  struct retro_memory_descriptor* desc;
  const struct retro_memory_descriptor* end = descriptors + num_descriptors;
  size_t disconnect_mask;
  size_t top_addr = 1;

  for (desc = descriptors; desc < end; desc++)
  {
    if (desc->select != 0)
    {
      top_addr |= desc->select;
    }
    else
    {
      top_addr |= desc->start + desc->len - 1;
    }
  }

  top_addr = addBitsDown(top_addr);

  for (desc = descriptors; desc < end; desc++)
  {
    if (desc->select == 0)
    {
      if (desc->len == 0)
      {
        return false;
      }

      if ((desc->len & (desc->len - 1)) != 0)
      {
        return false;
      }

      desc->select = top_addr & ~inflate(addBitsDown(desc->len - 1), desc->disconnect);
    }

    if (desc->len == 0)
    {
      desc->len = addBitsDown(reduce(top_addr & ~desc->select, desc->disconnect)) + 1;
    }

    if (desc->start & ~desc->select)
    {
      return false;
    }

    while (reduce(top_addr & ~desc->select, desc->disconnect) >> 1 > desc->len - 1)
    {
      desc->disconnect |= highestBit(top_addr & ~desc->select & ~desc->disconnect);
    }

    disconnect_mask = addBitsDown(desc->len - 1);
    desc->disconnect &= disconnect_mask;

    while ((~disconnect_mask) >> 1 & desc->disconnect)
    {
      disconnect_mask >>= 1;
      desc->disconnect &= disconnect_mask;
    }
  }

  return true;
}
