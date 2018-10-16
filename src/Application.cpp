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

#include "Application.h"
#include "Git.h"

#include <SDL_syswm.h>

#include "libretro/Core.h"
#include "jsonsax/jsonsax.h"

#include "RA_Integration/RA_Implementation/RA_Implementation.h"
#include <RA_Resource.h>

#include "About.h"
#include "KeyBinds.h"
#include "Util.h"

#include "Gl.h"
#include "GlUtil.h"

#include "resource.h"

#include <time.h>
#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#include <commdlg.h>
#include <shlobj.h>

#define TAG "[APP] "

HWND g_mainWindow;
Application app;

static unsigned char memoryRead0(unsigned addr)
{
  return app.memoryRead(0, addr);
}

static void memoryWrite0(unsigned addr, unsigned value)
{
  app.memoryWrite(0, addr, value);
}

static unsigned char memoryRead1(unsigned addr)
{
  return app.memoryRead(1, addr);
}

static void memoryWrite1(unsigned addr, unsigned value)
{
  app.memoryWrite(1, addr, value);
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
    kConfigInited,
    kAllocatorInited,
    kSdlInited,
    kWindowInited,
    kGlInited,
    kAudioDeviceInited,
    kFifoInited,
    kAudioInited,
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

  _logger.info(TAG "RALibretro version %s starting", git::getReleaseVersion());
  _logger.info(TAG "RALibretro commit hash is %s", git::getFullHash());

  if (!_config.init(&_logger))
  {
    goto error;
  }

  inited = kConfigInited;

  if (!_allocator.init(&_logger))
  {
    _logger.error(TAG "Failed to initialize the allocator");
    goto error;
  }

  inited = kAllocatorInited;

  // Setup SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
  {
    _logger.error(TAG "SDL_Init: %s", SDL_GetError());
    goto error;
  }

  inited = kSdlInited;

  // Setup window
  if (SDL_GL_LoadLibrary(NULL) != 0)
  {
    _logger.error(TAG "SDL_GL_LoadLibrary: %s", SDL_GetError());
    goto error;
  }

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  _window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  if (_window == NULL)
  {
    _logger.error(TAG "SDL_CreateWindow: %s", SDL_GetError());
    goto error;
  }

  {
    SDL_GLContext context = SDL_GL_CreateContext(_window);

    if (SDL_GL_MakeCurrent(_window, context) != 0)
    {
      _logger.error(TAG "SDL_GL_MakeCurrent: %s", SDL_GetError());
      goto error;
    }
  }

  SDL_GL_SetSwapInterval(1);

  int major, minor;
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
  _logger.info(TAG "Got OpenGL %d.%d", major, minor);

  inited = kWindowInited;

  Gl::init(&_logger);
  GlUtil::init(&_logger);

  if (!Gl::ok())
  {
    goto error;
  }

  inited = kGlInited;

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
    _logger.error(TAG "SDL_OpenAudioDevice: %s", SDL_GetError());
    goto error;
  }

  inited = kAudioDeviceInited;

  if (!_fifo.init(_audioSpec.size * 4))
  {
    _logger.error(TAG "Error initializing the audio FIFO");
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

  if (!_video.init(&_logger, &_config))
  {
    goto error;
  }

  inited = kVideoInited;

  {
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);

    if (SDL_GetWindowWMInfo(_window, &wminfo) != SDL_TRUE)
    {
      _logger.error(TAG "SDL_GetWindowWMInfo: %s", SDL_GetError());
      goto error;
    }

    g_mainWindow = wminfo.info.win.window;

    _menu = LoadMenu(NULL, "MAIN");
    SetMenu(g_mainWindow, _menu);

    RA_Init(g_mainWindow, RA_Libretro, git::getReleaseVersion());
    RA_InitShared();
    RA_AttemptLogin(true);
    RebuildMenu();
  }

  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  _emulator = Emulator::kNone;
  _validSlots = 0;
  lastHardcore = hardcore();
  loadRecentList();
  updateMenu();
  return true;

