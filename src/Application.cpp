#include "Application.h"

#include <SDL_syswm.h>

#include "libretro/Core.h"
#include "jsonsax/jsonsax.h"

#include "RA_Integration/RA_Implementation.h"
#include "RA_Integration/RA_Interface.h"
#include "RA_Integration/RA_Resource.h"

#include "About.h"
#include "KeyBinds.h"
#include "Util.h"

#include "resource.h"

#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

HWND g_mainWindow;
Application app;

static unsigned char memoryRead(unsigned int addr)
{
	return app.memoryRead(addr);
}

static void memoryWrite(unsigned int addr, unsigned int value)
{
	app.memoryWrite(addr, value);
}

Application::Application(): _fsm(*this)
{
  _components.logger    = &_logger;
  _components.config    = &_config;
  _components.video     = &_video;
  _components.audio     = &_audio;
  _components.input     = &_input;
  _components.allocator = &_allocator;
}

bool Application::init(const char* title, int width, int height)
{
  enum
  {
    kNothingInited,
    kLoggerInited,
    kAllocatorInited,
    kSdlInited,
    kWindowInited,
    kRendererInited,
    kAudioDeviceInited,
    kFifoInited,
    kAudioInited,
    kConfigInited,
    kInputInited,
    kKeyBindsInited,
    kVideoInited
  }
  inited = kNothingInited;

  _emulator = Emulator::kNone;

  if (!_logger.init())
  {
    goto error;
  }

  inited = kLoggerInited;

  if (!_allocator.init(&_logger))
  {
    _logger.printf(RETRO_LOG_ERROR, "Failed to initialize the allocator");
    goto error;
  }

  inited = kAllocatorInited;

  // Setup SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
  {
    _logger.printf(RETRO_LOG_ERROR, "SDL_Init: %s", SDL_GetError());
    goto error;
  }

  inited = kSdlInited;

  // Setup window
#ifdef CROSS_BUILD
  // SDL_WINDOW_OPENGL crashes when run with wine
  _window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_RESIZABLE);
#else
  _window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
#endif

  if (_window == NULL)
  {
    _logger.printf(RETRO_LOG_ERROR, "SDL_CreateWindow: %s", SDL_GetError());
    goto error;
  }

  inited = kWindowInited;

  _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  if (_renderer == NULL)
  {
    _logger.printf(RETRO_LOG_ERROR, "SDL_CreateRenderer: %s", SDL_GetError());
    goto error;
  }

  inited = kRendererInited;

  // Init audio
  SDL_AudioSpec want;
  memset(&want, 0, sizeof(want));

  want.freq = 44100;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = s_audioCallback;
  want.userdata = this;

  _audioDev = SDL_OpenAudioDevice(NULL, 0, &want, &_audioSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);

  if (_audioDev == 0)
  {
    _logger.printf(RETRO_LOG_ERROR, "SDL_OpenAudioDevice: %s", SDL_GetError());
    goto error;
  }

  inited = kAudioDeviceInited;

  if (!_fifo.init(_audioSpec.size * 4))
  {
    _logger.printf(RETRO_LOG_ERROR, "Error initializing the audio FIFO");
    goto error;
  }

  inited = kFifoInited;

  SDL_PauseAudioDevice(_audioDev, 0);

  // Initialize the rest of the components
  if (!_audio.init(&_logger, (double)_audioSpec.freq, &_fifo))
  {
    goto error;
  }

  inited = kAudioInited;

  if (!_config.init(&_logger))
  {
    goto error;
  }

  inited = kConfigInited;

  if (!_input.init(&_logger))
  {
    goto error;
  }

  inited = kInputInited;

  if (!_keybinds.init(&_logger))
  {
    goto error;
  }

  inited = kKeyBindsInited;

  if (!_video.init(&_logger, &_config, _renderer))
  {
    goto error;
  }

  inited = kVideoInited;

  {
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);

    if (SDL_GetWindowWMInfo(_window, &wminfo) != SDL_TRUE)
    {
      _logger.printf(RETRO_LOG_ERROR, "SDL_GetWindowWMInfo: %s", SDL_GetError());
      goto error;
    }

    g_mainWindow = wminfo.info.win.window;

    _menu = LoadMenu(NULL, "MAIN");
    SetMenu(g_mainWindow, _menu);

    RA_Init(g_mainWindow, RA_Libretro, VERSION);
    RA_InitShared();
    RA_InitDirectX();
    RA_AttemptLogin(true);
    RebuildMenu();
  }

  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  _emulator = Emulator::kNone;
  _validSlots = 0;
  _currentSlot = 0;
  lastHardcore = hardcore();
  updateMenu();
  return true;

