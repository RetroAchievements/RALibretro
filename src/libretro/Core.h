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

#include "BareCore.h"
#include "Components.h"

#include <stdarg.h>

namespace libretro
{
  /**
   * core contains a BareCore instance, and provides high level functionality.
   */
  class Core
  {
  public:
    bool init(const Components* components);
    bool loadCore(const char* core_path);
    bool initCore();
    bool loadGame(const char* game_path, void* data, size_t size, std::string* errorBuffer);

    void destroy();
    
    void step(bool generateVideo, bool generateAudio);

    unsigned getApiVersion();
    unsigned getRegion();
    void*    getMemoryData(unsigned id);
    size_t   getMemorySize(unsigned id);
    void     resetGame();
    size_t   serializeSize();
    bool     serialize(void* data, size_t size);
    bool     unserialize(const void* data, size_t size);
    
    inline unsigned                getPerformanceLevel()    const { return _performanceLevel; }
    inline enum retro_pixel_format getPixelFormat()         const { return _pixelFormat; }
    inline bool                    getNeedsHardwareRender() const { return _needsHardwareRender; }
    inline bool                    getSupportsNoGame()      const { return _supportsNoGame; }
    inline bool                    getSupportAchievements() const { return _supportAchievements; }
    inline void                    unloadGame()                   { _core.unloadGame(); }
    inline bool                    gameLoaded()             const { return _gameLoaded; }

    inline unsigned                getNumDiscs()            const { return (_diskControlInterface.get_num_images != NULL) ? _diskControlInterface.get_num_images() : 0; }
    inline unsigned                getCurrentDiscIndex()    const { return (_diskControlInterface.get_image_index != NULL) ? _diskControlInterface.get_image_index() : 0; }
    void                           setCurrentDiscIndex(unsigned);
    inline bool                    getTrayOpen()            const { return (_diskControlInterface.get_eject_state != NULL) ? (bool)_diskControlInterface.get_eject_state() : false; }
    void                           setTrayOpen(bool open);

    inline const struct retro_input_descriptor* getInputDescriptors(unsigned* count) const
    {
      *count = _inputDescriptorsCount;
      return _inputDescriptors;
    }

    inline const struct retro_subsystem_info* getSubsystemInfo(unsigned* count) const
    {
      *count = _subsystemInfoCount;
      return _subsystemInfo;
    }

    inline const struct retro_controller_info* getControllerInfo(unsigned* count) const
    {
      *count = _controllerInfoCount;
      return _controllerInfo;
    }

    inline const struct retro_hw_render_callback* getHardwareRenderCallback() const
    {
      return &_hardwareRenderCallback;
    }
    
    inline const struct retro_system_info* getSystemInfo() const
    {
      return &_systemInfo;
    }

    inline const struct retro_system_av_info* getSystemAVInfo() const
    {
      return &_systemAVInfo;
    }
    
    inline const struct retro_memory_map* getMemoryMap() const
    {
      return &_memoryMap;
    }
    
  protected:
    // Initialization
    bool initAV();
    void reset();
    
    // Configuration
    const char* getLibretroPath() const;

    // Memory allocation
    template<typename T>
    inline T* alloc(size_t count = 1)
    {
      return (T*)_allocator->allocate(alignof(T), count * sizeof(T));
    }

    char* strdup(const char* str);
    