error:
  switch (inited)
  {
  case kVideoInited:       _video.destroy();
  case kKeyBindsInited:    _keybinds.destroy();
  case kInputInited:       _input.destroy();
  case kAudioInited:       _audio.destroy();
  case kFifoInited:        _fifo.destroy();
  case kAudioDeviceInited: SDL_CloseAudioDevice(_audioDev);
  case kGlInited:          // nothing to undo
  case kWindowInited:      SDL_DestroyWindow(_window);
  case kSdlInited:         SDL_Quit();
  case kAllocatorInited:   _allocator.destroy();
  case kConfigInited:      _config.destroy();
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

      case SDL_WINDOWEVENT:
        handle(&event.window);
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
    case Fsm::State::GamePausedNoOvl:
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

    if (!RA_IsOverlayFullyVisible())
    {
      Gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      _video.draw();
      SDL_GL_SwapWindow(_window);
    }

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
      else
      {
        _logger.error(TAG "GetDC(g_mainWindow) returned NULL");
      }

      t0 = t1;
    }

    SDL_Delay(1);
  }
  while (_fsm.currentState() != Fsm::State::Quit);
}

void Application::destroy()
{
  std::string json = "{\"recent\":";
  json += serializeRecentList();
  json += "}";

  util::saveFile(&_logger, getConfigPath(), json.c_str(), json.length());

  RA_Shutdown();

  _video.destroy();
  _keybinds.destroy();
  _input.destroy();
  _config.destroy();
  _audio.destroy();
  _fifo.destroy();

  SDL_CloseAudioDevice(_audioDev);
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
  path += getEmulatorFileName(emulator);
  path += ".dll";

  if (!_core.loadCore(path.c_str()))
  {
    return false;
  }

  size_t size;
  void* data = util::loadFile(&_logger, getCoreConfigPath(emulator), &size);

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
        else if (ud->key == "video")
        {
          ud->self->_video.deserialize(str);
        }
      }

      return 0;
    });

    free(data);
  }

  _emulator = emulator;
  return true;
}