error:
  switch (inited)
  {
  case kVideoInited:       _video.destroy();
  case kKeyBindsInited:    _keybinds.destroy();
  case kInputInited:       _input.destroy();
  case kConfigInited:      _config.destroy();
  case kAudioInited:       _audio.destroy();
  case kFifoInited:        _fifo.destroy();
  case kAudioDeviceInited: SDL_CloseAudioDevice(_audioDev);
  case kRendererInited:    SDL_DestroyRenderer(_renderer);
  case kWindowInited:      SDL_DestroyWindow(_window);
  case kSdlInited:         SDL_Quit();
  case kAllocatorInited:   _allocator.destroy();
  case kLoggerInited:      _logger.destroy();
  case kNothingInited:     break;
  }

  _emulator = Emulator::kNone;
  return false;
}

void Application::run()
{
  do
  {
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_QUIT:
        _fsm.quit();
        break;

      case SDL_CONTROLLERDEVICEADDED:
      case SDL_CONTROLLERDEVICEREMOVED:
      case SDL_CONTROLLERBUTTONUP:
      case SDL_CONTROLLERBUTTONDOWN:
      case SDL_CONTROLLERAXISMOTION:
        _input.processEvent(&event);
        break;
      
      case SDL_SYSWMEVENT:
        handle(&event.syswm);
        break;
      
      case SDL_KEYUP:
      case SDL_KEYDOWN:
        handle(&event.key);
        break;
      }
    }

    if (hardcore() != lastHardcore)
    {
      lastHardcore = hardcore();
      updateMenu();
    }

    int limit = 0;
    bool audio = false;

    switch (_fsm.currentState())
    {
    case Fsm::State::GameRunning:
      limit = 1;
      audio = true;
      break;

    case Fsm::State::GamePaused:
    default:
      limit = 0;
      audio = false;
      break;

    case Fsm::State::FrameStep:
      limit = 1;
      audio = false;
      _fsm.resumeGame();
      break;

    case Fsm::State::GameTurbo:
      limit = 5;
      audio = false;
      break;
    }

    for (int i = 0; i < limit; i++)
    {
      _core.step(audio);
      RA_DoAchievementsFrame();
    }

    SDL_RenderClear(_renderer);
    _video.draw();
    SDL_RenderPresent(_renderer);

    RA_HandleHTTPResults();

    static Uint32 t0 = 0;

    if (t0 == 0)
    {
      t0 = SDL_GetTicks();
    }
    else
    {
      Uint32 t1 = SDL_GetTicks();
      HDC hdc = GetDC(g_mainWindow);

      if (hdc != NULL)
      {
        ControllerInput input;
        input.m_bUpPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) != 0;
        input.m_bDownPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0;
        input.m_bLeftPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) != 0;
        input.m_bRightPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0;
        input.m_bConfirmPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) != 0;
        input.m_bCancelPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) != 0;
        input.m_bQuitPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) != 0;

        RECT size;
        GetClientRect(g_mainWindow, &size);

        RA_UpdateRenderOverlay(hdc, &input, (t1 - t0) / 1000.0f, &size, false, _fsm.currentState() == Fsm::State::GamePaused);
        ReleaseDC(g_mainWindow, hdc);
      }

      t0 = t1;
    }

    SDL_Delay(1);
  }
  while (_fsm.currentState() != Fsm::State::Quit);
}

void Application::destroy()
{
  _video.destroy();
  _keybinds.destroy();
  _input.destroy();
  _config.destroy();
  _audio.destroy();
  _fifo.destroy();

  SDL_CloseAudioDevice(_audioDev);
  SDL_DestroyRenderer(_renderer);
  SDL_DestroyWindow(_window);
  SDL_Quit();

  _allocator.destroy();
  _logger.destroy();
}

bool Application::loadCore(Emulator emulator)
{
  _config.reset();
  _input.reset();

  if (!_core.init(&_components))
  {
    return false;
  }

  std::string path = _config.getRootFolder();
  path += "Cores\\";
  path += getCoreFileName(emulator);
  path += ".dll";

  if (!_core.loadCore(path.c_str()))
  {
    _core.destroy();
    return false;
  }

  _emulator = emulator;
  return true;
}