    // Environment functions
    bool setRotation(unsigned data);
    bool getOverscan(bool* data) const;
    bool getCanDupe(bool* data) const;
    bool setMessage(const struct retro_message* data);
    bool setPerformanceLevel(unsigned data);
    bool getSystemDirectory(const char** data) const;
    bool setPixelFormat(enum retro_pixel_format data);
    bool setInputDescriptors(const struct retro_input_descriptor* data);
    bool setDiskControlInterface(const struct retro_disk_control_callback* data);
    bool setDiskControlExtInterface(const struct retro_disk_control_ext_callback* data);
    bool setHWRender(struct retro_hw_render_callback* data);
    bool getVariable(struct retro_variable* data);
    bool setVariables(const struct retro_variable* data);
    bool getVariableUpdate(bool* data);
    bool setSupportNoGame(bool data);
    bool getLibretroPath(const char** data) const;
    bool getRumbleInterface(struct retro_rumble_interface* data) const;
    bool getInputDeviceCapabilities(uint64_t* data) const;
    bool getLogInterface(struct retro_log_callback* data) const;
    bool getCoreAssetsDirectory(const char** data) const;
    bool getSaveDirectory(const char** data) const;
    bool setSystemAVInfo(const struct retro_system_av_info* data);
    bool setSubsystemInfo(const struct retro_subsystem_info* data);
    bool setControllerInfo(const struct retro_controller_info* data);
    bool setMemoryMaps(const struct retro_memory_map* data);
    bool setGeometry(const struct retro_game_geometry* data);
    bool getUsername(const char** data) const;
    bool getLanguage(unsigned* data) const;
    bool setSupportAchievements(bool data);
    bool getFastForwarding(bool* data);
    bool getInputBitmasks(bool* data);
    bool getCoreOptionsVersion(unsigned* data) const;
    bool setCoreOptions(const struct retro_core_option_definition* data);
    bool setCoreOptionsIntl(const struct retro_core_options_intl* data);
    bool setCoreOptionsDisplay(const struct retro_core_option_display* data);
    bool getPreferredHWRender(unsigned* data);

    // Callbacks
    bool                 environmentCallback(unsigned cmd, void* data);
    void                 videoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch);
    size_t               audioSampleBatchCallback(const int16_t* data, size_t frames);
    void                 audioSampleCallback(int16_t left, int16_t right);
    int16_t              inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id);
    void                 inputPollCallback();
    uintptr_t            getCurrentFramebuffer();
    retro_proc_address_t getProcAddress(const char* symbol);
    void                 logCallback(enum retro_log_level level, const char *fmt, va_list args);
    bool                 setRumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength);

    // Static callbacks that use s_instance to call into the core's implementation
    static bool                 s_environmentCallback(unsigned cmd, void* data);
    static void                 s_videoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch);
    static size_t               s_audioSampleBatchCallback(const int16_t* data, size_t frames);
    static void                 s_audioSampleCallback(int16_t left, int16_t right);
    static int16_t              s_inputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id);
    static void                 s_inputPollCallback();
    static uintptr_t            s_getCurrentFramebuffer();
    static retro_proc_address_t s_getProcAddress(const char* symbol);
    static void                 s_logCallback(enum retro_log_level level, const char *fmt, ...);
    static bool                 s_setRumbleCallback(unsigned port, enum retro_rumble_effect effect, uint16_t strength);

    LoggerComponent*                _logger;
    ConfigComponent*                _config;
    VideoContextComponent*          _videoContext;
    VideoComponent*                 _video;
    AudioComponent*                 _audio;
    InputComponent*                 _input;
    AllocatorComponent*             _allocator;
        
    BareCore                        _core;
    bool                            _gameLoaded;
    
    int16_t*                        _samples;
    size_t                          _samplesCount;

    const char*                     _libretroPath;
    unsigned                        _performanceLevel;
    enum retro_pixel_format         _pixelFormat;
    bool                            _supportsNoGame;
    bool                            _supportAchievements;
    bool                            _fastForwarding;
    
    unsigned                        _inputDescriptorsCount;
    struct retro_input_descriptor*  _inputDescriptors;

    struct retro_hw_render_callback _hardwareRenderCallback;
    bool                            _needsHardwareRender;
    
    struct retro_system_info        _systemInfo;
    struct retro_system_av_info     _systemAVInfo;
    
    unsigned                        _subsystemInfoCount;
    struct retro_subsystem_info*    _subsystemInfo;
    
    unsigned                        _controllerInfoCount;
    struct retro_controller_info*   _controllerInfo;
    unsigned*                       _ports;

    struct retro_disk_control_callback _diskControlInterface;

    struct retro_memory_map         _memoryMap;

    uint8_t                         _calls[128 / 8];
  };
}