void Application::updateMenu()
{
  static const UINT all_items[] =
  {
    IDM_LOAD_GAME,
    IDM_PAUSE_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_INPUT_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT start_items[] =
  {
    IDM_EXIT, IDM_ABOUT
  };

  static const UINT core_loaded_items[] =
  {
    IDM_LOAD_GAME, IDM_EXIT, IDM_CORE_CONFIG, IDM_INPUT_CONFIG, IDM_VIDEO_CONFIG, IDM_ABOUT
  };

  static const UINT game_running_items[] =
  {
    IDM_LOAD_GAME, IDM_PAUSE_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_INPUT_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_paused_items[] =
  {
    IDM_LOAD_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_INPUT_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_turbo_items[] =
  {
    IDM_LOAD_GAME, IDM_PAUSE_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_INPUT_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  enableItems(all_items, sizeof(all_items) / sizeof(all_items[0]), MF_DISABLED);

  switch (_fsm.currentState())
  {
  case Fsm::State::Start:
    enableItems(start_items, sizeof(start_items) / sizeof(start_items[0]), MF_ENABLED);
    break;
  case Fsm::State::CoreLoaded:
    enableItems(core_loaded_items, sizeof(core_loaded_items) / sizeof(core_loaded_items[0]), MF_ENABLED);
    break;
  case Fsm::State::GameRunning:
    enableItems(game_running_items, sizeof(game_running_items) / sizeof(game_running_items[0]), MF_ENABLED);
    break;
  case Fsm::State::GamePaused:
  case Fsm::State::GamePausedNoOvl:
    enableItems(game_paused_items, sizeof(game_paused_items) / sizeof(game_paused_items[0]), MF_ENABLED);
    break;
  case Fsm::State::GameTurbo:
    enableItems(game_turbo_items, sizeof(game_turbo_items) / sizeof(game_turbo_items[0]), MF_ENABLED);
    break;
  default:
    break;
  }

  enableSlots();
  enableRecent();

  if (_fsm.currentState() == Fsm::State::CoreLoaded)
  {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s %s", _core.getSystemInfo()->library_name, _core.getSystemInfo()->library_version);
    RA_UpdateAppTitle(msg);
  }
  else if (isGameActive())
  {
    char msg[256];
    snprintf(msg, sizeof(msg), "%s %s - %s", _core.getSystemInfo()->library_name, _core.getSystemInfo()->library_version, getSystemName(_system));
    RA_UpdateAppTitle(msg);
  }
  else
  {
    RA_UpdateAppTitle("");
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
    data = util::loadFile(&_logger, path, &size);

    if (data == NULL)
    {
      return false;
    }
  }

  _system = getSystem(_emulator, path, &_core);
    
  if (!_core.loadGame(path.c_str(), data, size))
  {
    if (_system == System::kNintendo)
    {
      // Assume that the FDS system is missing
      _logger.debug(TAG "Game load failure (Nintendo)");

      MessageBox(g_mainWindow, "Game load error. Are you missing a system file?", "Core Error", MB_OK);
    }

    if (data)
    {
      free(data);
    }

    return false;
  }
  
  RA_SetConsoleID((unsigned)_system);
  RA_ClearMemoryBanks();

  if (!romLoaded(&_logger, _system, path, data, size))
  {
    _core.unloadGame();

    if (data)
    {
      free(data);
    }

    return false;
  }

  if (data)
  {
    free(data);
  }

  _gamePath = path;

  for (size_t i = 0; i < _recentList.size(); i++)
  {
    const RecentItem item = _recentList[i];

    if (item.path == path && item.emulator == _emulator && item.system == _system)
    {
      _recentList.erase(_recentList.begin() + i);
      _recentList.insert(_recentList.begin(), item);
      _logger.debug(TAG "Moved recent file %zu to front \"%s\" - %u - %u", i, util::fileName(item.path).c_str(), (unsigned)item.emulator, (unsigned)item.system);
      goto moved_recent_item;
    }
  }

  if (_recentList.size() == 10)
  {
    _recentList.pop_back();
    _logger.debug(TAG "Removed last entry in the recent list");
  }

  {
    RecentItem item;
    item.path = path;
    item.emulator = _emulator;
    item.system = _system;

    _recentList.insert(_recentList.begin(), item);
    _logger.debug(TAG "Added recent file \"%s\" - %u - %u", util::fileName(item.path).c_str(), (unsigned)item.emulator, (unsigned)item.system);
  }

moved_recent_item:
  if (_core.getMemorySize(RETRO_MEMORY_SAVE_RAM) != 0)
  {
    std::string sram = getSRamPath();
    data = util::loadFile(&_logger, sram, &size);

    if (data != NULL)
    {
      if (size == _core.getMemorySize(RETRO_MEMORY_SAVE_RAM))
      {
        void* memory = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
        memcpy(memory, data, size);
      }
      else
      {
        _logger.error(TAG "Save RAM size mismatch, wanted %lu, got %lu from disk", _core.getMemorySize(RETRO_MEMORY_SAVE_RAM), size);
      }

      free(data);
    }
  }

  static uint8_t _1k[1024] = {0};

  _memoryBanks[0].count = 0;
  _memoryBanks[1].count = 0;
  unsigned numBanks = 0;

  switch (_emulator)
  {
  case Emulator::kNone:
    break;

  case Emulator::kStella:
  case Emulator::kPicoDrive:
  case Emulator::kGenesisPlusGx:
  case Emulator::kHandy:
  case Emulator::kBeetleSgx:
  case Emulator::kMednafenPsx:
  case Emulator::kMednafenNgp:
  case Emulator::kFBAlpha:
  case Emulator::kProSystem:
    data = _core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
    size = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
    registerMemoryRegion(&numBanks, 0, data, size);
    break;

  case Emulator::kSnes9x:
  case Emulator::kMednafenVb:
    data = _core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
    size = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
    registerMemoryRegion(&numBanks, 0, data, size);

    data = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
    size = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);
    registerMemoryRegion(&numBanks, 1, data, size);

    break;
  
  case Emulator::kGambatte:
    {
      const struct retro_memory_map* mmap = _core.getMemoryMap();
      struct retro_memory_descriptor* layout = new struct retro_memory_descriptor[mmap->num_descriptors + 2];
      memcpy(layout, mmap->descriptors, mmap->num_descriptors * sizeof(struct retro_memory_descriptor));

      layout[mmap->num_descriptors + 1] = {0, NULL, 0, 0x10000, 0, 0, 0, NULL};

      for (unsigned i = 0; i < mmap->num_descriptors; i++)
      {
        if (layout[i].start == 0xc000)
        {
          layout[mmap->num_descriptors] = layout[i];
          layout[mmap->num_descriptors].start = 0xe000;
          layout[mmap->num_descriptors].len = 0x1e00;
        }
        else if (mmap->descriptors[i].start == 0xa000)
        {
          if (layout[i].len > 0x2000)
          {
            layout[i].len = 0x2000;
          }
        }
      }

      struct Comparator {
        static int compare(const void* e1, const void* e2)
        {
          auto d1 = (const struct retro_memory_descriptor*)e1;
          auto d2 = (const struct retro_memory_descriptor*)e2;

          if (d1->start < d2->start)
          {
            return -1;
          }
          else if (d1->start > d2->start)
          {
            return 1;
          }
          else
          {
            return 0;
          }
        }
      };

      qsort(layout, mmap->num_descriptors + 2, sizeof(struct retro_memory_descriptor), Comparator::compare);

      size_t address = 0;

      for (unsigned i = 0; i < mmap->num_descriptors + 2; i++)
      {
        if (layout[i].start > address)
        {
          size_t fill = layout[i].start - address;

          while (fill > 1024)
          {
            registerMemoryRegion(&numBanks, 0, _1k, 1024);
            fill -= 1024;
          }

          if (fill != 0)
          {
            registerMemoryRegion(&numBanks, 0, _1k, fill);
          }
        }

        if (layout[i].len != 0)
        {
          registerMemoryRegion(&numBanks, 0, layout[i].ptr, layout[i].len);
        }

        address = layout[i].start + layout[i].len;
      }

      delete[] layout;
    }

    break;

  case Emulator::kFceumm:
    {
      const struct retro_memory_map* mmap = _core.getMemoryMap();
      void* pointer[64];

      for (unsigned i = 0; i < 64; i++)
      {
        pointer[i] = _1k;
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
        registerMemoryRegion(&numBanks, 0, pointer[i], 1024);
      }
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
          // IRAM: Internal RAM (on-chip work RAM)
          data = mmap->descriptors[i].ptr;
          size = mmap->descriptors[i].len;
          registerMemoryRegion(&numBanks, 0, data, size);
        }
        else if (mmap->descriptors[i].start == 0x02000000U)
        {
          // WRAM: On-board Work RAM
          data = mmap->descriptors[i].ptr;
          size = mmap->descriptors[i].len;
          registerMemoryRegion(&numBanks, 1, data, size);
        }
      }
    }
    else
    {
      data = _core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      size = 32768;
      registerMemoryRegion(&numBanks, 0, data, size);
    }

    break;
  }

  for (unsigned bank = 0; bank < numBanks; bank++)
  {
    MemoryBank* mb = _memoryBanks + bank;
    size_t size = 0;

    for (size_t i = 0; i < mb->count; i++)
    {
      size += mb->regions[i].size;
    }

    if (size == 0)
    {
      break;
    }

    switch (bank)
    {
    case 0:
      RA_InstallMemoryBank(0, (void*)::memoryRead0, (void*)::memoryWrite0, size);
      break;

    case 1:
      RA_InstallMemoryBank(1, (void*)::memoryRead1, (void*)::memoryWrite1, size);
      break;
    }

    _logger.info(TAG "Installed  %7zu bytes on bank %u", size, bank);
  }

  _validSlots = 0;

  for (unsigned ndx = 1; ndx <= 10; ndx++)
  {
    std::string path = getStatePath(ndx);
    struct stat statbuf;

    if (stat(path.c_str(), &statbuf) == 0)
    {
      _validSlots |= 1 << ndx;
    }
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
  json.append(",\"video\":");
  json.append(_video.serialize());
  json.append("}");

  util::saveFile(&_logger, getCoreConfigPath(_emulator), json.c_str(), json.length());

  _core.destroy();
}

void Application::resetGame()
{
  if (isGameActive())
  {
    _core.resetGame();
    RA_OnReset();
  }
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
    util::saveFile(&_logger, sram.c_str(), data, size);
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

unsigned char Application::memoryRead(unsigned bank, unsigned addr)
{
  MemoryBank* mb = _memoryBanks + bank;
  MemoryRegion* region = mb->regions;
  unsigned count = 0;

  while (count < mb->count && addr >= region->size)
  {
    addr -= region->size;
    region++;
    count++;
  }

  if (count < mb->count && region->data != NULL)
  {
    return region->data[addr];
  }

  return 0;
}

void Application::memoryWrite(unsigned bank, unsigned addr, unsigned value)
{
  MemoryBank* mb = _memoryBanks + bank;
  MemoryRegion* region = mb->regions;
  unsigned count = 0;

  while (count < mb->count && addr >= region->size)
  {
    addr -= region->size;
    region++;
    count++;
  }

  if (count < mb->count && region->data != NULL)
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
  case Fsm::State::GamePausedNoOvl:
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
      app->_logger.debug("[AUD] Audio hardware requested %d bytes, only %zu available, padding with zeroes", len, avail);
    }
    else
    {
      app->_fifo.read((void*)stream, len);
      app->_logger.debug("[AUD] Audio hardware requested %d bytes", len);
    }
  }
  else
  {
    app->_logger.debug("[AUD] Audio hardware requested %d bytes, game is not running, sending silence", len);
    memset((void*)stream, 0, len);
  }
}

void Application::loadGame()
{
  std::string path = util::openFileDialog(g_mainWindow, getEmulatorExtensions(_emulator));

  if (!path.empty())
  {
    _fsm.loadGame(path);
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
  
  for (unsigned ndx = 1; ndx <= 10; ndx++)
  {
    if ((_validSlots & (1 << ndx)) != 0)
    {
      EnableMenuItem(_menu, IDM_LOAD_STATE_1 + ndx - 1, enabled);
    }
    else
    {
      EnableMenuItem(_menu, IDM_LOAD_STATE_1 + ndx - 1, MF_DISABLED);
    }
  }
}

void Application::enableRecent()
{
  size_t i = 0;

  for (; i < _recentList.size(); i++)
  {
    UINT id = IDM_LOAD_RECENT_1 + i;
    EnableMenuItem(_menu, id, MF_ENABLED);
    
    MENUITEMINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_TYPE | MIIM_DATA;
    GetMenuItemInfo(_menu, id, false, &info);

    std::string caption = util::fileName(_recentList[i].path);
    caption += " (";
    caption += getEmulatorName(_recentList[i].emulator);
    caption += " - ";
    caption += getSystemName(_recentList[i].system);
    caption += ")";

    info.dwTypeData = (char*)caption.c_str();
    SetMenuItemInfo(_menu, id, false, &info);
  }

  for (; i < 10; i++)
  {
    UINT id = IDM_LOAD_RECENT_1 + i;
    EnableMenuItem(_menu, id, MF_DISABLED);
    
    MENUITEMINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_TYPE | MIIM_DATA;
    GetMenuItemInfo(_menu, id, false, &info);

    info.dwTypeData = (char*)"Empty";
    SetMenuItemInfo(_menu, id, false, &info);
  }
}

void Application::registerMemoryRegion(unsigned* max, unsigned bank, void* data, size_t size)
{
  if (data != NULL && size != 0)
  {
    MemoryBank* mb = _memoryBanks + bank;
    mb->regions[mb->count].data = (uint8_t*)data;
    mb->regions[mb->count].size = size;
    mb->count++;

    if ((bank + 1) > *max)
    {
      *max = bank + 1;
    }

    _logger.info(TAG "Registered %7zu bytes at 0x%08x on bank %u", size, data, bank);
  }
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
  path += getEmulatorFileName(_emulator);
  
  char index[16];
  sprintf(index, ".%03u", ndx);

  path += index;
  path += ".state";

  return path;
}

std::string Application::getConfigPath()
{
  std::string path = _config.getRootFolder();
  path += "RALibretro.json";
  return path;
}

std::string Application::getCoreConfigPath(Emulator emulator)
{
  std::string path = _config.getRootFolder();
  path += "Cores\\";
  path += getEmulatorFileName(emulator);
  path += ".json";
  return path;
}

std::string Application::getScreenshotPath()
{
  std::string path = _config.getScreenshotsFolder();

  char fname[128];
  time_t t = time(NULL);
  struct tm* tm = gmtime(&t);
  strftime(fname, sizeof(fname), "%Y-%m-%dT%H-%M-%SZ.png", tm);
  path += fname;

  return path;
}

void Application::saveState(const std::string& path)
{
  _logger.info(TAG "Saving state to %s", path.c_str());
  
  size_t size = _core.serializeSize();
  void* data = malloc(size);

  if (data == NULL)
  {
    _logger.error(TAG "Out of memory allocating %lu bytes for the game state", size);
    return;
  }

  if (!_core.serialize(data, size))
  {
    free(data);
    return;
  }

  unsigned width, height, pitch;
  enum retro_pixel_format format;
  const void* pixels = _video.getFramebuffer(&width, &height, &pitch, &format);

  if (pixels == NULL)
  {
    free(data);
    return;
  }

  if (!util::saveFile(&_logger, path.c_str(), data, size))
  {
    free((void*)pixels);
    free(data);
    return;
  }

  util::saveImage(&_logger, path + ".png", pixels, width, height, pitch, format);
  RA_OnSaveState(path.c_str());

  free((void*)pixels);
  free(data);
}

void Application::saveState(unsigned ndx)
{
  saveState(getStatePath(ndx));
  _validSlots |= 1 << ndx;
  enableSlots();
}

void Application::saveState()
{
  std::string path = util::saveFileDialog(g_mainWindow, "*.STATE\0");

  if (!path.empty())
  {
    if (util::extension(path).empty())
    {
      path += ".state";
    }

    saveState(path);
  }
}

void Application::loadState(const std::string& path)
{
  if (!RA_WarnDisableHardcore("load a state"))
  {
    _logger.warn(TAG "Hardcore mode is active, can't load state");
    return;
  }

  size_t size;
  void* data = util::loadFile(&_logger, path.c_str(), &size);

  if (data == NULL)
  {
    return;
  }

  _core.unserialize(data, size);
  free(data);
  RA_OnLoadState(path.c_str());

  unsigned width, height, pitch;
  enum retro_pixel_format format;
  _video.getFramebufferSize(&width, &height, &format);

  const void* pixels = util::loadImage(&_logger, path + ".png", &width, &height, &pitch);

  if (pixels == NULL)
  {
    _logger.error(TAG "Error loading savestate screenshot");
    return;
  }

  const void* converted = util::fromRgb(&_logger, pixels, width, height, &pitch, format);

  if (converted == NULL)
  {
    free((void*)pixels);
    _logger.error(TAG "Error converting savestate screenshot to the framebuffer format");
    return;
  }

  _video.setFramebuffer(converted, width, height, pitch);
  free((void*)converted);
  free((void*)pixels);
}

void Application::loadState(unsigned ndx)
{
  if ((_validSlots & (1 << ndx)) == 0)
  {
    return;
  }

  loadState(getStatePath(ndx));
}

void Application::loadState()
{
  std::string path = util::openFileDialog(g_mainWindow, "*.STATE\0");

  if (path.empty())
  {
    return;
  }

  loadState(path);
}

void Application::screenshot()
{
  if (!isGameActive())
  {
    _logger.warn(TAG "No active game, screenshot not taken");
    return;
  }

  unsigned width, height, pitch;
  enum retro_pixel_format format;
  const void* data = _video.getFramebuffer(&width, &height, &pitch, &format);

  if (data == NULL)
  {
    _logger.error(TAG "Error getting framebuffer from the video component");
    return;
  }

  std::string path = getScreenshotPath();
  util::saveImage(&_logger, path, data, width, height, pitch, format);
  free((void*)data);
}

void Application::aboutDialog()
{
  ::aboutDialog(_logger.contents().c_str());
}

void Application::loadRecentList()
{
  _recentList.clear();
  _logger.debug(TAG "Recent file list cleared");

  size_t size;
  void* data = util::loadFile(&_logger, getConfigPath(), &size);

  if (data != NULL)
  {
    struct Deserialize
    {
      Application* self;
      std::string key;
      RecentItem item;
    };

    Deserialize ud;
    ud.self = this;

    jsonsax_parse((char*)data, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
    {
      auto ud = (Deserialize*)udata;

      if (event == JSONSAX_KEY)
      {
        ud->key = std::string(str, num);
      }
      else if (ud->key == "recent" && event == JSONSAX_ARRAY && num == 1)
      {
        jsonsax_result_t res2 = jsonsax_parse((char*)str, ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
        {
          auto ud = (Deserialize*)udata;

          if (event == JSONSAX_KEY)
          {
            ud->key = std::string(str, num);
          }
          else if (event == JSONSAX_STRING && ud->key == "path")
          {
            ud->item.path = util::jsonUnescape(std::string(str, num));
          }
          else if (event == JSONSAX_NUMBER && ud->key == "emulator")
          {
            ud->item.emulator = (Emulator)strtoul(str, NULL, 10);
          }
          else if (event == JSONSAX_NUMBER && ud->key == "system")
          {
            ud->item.system = (System)strtoul(str, NULL, 10);
          }
          else if (event == JSONSAX_OBJECT && num == 0)
          {
            ud->self->_recentList.push_back(ud->item);
            ud->self->_logger.debug(TAG "Added recent file \"%s\" - %u - %u", util::fileName(ud->item.path).c_str(), (unsigned)ud->item.emulator, (unsigned)ud->item.system);
          }

          return 0;
        });

        if (res2 != JSONSAX_OK)
        {
          return -1;
        }
      }

      return 0;
    });

    free(data);
  }
}

std::string Application::serializeRecentList()
{
  std::string json = "[";

  for (unsigned i = 0; i < _recentList.size(); i++)
  {
    char buffer[64];

    if (i != 0)
    {
      json += ",";
    }

    json += "{";
    json += "\"path\":\"";
    json += util::jsonEscape(_recentList[i].path);

    json += "\",\"emulator\":";
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)_recentList[i].emulator);
    json += buffer;

    json += ",\"system\":";
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)_recentList[i].system);
    json += buffer;

    json += "}";
  }

  json += "]";
  return json;
}