void Application::updateMenu()
{
  static const UINT all_items[] =
  {
    IDM_SYSTEM_STELLA, IDM_SYSTEM_SNES9X, IDM_SYSTEM_PICODRIVE, IDM_SYSTEM_GENESISPLUSGX, IDM_SYSTEM_FCEUMM,
    IDM_SYSTEM_HANDY, IDM_SYSTEM_BEETLESGX, IDM_SYSTEM_GAMBATTE, IDM_SYSTEM_MGBA, IDM_SYSTEM_MEDNAFENPSX,
    IDM_SYSTEM_MEDNAFENNGP,

    IDM_CLOSE_CORE,
    IDM_LOAD_GAME,
    IDM_PAUSE_GAME, IDM_RESUME_GAME, IDM_RESET_GAME, IDM_CLOSE_GAME,
    IDM_EXIT,

    IDM_CORE, IDM_INPUT, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT start_items[] =
  {
    IDM_SYSTEM_STELLA, IDM_SYSTEM_SNES9X, IDM_SYSTEM_PICODRIVE, IDM_SYSTEM_GENESISPLUSGX, IDM_SYSTEM_FCEUMM,
    IDM_SYSTEM_HANDY, IDM_SYSTEM_BEETLESGX, IDM_SYSTEM_GAMBATTE, IDM_SYSTEM_MGBA, IDM_SYSTEM_MEDNAFENPSX,
    IDM_SYSTEM_MEDNAFENNGP,

    IDM_EXIT, IDM_ABOUT
  };

  static const UINT core_loaded_items[] =
  {
    IDM_CLOSE_CORE, IDM_LOAD_GAME, IDM_EXIT, IDM_CORE, IDM_INPUT, IDM_ABOUT
  };

  static const UINT game_running_items[] =
  {
    IDM_PAUSE_GAME, IDM_RESET_GAME, IDM_CLOSE_GAME,
    IDM_EXIT,

    IDM_CORE, IDM_INPUT, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_paused_items[] =
  {
    IDM_RESUME_GAME, IDM_RESET_GAME, IDM_CLOSE_GAME,
    IDM_EXIT,

    IDM_CORE, IDM_INPUT, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_turbo_items[] =
  {
    IDM_PAUSE_GAME, IDM_RESET_GAME, IDM_CLOSE_GAME,
    IDM_EXIT,

    IDM_CORE, IDM_INPUT, IDM_TURBO_GAME, IDM_ABOUT
  };

  enableItems(all_items, sizeof(all_items) / sizeof(all_items[0]), MF_DISABLED);

  switch (_fsm.currentState())
  {
  case Fsm::State::Start:
    enableItems(start_items, sizeof(start_items) / sizeof(start_items[0]), MF_ENABLED);
    enableSlots();
    break;
  case Fsm::State::CoreLoaded:
    enableItems(core_loaded_items, sizeof(core_loaded_items) / sizeof(core_loaded_items[0]), MF_ENABLED);
    enableSlots();
    break;
  case Fsm::State::GameRunning:
    enableItems(game_running_items, sizeof(game_running_items) / sizeof(game_running_items[0]), MF_ENABLED);
    enableSlots();
    break;
  case Fsm::State::GamePaused:
    enableItems(game_paused_items, sizeof(game_paused_items) / sizeof(game_paused_items[0]), MF_ENABLED);
    enableSlots();
    break;
  case Fsm::State::GameTurbo:
    enableItems(game_turbo_items, sizeof(game_turbo_items) / sizeof(game_turbo_items[0]), MF_ENABLED);
    enableSlots();
    break;
  default:
    break;
  }
}

bool Application::loadGame(const std::string& path)
{
  const struct retro_system_info* info = _core.getSystemInfo();
  size_t size;
  void* data;

  if (info->need_fullpath)
  {
    size = 0;
    data = NULL;
  }
  else
  {
    data = loadFile(&_logger, path, &size);

    if (data == NULL)
    {
      return false;
    }
  }
    
  if (!_core.loadGame(path.c_str(), data, size))
  {
    free(data);
    return false;
  }

  switch (_emulator)
  {
  case Emulator::kNone:
    break;

  case Emulator::kStella:
    _system = System::kAtari2600;
    break;

  case Emulator::kSnes9x:
    _system = System::kSuperNintendo;
    break;

  case Emulator::kPicoDrive:
  case Emulator::kGenesisPlusGx:
    if (_core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM) == 0x2000)
    {
      _system = System::kMasterSystem;
    }
    else
    {
      _system = System::kMegaDrive;
    }

    break;
  
  case Emulator::kFceumm:
    _system = System::kNintendo;
    break;
  
  case Emulator::kHandy:
    _system = System::kAtariLynx;
    break;
  
  case Emulator::kBeetleSgx:
    _system = System::kPCEngine;
    break;
  
  case Emulator::kGambatte:
  case Emulator::kMGBA:
    if (path.substr(path.length() - 4) == ".gbc")
    {
      _system = System::kGameBoyColor;
    }
    else if (path.substr(path.length() - 4) == ".gba")
    {
      // Gambatte doesn't support GBA, but it won't be a problem to test it here
      _system = System::kGameBoyAdvance;
    }
    else
    {
      _system = System::kGameBoy;
    }

    break;
  
  case Emulator::kMednafenPsx:
    _system = System::kPlayStation1;
    break;
  
  case Emulator::kMednafenNgp:
    _system = System::kNeoGeoPocket;
    break;
  }

  RA_SetConsoleID((unsigned)_system);
  RA_ClearMemoryBanks();
  signalRomLoaded(path, data, size);
  free(data);
  _gamePath = path;

  if (_core.getMemorySize(RETRO_MEMORY_SAVE_RAM) != 0)
  {
    std::string sram = getSRamPath();
    data = loadFile(&_logger, sram, &size);

    if (data != NULL)
    {
      if (size == _core.getMemorySize(RETRO_MEMORY_SAVE_RAM))
      {
        void* memory = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
        memcpy(memory, data, size);
      }
      else
      {
        _logger.printf(RETRO_LOG_ERROR, "Save RAM size mismatch, wanted %lu, got %lu from disk", _core.getMemorySize(RETRO_MEMORY_SAVE_RAM), size);
      }

      free(data);
    }
  }

  _memoryRegionCount = 0;

  switch (_emulator)
  {
  case Emulator::kNone:
    break;

  case Emulator::kStella:
  case Emulator::kPicoDrive:
  case Emulator::kGenesisPlusGx:
  case Emulator::kHandy:
  case Emulator::kBeetleSgx:
  case Emulator::kGambatte:
  case Emulator::kMednafenPsx:
  case Emulator::kMednafenNgp:
    data = _core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
    size = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
    registerMemoryRegion(data, size);
    RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, size);
    _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);
    break;

  case Emulator::kSnes9x:
    data = _core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
    size = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
    registerMemoryRegion(data, size);
    RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, size);
    _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);

    data = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
    size = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);
    registerMemoryRegion(data, size);
    RA_InstallMemoryBank(1, (void*)::memoryRead, (void*)::memoryWrite, size);
    _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);

    break;
  
  case Emulator::kFceumm:
    {
      static const uint8_t _1k[1024] = {0};
      const struct retro_memory_map* mmap = _core.getMemoryMap();
      void* pointer[64];

      for (unsigned i = 0; i < 64; i++)
      {
        pointer[i] = (void*)_1k;
      }

      for (unsigned i = 0; i < mmap->num_descriptors; i++)
      {
        if (mmap->descriptors[i].start < 65536 && mmap->descriptors[i].len == 1024)
        {
          pointer[mmap->descriptors[i].start / 1024] = mmap->descriptors[i].ptr;
        }
      }

      for (unsigned i = 2; i < 8; i += 2)
      {
        pointer[i] = pointer[0];
        pointer[i + 1] = pointer[1];
      }

      for (unsigned i = 0; i < 64; i++)
      {
        registerMemoryRegion(pointer[i], 1024);
      }

      RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, 65536);
      _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);
    }

    break;
  
  case Emulator::kMGBA:
    if (_system == System::kGameBoyAdvance)
    {
      const struct retro_memory_map* mmap = _core.getMemoryMap();

      for (unsigned i = 0; i < mmap->num_descriptors; i++)
      {
        if (mmap->descriptors[i].start == 0x03000000U)
        {
          data = mmap->descriptors[i].ptr;
          size = mmap->descriptors[i].len;
          registerMemoryRegion(data, size);
          RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, size);
          _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);
        }
        else if (mmap->descriptors[i].start == 0x02000000U)
        {
          data = mmap->descriptors[i].ptr;
          size = mmap->descriptors[i].len;
          registerMemoryRegion(data, size);
          RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, size);
          _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);
        }
      }
    }
    else
    {
      data = _core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      size = 32768;
      registerMemoryRegion(data, size);
      RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, size);
      _logger.printf(RETRO_LOG_INFO, "Installed  %7zu bytes at 0x%08x", size, data);
    }

    break;
  }

  _validSlots = 0;

  for (unsigned ndx = 0; ndx < 10; ndx++)
  {
    std::string path = getStatePath(ndx);
    struct stat statbuf;

    if (stat(path.c_str(), &statbuf) == 0)
    {
      _validSlots |= 1 << ndx;
    }
  }

  data = loadFile(&_logger, getCoreConfigPath(_emulator), &size);

  if (data != NULL)
  {
    struct Deserialize
    {
      Application* self;
      std::string key;
    };

    Deserialize ud;
    ud.self = this;

    jsonsax_parse( (char*)data, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
    {
      auto ud = (Deserialize*)udata;

      if (event == JSONSAX_KEY)
      {
        ud->key = std::string(str, num);
      }
      else if (event == JSONSAX_OBJECT)
      {
        if (ud->key == "core")
        {
          ud->self->_config.deserialize(str);
        }
        else if (ud->key == "input")
        {
          ud->self->_input.deserialize(str);
        }
      }

      return 0;
    });

    free(data);
  }

  _input.autoAssign();
  return true;
}

void Application::unloadCore()
{
  std::string json;
  json.append("{\"core\":");
  json.append(_config.serialize());
  json.append(",\"input\":");
  json.append(_input.serialize());
  json.append("}");

  saveFile(&_logger, getCoreConfigPath(_emulator), json.c_str(), json.length());

  _core.destroy();
}

void Application::resetGame()
{
  _core.resetGame();
}

bool Application::hardcore()
{
  return RA_HardcoreModeIsActive();
}

bool Application::unloadGame()
{
  if (!RA_ConfirmLoadNewRom(true))
  {
    return false;
  }

  size_t size = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);

  if (size != 0)
  {
    void* data = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
    std::string sram = getSRamPath();
    saveFile(&_logger, sram.c_str(), data, size);
  }

  return true;
}

void Application::pauseGame(bool pause)
{
  RA_SetPaused(pause);
}

void Application::printf(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  _logger.vprintf(RETRO_LOG_DEBUG, fmt, args);
  va_end(args);
  fflush(stdout);
}

unsigned char Application::memoryRead(unsigned int addr)
{
  MemoryRegion* region = _memoryRegions;
  unsigned count = 0;

  while (count < _memoryRegionCount && addr > region->size)
  {
    addr -= region->size;
    region++;
    count++;
  }

  if (count < _memoryRegionCount && region->data != NULL)
  {
    return region->data[addr];
  }

  return 0;
}