void Application::resizeWindow(unsigned multiplier)
{
  Uint32 active = SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
  
  if (!active)
  {
    unsigned width, height;
    enum retro_pixel_format format;
    _video.getFramebufferSize(&width, &height, &format);
    SDL_SetWindowSize(_window, width * multiplier, height * multiplier);
  }
}

void Application::toggleFullscreen()
{
  Uint32 active = SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
  SDL_SetWindowFullscreen(_window, active ^ SDL_WINDOW_FULLSCREEN_DESKTOP);
  
  if (active)
  {
    SetMenu(g_mainWindow, _menu);
    SDL_ShowCursor(SDL_ENABLE);
  }
  else
  {
    SetMenu(g_mainWindow, NULL);
    SDL_ShowCursor(SDL_DISABLE);
  }
}

void Application::handle(const SDL_SysWMEvent* syswm)
{
  if (syswm->msg->msg.win.msg == WM_COMMAND)
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
    case IDM_SYSTEM_MEDNAFENVB:
    case IDM_SYSTEM_FBALPHA:
    case IDM_SYSTEM_PROSYSTEM:
      {
        static Emulator emulators[] =
        {
          Emulator::kStella, Emulator::kSnes9x, Emulator::kPicoDrive, Emulator::kGenesisPlusGx, Emulator::kFceumm,
          Emulator::kHandy, Emulator::kBeetleSgx, Emulator::kGambatte, Emulator::kMGBA, Emulator::kMednafenPsx,
          Emulator::kMednafenNgp, Emulator::kMednafenVb, Emulator::kFBAlpha, Emulator::kProSystem
        };

        _fsm.loadCore(emulators[cmd - IDM_SYSTEM_STELLA]);
      }
      
      break;
    
    case IDM_LOAD_GAME:
      loadGame();
      break;
    
    case IDM_LOAD_RECENT_1:
    case IDM_LOAD_RECENT_2:
    case IDM_LOAD_RECENT_3:
    case IDM_LOAD_RECENT_4:
    case IDM_LOAD_RECENT_5:
    case IDM_LOAD_RECENT_6:
    case IDM_LOAD_RECENT_7:
    case IDM_LOAD_RECENT_8:
    case IDM_LOAD_RECENT_9:
    case IDM_LOAD_RECENT_10:
      _fsm.loadRecent(_recentList[cmd - IDM_LOAD_RECENT_1].emulator, _recentList[cmd - IDM_LOAD_RECENT_1].path);
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
    
    case IDM_SAVE_STATE_1:
    case IDM_SAVE_STATE_2:
    case IDM_SAVE_STATE_3:
    case IDM_SAVE_STATE_4:
    case IDM_SAVE_STATE_5:
    case IDM_SAVE_STATE_6:
    case IDM_SAVE_STATE_7:
    case IDM_SAVE_STATE_8:
    case IDM_SAVE_STATE_9:
    case IDM_SAVE_STATE_10:
      saveState(cmd - IDM_SAVE_STATE_1 + 1);
      break;
      
    case IDM_LOAD_STATE_1:
    case IDM_LOAD_STATE_2:
    case IDM_LOAD_STATE_3:
    case IDM_LOAD_STATE_4:
    case IDM_LOAD_STATE_5:
    case IDM_LOAD_STATE_6:
    case IDM_LOAD_STATE_7:
    case IDM_LOAD_STATE_8:
    case IDM_LOAD_STATE_9:
    case IDM_LOAD_STATE_10:
      loadState(cmd - IDM_LOAD_STATE_1 + 1);
      break;
    
    case IDM_LOAD_STATE:
      loadState();
      break;
    
    case IDM_SAVE_STATE:
      saveState();
      break;
      
    case IDM_CORE_CONFIG:
      _config.showDialog();
      break;

    case IDM_INPUT_CONFIG:
      _input.showDialog();
      break;
    
    case IDM_VIDEO_CONFIG:
      _video.showDialog();
      break;
    
    case IDM_WINDOW_1X:
    case IDM_WINDOW_2X:
    case IDM_WINDOW_3X:
    case IDM_WINDOW_4X:
      resizeWindow(cmd - IDM_WINDOW_1X + 1);
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

void Application::handle(const SDL_WindowEvent* window)
{
  if (window->event == SDL_WINDOWEVENT_SIZE_CHANGED)
  {
    _video.windowResized(window->data1, window->data2);
  }
}

void Application::handle(const SDL_KeyboardEvent* key)
{
  unsigned extra;

  switch (_keybinds.translate(key, &extra))
  {
  case KeyBinds::Action::kNothing:          break;
  // Joypad buttons
  case KeyBinds::Action::kButtonUp:         _input.buttonEvent(Input::Button::kUp, extra != 0); break;
  case KeyBinds::Action::kButtonDown:       _input.buttonEvent(Input::Button::kDown, extra != 0); break;
  case KeyBinds::Action::kButtonLeft:       _input.buttonEvent(Input::Button::kLeft, extra != 0); break;
  case KeyBinds::Action::kButtonRight:      _input.buttonEvent(Input::Button::kRight, extra != 0); break;
  case KeyBinds::Action::kButtonX:          _input.buttonEvent(Input::Button::kX, extra != 0); break;
  case KeyBinds::Action::kButtonY:          _input.buttonEvent(Input::Button::kY, extra != 0); break;
  case KeyBinds::Action::kButtonA:          _input.buttonEvent(Input::Button::kA, extra != 0); break;
  case KeyBinds::Action::kButtonB:          _input.buttonEvent(Input::Button::kB, extra != 0); break;
  case KeyBinds::Action::kButtonL:          _input.buttonEvent(Input::Button::kL, extra != 0); break;
  case KeyBinds::Action::kButtonR:          _input.buttonEvent(Input::Button::kR, extra != 0); break;
  case KeyBinds::Action::kButtonL2:         _input.buttonEvent(Input::Button::kL2, extra != 0); break;
  case KeyBinds::Action::kButtonR2:         _input.buttonEvent(Input::Button::kR2, extra != 0); break;
  case KeyBinds::Action::kButtonL3:         _input.buttonEvent(Input::Button::kL3, extra != 0); break;
  case KeyBinds::Action::kButtonR3:         _input.buttonEvent(Input::Button::kR3, extra != 0); break;
  case KeyBinds::Action::kButtonSelect:     _input.buttonEvent(Input::Button::kSelect, extra != 0); break;
  case KeyBinds::Action::kButtonStart:      _input.buttonEvent(Input::Button::kStart, extra != 0); break;
  // State management
  case KeyBinds::Action::kSaveState:        saveState(extra); break;
  case KeyBinds::Action::kLoadState:        loadState(extra); break;
  // Window size
  case KeyBinds::Action::kSetWindowSize1:   resizeWindow(1); break;
  case KeyBinds::Action::kSetWindowSize2:   resizeWindow(2); break;
  case KeyBinds::Action::kSetWindowSize3:   resizeWindow(3); break;
  case KeyBinds::Action::kSetWindowSize4:   resizeWindow(4); break;
  case KeyBinds::Action::kToggleFullscreen: toggleFullscreen(); break;
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

  case KeyBinds::Action::kPauseToggleNoOvl:
    if (_fsm.currentState() == Fsm::State::GamePausedNoOvl)
    {
      _fsm.resumeGame();
    }
    else
    {
      _fsm.pauseGameNoOvl();
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
  
  case KeyBinds::Action::kScreenshot:
    screenshot();
    break;
  }
}