void Application::memoryWrite(unsigned int addr, unsigned int value)
{
  MemoryRegion* region = _memoryRegions;
  unsigned count = 0;

  while (count < _memoryRegionCount && addr > region->size)
  {
    addr -= region->size;
    region++;
    count++;
  }

  if (count < _memoryRegionCount && region->data != NULL)
  {
    region->data[addr] = value;
  }
}

bool Application::isGameActive()
{
  switch (_fsm.currentState())
  {
  case Fsm::State::GameRunning:
  case Fsm::State::GamePaused:
  case Fsm::State::GameTurbo:
  case Fsm::State::FrameStep:
    return true;

  default:
    return false;
  }
}

void Application::s_audioCallback(void* udata, Uint8* stream, int len)
{
  Application* app = (Application*)udata;

  if (app->_fsm.currentState() == Fsm::State::GameRunning)
  {
    size_t avail = app->_fifo.occupied();

    if (avail < (size_t)len)
    {
      app->_fifo.read((void*)stream, avail);
      memset((void*)(stream + avail), 0, len - avail);
    }
    else
    {
      app->_fifo.read((void*)stream, len);
    }
  }
  else
  {
    memset((void*)stream, 0, len);
  }
}

void Application::signalRomLoadedWithPadding(void* rom, size_t size, size_t max_size, int fill)
{
  uint8_t* data = (uint8_t*)malloc(max_size);

  if (data != NULL)
  {
    if (size < max_size)
    {
      memcpy(data, rom, size);
      memset(data + size, fill, max_size - size);
    }
    else
    {
      memcpy(data, rom, max_size);
    }

    RA_OnLoadNewRom(data, max_size);
    free(data);
  }
}

void Application::signalRomLoadedNes(void* rom, size_t size)
{
  /* Note about the references to the FCEU emulator below. There is no
    * core-specific code in this function, it's rather Retro Achievements
    * specific code that must be followed to the letter so we compute
    * the correct ROM hash. Retro Achievements does indeed use some
    * FCEU related method to compute the hash, since its NES emulator
    * is based on it. */

  struct NesHeader
  {
    uint8_t id[4]; /* NES^Z */
    uint8_t rom_size;
    uint8_t vrom_size;
    uint8_t rom_type;
    uint8_t rom_type2;
    uint8_t reserve[8];
  };

  if (size < sizeof(NesHeader))
  {
    return;
  }

  NesHeader header;
  memcpy(&header, rom, sizeof(header));

  if (header.id[0] != 'N' || header.id[1] != 'E' || header.id[2] != 'S' || header.id[3] != 0x1a)
  {
    return;
  }

  size_t rom_size;

  if (header.rom_size != 0)
  {
    rom_size = nextPow2(header.rom_size);
  }
  else
  {
    rom_size = 256;
  }

  /* from FCEU core - compute size using the cart mapper */
  int mapper = (header.rom_type >> 4) | (header.rom_type2 & 0xf0);

  /* for games not to the power of 2, so we just read enough
    * PRG rom from it, but we have to keep ROM_size to the power of 2
    * since PRGCartMapping wants ROM_size to be to the power of 2
    * so instead if not to power of 2, we just use head.ROM_size when
    * we use FCEU_read. */
  bool round = mapper != 53 && mapper != 198 && mapper != 228;
  size_t bytes = round ? rom_size : header.rom_size;

  /* from FCEU core - check if Trainer included in ROM data */
  size_t offset = sizeof(header) + (header.rom_type & 4 ? sizeof(header) : 0);
  size_t count = 0x4000 * bytes;

  signalRomLoadedWithPadding((uint8_t*)rom + offset, size, count, 0xff);
}

void Application::signalRomLoadPsx(const std::string& path)
{
  (void)path;
}

void Application::signalRomLoaded(const std::string& path, void* rom, size_t size)
{
  if (_system != System::kPlayStation1 && rom == NULL)
  {
    rom = loadFile(&_logger, path, &size);
  }

  switch (_system)
  {
  case System::kAtari2600:
  case System::kPCEngine:
  case System::kGameBoy:
  case System::kGameBoyColor:
  case System::kGameBoyAdvance:
  case System::kNeoGeoPocket:
  case System::kMasterSystem:
  case System::kMegaDrive:
  default:
    RA_OnLoadNewRom((BYTE*)rom, size);
    break;

  case System::kSuperNintendo:
    signalRomLoadedWithPadding(rom, size, 8 * 1024 * 1024, 0);
    break;

  case System::kNintendo:
    signalRomLoadedNes(rom, size);
    break;
  
  case System::kAtariLynx:
    RA_OnLoadNewRom((BYTE*)rom + 0x0040, size > 0x0240 ? 0x0200 : size - 0x0040);
    break;
  
  case System::kPlayStation1:
    signalRomLoadPsx(path);
    break;
  }
}

void Application::loadGame()
{
  char filter[128];

  {
    char* aux = filter;
    const char* end = filter + sizeof(filter);

    const struct retro_system_info* info = _core.getSystemInfo();
    const char* ext = info->valid_extensions;

    aux += sprintf(aux, "All Files") + 1;
    aux += sprintf(aux, "*.*") + 1;
    aux += sprintf(aux, "Supported Files") + 1;

    if (_emulator != Emulator::kGenesisPlusGx)
    {
      for (;;)
      {
        if (aux < end)
        {
          *aux++ = '*';
        }

        if (aux < end)
        {
          *aux++ = '.';
        }

        while (aux < end && *ext != '|' && *ext != 0)
        {
          *aux++ = toupper(*ext++);
        }

        if (*ext == 0)
        {
          break;
        }

        ext++;

        if (aux < end)
        {
          *aux++ = ';';
        }
      }

      if (aux < end)
      {
        *aux++ = 0;
      }

      if (aux < end)
      {
        *aux++ = 0;
      }
    }
    else
    {
      strcpy(aux, "*.gba");
    }
  }

  char game_path[1024];
  game_path[0] = 0;

  OPENFILENAME cfg;

  cfg.lStructSize = sizeof(cfg);
  cfg.hwndOwner = g_mainWindow;
  cfg.hInstance = NULL;
  cfg.lpstrFilter = filter;
  cfg.lpstrCustomFilter = NULL;
  cfg.nMaxCustFilter = 0;
  cfg.nFilterIndex = 2;
  cfg.lpstrFile = game_path;
  cfg.nMaxFile = sizeof(game_path);
  cfg.lpstrFileTitle = NULL;
  cfg.nMaxFileTitle = 0;
  cfg.lpstrInitialDir = NULL;
  cfg.lpstrTitle = "Load Game";
  cfg.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  cfg.nFileOffset = 0;
  cfg.nFileExtension = 0;
  cfg.lpstrDefExt = NULL;
  cfg.lCustData = 0;
  cfg.lpfnHook = NULL;
  cfg.lpTemplateName = NULL;

  if (GetOpenFileName(&cfg) == TRUE)
  {
    _fsm.loadGame(game_path);
  }
}

void Application::enableItems(const UINT* items, size_t count, UINT enable)
{
  for (size_t i = 0; i < count; i++, items++)
  {
    EnableMenuItem(_menu, *items, enable);
  }
}

void Application::enableSlots()
{
  UINT enabled = hardcore() ? MF_DISABLED : MF_ENABLED;
  
  for (unsigned ndx = 0; ndx < 10; ndx++)
  {
    EnableMenuItem(_menu, IDM_SAVE_STATE_0 + ndx, enabled);
  }
  
  for (unsigned ndx = 0; ndx < 10; ndx++)
  {
    if ((_validSlots & (1 << ndx)) != 0)
    {
      EnableMenuItem(_menu, IDM_LOAD_STATE_0 + ndx, enabled);
    }
    else
    {
      EnableMenuItem(_menu, IDM_LOAD_STATE_0 + ndx, MF_DISABLED);
    }
  }
}

void Application::registerMemoryRegion(void* data, size_t size)
{
  _memoryRegions[_memoryRegionCount].data = (uint8_t*)data;
  _memoryRegions[_memoryRegionCount].size = size;
  _memoryRegionCount++;
  _logger.printf(RETRO_LOG_INFO, "Registered %7zu bytes at 0x%08x", size, data);
}

std::string Application::getSRamPath()
{
  std::string path = _config.getSaveDirectory();

  size_t last_slash = _gamePath.find_last_of("/");
  size_t last_bslash = _gamePath.find_last_of("\\");

  if (last_bslash > last_slash || last_slash == std::string::npos)
  {
    last_slash = last_bslash;
  }

  if (last_slash == std::string::npos)
  {
    path += _gamePath;
  }
  else
  {
    path += _gamePath.substr(last_slash);
  }
  
  path += ".sram";
  return path;
}

std::string Application::getStatePath(unsigned ndx)
{
  std::string path = _config.getSaveDirectory();

  size_t last_slash = _gamePath.find_last_of("/");
  size_t last_bslash = _gamePath.find_last_of("\\");

  if (last_bslash > last_slash || last_slash == std::string::npos)
  {
    last_slash = last_bslash;
  }

  if (last_slash == std::string::npos)
  {
    path += _gamePath;
  }
  else
  {
    path += _gamePath.substr(last_slash);
  }

  path += "-";
  path += getCoreFileName(_emulator);
  
  char index[16];
  sprintf(index, ".%03u", ndx);

  path += index;
  path += ".state";

  return path;
}

std::string Application::getCoreConfigPath(Emulator emulator)
{
  std::string path = _config.getRootFolder();
  path += "Cores\\";
  path += getCoreFileName(emulator);
  path += ".json";
  return path;
}

std::string Application::getCoreFileName(Emulator emulator)
{
  switch (emulator)
  {
  case Emulator::kNone:          break;
  case Emulator::kStella:        return "stella_libretro";
  case Emulator::kSnes9x:        return "snes9x_libretro";
  case Emulator::kPicoDrive:     return "picodrive_libretro";
  case Emulator::kGenesisPlusGx: return "genesis_plus_gx_libretro";
  case Emulator::kFceumm:        return "fceumm_libretro";
  case Emulator::kHandy:         return "handy_libretro";
  case Emulator::kBeetleSgx:     return "mednafen_supergrafx_libretro";
  case Emulator::kGambatte:      return "gambatte_libretro";
  case Emulator::kMGBA:          return "mgba_libretro";
  case Emulator::kMednafenPsx:   return "mednafen_psx_libretro";
  case Emulator::kMednafenNgp:   return "mednafen_ngp_libretro";
  }

  return "";
}

void Application::saveState(unsigned ndx)
{
  if (hardcore())
  {
    _logger.printf(RETRO_LOG_INFO, "Hardcore mode is active, can't save state");
    return;
  }

  size_t size = _core.serializeSize();
  void* data = malloc(size);

  if (data == NULL)
  {
    _logger.printf(RETRO_LOG_ERROR, "Out of memory allocating %lu bytes for the game state", size);
    return;
  }

  if (!_core.serialize(data, size))
  {
    free(data);
    return;
  }

  std::string path = getStatePath(ndx);
  _logger.printf(RETRO_LOG_INFO, "Saving state to %s", path.c_str());
  
  if (saveFile(&_logger, path.c_str(), data, size))
  {
    _validSlots |= 1 << ndx;
    enableSlots();
  }

  free(data);
}

void Application::loadState(unsigned ndx)
{
  if (hardcore())
  {
    _logger.printf(RETRO_LOG_INFO, "Hardcore mode is active, can't load state");
    return;
  }

  if ((_validSlots & (1 << ndx)) != 0)
  {
    std::string path = getStatePath(ndx);
    size_t size;
    void* data = loadFile(&_logger, path.c_str(), &size);
    
    if (data != NULL)
    {
      _core.unserialize(data, size);
      free(data);
    }
  }
}

void Application::aboutDialog()
{
  ::aboutDialog(_logger.contents().c_str());
}

void Application::handle(const SDL_SysWMEvent* syswm)
{
  if (syswm->msg->msg.win.msg == WM_PAINT)
  {
    RA_OnPaint(g_mainWindow);
  }
  else if (syswm->msg->msg.win.msg == WM_COMMAND)
  {
    WORD cmd = LOWORD(syswm->msg->msg.win.wParam);

    switch (cmd)
    {
    case IDM_SYSTEM_STELLA:
    case IDM_SYSTEM_SNES9X:
    case IDM_SYSTEM_PICODRIVE:
    case IDM_SYSTEM_GENESISPLUSGX:
    case IDM_SYSTEM_FCEUMM:
    case IDM_SYSTEM_HANDY:
    case IDM_SYSTEM_BEETLESGX:
    case IDM_SYSTEM_GAMBATTE:
    case IDM_SYSTEM_MGBA:
    case IDM_SYSTEM_MEDNAFENPSX:
    case IDM_SYSTEM_MEDNAFENNGP:
      {
        static Emulator emulators[] =
        {
          Emulator::kStella, Emulator::kSnes9x, Emulator::kPicoDrive, Emulator::kGenesisPlusGx, Emulator::kFceumm,
          Emulator::kHandy, Emulator::kBeetleSgx, Emulator::kGambatte, Emulator::kMGBA, Emulator::kMednafenPsx,
          Emulator::kMednafenNgp
        };

        _fsm.loadCore(emulators[cmd - IDM_SYSTEM_STELLA]);
      }
      
      break;
    
    case IDM_CLOSE_CORE:
      _fsm.unloadCore();
      break;

    case IDM_LOAD_GAME:
      loadGame();
      break;
    
    case IDM_PAUSE_GAME:
      _fsm.pauseGame();
      break;

    case IDM_RESUME_GAME:
      _fsm.resumeGame();
      break;
    
    case IDM_TURBO_GAME:
      _fsm.turbo();
      break;

    case IDM_RESET_GAME:
      _fsm.resetGame();
      break;
    
    case IDM_CLOSE_GAME:
      _fsm.unloadGame();
      break;

    case IDM_SAVE_STATE_0:
    case IDM_SAVE_STATE_1:
    case IDM_SAVE_STATE_2:
    case IDM_SAVE_STATE_3:
    case IDM_SAVE_STATE_4:
    case IDM_SAVE_STATE_5:
    case IDM_SAVE_STATE_6:
    case IDM_SAVE_STATE_7:
    case IDM_SAVE_STATE_8:
    case IDM_SAVE_STATE_9:
      saveState(cmd - IDM_SAVE_STATE_0);
      break;
      
    case IDM_LOAD_STATE_0:
    case IDM_LOAD_STATE_1:
    case IDM_LOAD_STATE_2:
    case IDM_LOAD_STATE_3:
    case IDM_LOAD_STATE_4:
    case IDM_LOAD_STATE_5:
    case IDM_LOAD_STATE_6:
    case IDM_LOAD_STATE_7:
    case IDM_LOAD_STATE_8:
    case IDM_LOAD_STATE_9:
      loadState(cmd - IDM_LOAD_STATE_0);
      break;
      
    case IDM_CORE:
      _config.showDialog();
      break;

    case IDM_INPUT:
      _input.showDialog();
      break;
    
    case IDM_EXIT:
      _fsm.quit();
      break;
      
    case IDM_ABOUT:
      aboutDialog();
      break;
    
    default:
      if (cmd >= IDM_RA_MENUSTART && cmd < IDM_RA_MENUEND)
      {
        RA_InvokeDialog(cmd);
      }

      break;
    }
  }
}

void Application::handle(const SDL_KeyboardEvent* key)
{
  unsigned extra;

  switch (_keybinds.translate(key, &extra))
  {
  case KeyBinds::Action::kNothing:      break;
  // Joypad buttons
  case KeyBinds::Action::kButtonUp:     _input.buttonEvent(Input::Button::kUp, extra != 0); break;
  case KeyBinds::Action::kButtonDown:   _input.buttonEvent(Input::Button::kDown, extra != 0); break;
  case KeyBinds::Action::kButtonLeft:   _input.buttonEvent(Input::Button::kLeft, extra != 0); break;
  case KeyBinds::Action::kButtonRight:  _input.buttonEvent(Input::Button::kRight, extra != 0); break;
  case KeyBinds::Action::kButtonX:      _input.buttonEvent(Input::Button::kX, extra != 0); break;
  case KeyBinds::Action::kButtonY:      _input.buttonEvent(Input::Button::kY, extra != 0); break;
  case KeyBinds::Action::kButtonA:      _input.buttonEvent(Input::Button::kA, extra != 0); break;
  case KeyBinds::Action::kButtonB:      _input.buttonEvent(Input::Button::kB, extra != 0); break;
  case KeyBinds::Action::kButtonL:      _input.buttonEvent(Input::Button::kL, extra != 0); break;
  case KeyBinds::Action::kButtonR:      _input.buttonEvent(Input::Button::kR, extra != 0); break;
  case KeyBinds::Action::kButtonL2:     _input.buttonEvent(Input::Button::kL2, extra != 0); break;
  case KeyBinds::Action::kButtonR2:     _input.buttonEvent(Input::Button::kR2, extra != 0); break;
  case KeyBinds::Action::kButtonL3:     _input.buttonEvent(Input::Button::kL3, extra != 0); break;
  case KeyBinds::Action::kButtonR3:     _input.buttonEvent(Input::Button::kR3, extra != 0); break;
  case KeyBinds::Action::kButtonSelect: _input.buttonEvent(Input::Button::kSelect, extra != 0); break;
  case KeyBinds::Action::kButtonStart:  _input.buttonEvent(Input::Button::kStart, extra != 0); break;
  // State management
  case KeyBinds::Action::kSetStateSlot: _currentSlot = extra; break;
  case KeyBinds::Action::kSaveState:    saveState(_currentSlot); break;
  case KeyBinds::Action::kLoadState:    loadState(_currentSlot); break;
  // Emulation speed
  case KeyBinds::Action::kStep:
    _fsm.step();
    break;

  case KeyBinds::Action::kPauseToggle:
    if (_fsm.currentState() == Fsm::State::GamePaused)
    {
      _fsm.resumeGame();
    }
    else
    {
      _fsm.pauseGame();
    }

    break;

  case KeyBinds::Action::kFastForward:
    if (_fsm.currentState() == Fsm::State::GameTurbo)
    {
      _fsm.normal();
    }
    else
    {
      _fsm.turbo();
    }

    break;
  }
}
