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
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Application.h"
#include "Git.h"

#include <SDL_syswm.h>

#include "libretro/Core.h"
#include "jsonsax/jsonsax.h"

#include "RA_Interface.h"

#include "About.h"
#include "CdRom.h"
#include "KeyBinds.h"
#include "Hash.h"
#include "Util.h"

#include "Gl.h"
#include "GlUtil.h"

#include "resource.h"

#include <assert.h>
#include <chrono>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#include <commdlg.h>
#include <shlobj.h>

#define TAG "[APP] "

//#define DISPLAY_FRAMERATE 1

//#define DEBUG_AUDIO 1

HWND g_mainWindow;
Application app;

#define CDROM_MENU_INDEX 10

static void s_onRotationChanged(Video::Rotation oldRotation, Video::Rotation newRotation)
{
  app.onRotationChanged(oldRotation, newRotation);
}

Application::Application(): _fsm(*this)
{
  _components.logger       = &_logger;
  _components.config       = &_config;
  _components.videoContext = &_videoContext;
  _components.video        = &_video;
  _components.audio        = &_audio;
  _components.microphone   = &_microphone;
  _components.input        = &_input;
  _components.allocator    = &_allocator;
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
    kKeyBindsInited,
    kWindowInited,
    kAudioDeviceInited,
    kFifoInited,
    kAudioInited,
    kInputInited,
    kVideoContextInited,
    kGlInited,
    kVideoInited
  }
  inited = kNothingInited;

  _config.initRootFolder();

  if (!_logger.init(_config.getRootFolder()))
  {
    goto error;
  }

  inited = kLoggerInited;

#if defined(_M_X64) || defined(__amd64__)
  _logger.info(TAG "RALibretro version %s-x64 starting", git::getReleaseVersion());
#else
  _logger.info(TAG "RALibretro version %s starting", git::getReleaseVersion());
#endif
  _logger.info(TAG "RALibretro commit hash is %s", git::getFullHash());

  {
    const time_t now_timet = time(0);
    std::tm now_tm;
    localtime_s(&now_tm, &now_timet);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%m/%d/%y %H:%M:%S %Z", &now_tm);
    _logger.info(TAG "Current time is %s", buffer);
  }

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

  // keybinds must be initialized before calling loadConfiguration
  if (!_keybinds.init(&_logger))
  {
    goto error;
  }

  inited = kKeyBindsInited;

  // states must be initialized before calling loadConfiguration
  if (!_states.init(&_logger, &_config, &_video))
  {
    goto error;
  }

  // Load the configuration from previous runs - primarily looking for the window size/location.
  {
    int window_x = SDL_WINDOWPOS_CENTERED, window_y = SDL_WINDOWPOS_CENTERED;

    loadConfiguration(&window_x, &window_y, &width, &height);
    if (window_y != SDL_WINDOWPOS_CENTERED)
    {
      // captured window position includes menu bar, which won't exist at initial positioning
      // adjust for it.
      window_y -= GetSystemMetrics(SM_CYMENU);
    }

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
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    _window = SDL_CreateWindow(title, window_x, window_y, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  }

  if (_window == NULL)
  {
    _logger.error(TAG "SDL_CreateWindow: %s", SDL_GetError());
    goto error;
  }
  else
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

    SDL_SetWindowSize(_window, width, height);

    if (_config.getBackgroundInput())
      setBackgroundInput(true);
  }

  inited = kWindowInited;

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

  _logger.info(TAG "Initialized audio device. %d channels@%dHz (format:%04X, silence:%d, samples:%d, padding:%d, size:%d)",
    _audioSpec.channels, _audioSpec.freq, _audioSpec.format, _audioSpec.silence, _audioSpec.samples, _audioSpec.padding, _audioSpec.size);

  inited = kAudioDeviceInited;

  if (!_fifo.init(_audioSpec.size * 4))
  {
    _logger.error(TAG "Error initializing the audio FIFO");
    goto error;
  }

  inited = kFifoInited;

  SDL_PauseAudioDevice(_audioDev, 0);

  // Initialize the rest of the components
  if (!_audio.init(&_logger, (double)_audioSpec.freq, _audioSpec.channels, &_fifo))
  {
    goto error;
  }

  if (!_microphone.init(&_logger))
  {
    goto error;
  }

  inited = kAudioInited;

  if (!_input.init(&_logger))
  {
    goto error;
  }

  inited = kInputInited;

  if (!_videoContext.init(&_logger, _window))
  {
    goto error;
  }

  inited = kVideoContextInited;

  Gl::init(&_logger);
  GlUtil::init(&_logger);

  if (!Gl::ok())
  {
    goto error;
  }

  inited = kGlInited;

  if (!_video.init(&_logger, &_videoContext, &_config))
  {
    goto error;
  }
  _video.setRotationChangedHandler(s_onRotationChanged);

  inited = kVideoInited;

  {
    _cdRomMenu = GetSubMenu(GetSubMenu(_menu, 0), CDROM_MENU_INDEX);
    assert(GetMenuItemID(_cdRomMenu, 0) == IDM_CD_OPEN_TRAY);

    if (!loadCores(&_config, &_logger))
    {
      MessageBox(g_mainWindow, "Could not open Cores\\cores.json.", "Initialization failed", MB_OK);
      goto error;
    }

    buildSystemsMenu();

    extern void RA_Init(HWND hwnd);
    RA_Init(g_mainWindow);
  }

  if (!_memory.init(&_logger))
  {
    goto error;
  }

  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

  _coreName.clear();
  _gameData = NULL;
  _validSlots = 0;
  _isDriveFloppy = false;
  lastHardcore = hardcore();
  cancelLoad = false;
  _absViewMouseX = _absViewMouseY = 0;

  updateMenu();
  updateDiscMenu(true);
  return true;

error:
  switch (inited)
  {
  case kVideoInited:        _video.destroy();
  case kGlInited:           // nothing to undo
  case kVideoContextInited: _videoContext.destroy();
  case kInputInited:        _input.destroy();
  case kAudioInited:        _audio.destroy();
  case kFifoInited:         _fifo.destroy();
  case kAudioDeviceInited:  _microphone.destroy();
                            SDL_CloseAudioDevice(_audioDev);
  case kWindowInited:       SDL_DestroyWindow(_window);
  case kKeyBindsInited:     _keybinds.destroy();
  case kSdlInited:          SDL_Quit();
  case kAllocatorInited:    _allocator.destroy();
  case kConfigInited:       _config.destroy();
  case kLoggerInited:       _logger.destroy();
  case kNothingInited:      break;
  }

  _coreName.clear();
  return false;
}

void Application::processEvents()
{
  SDL_Event event;
  if (!SDL_PollEvent(&event))
    return;

  do
  {
    switch (event.type)
    {
      case SDL_QUIT:
        _fsm.quit();
        break;

      case SDL_CONTROLLERDEVICEADDED:
      case SDL_CONTROLLERDEVICEREMOVED:
        _input.processEvent(&event, &_keybinds, isGameActive() ? &_video : nullptr);
        break;

      case SDL_CONTROLLERBUTTONUP:
      case SDL_CONTROLLERBUTTONDOWN:
      {
        unsigned extra;
        KeyBinds::Action action = _keybinds.translate(&event.cbutton, &extra);
        handle(action, extra);
        break;
      }

      case SDL_CONTROLLERAXISMOTION:
      {
        KeyBinds::Action action1, action2;
        unsigned extra1, extra2;
        _keybinds.translate(&event.caxis, _input, &action1, &extra1, &action2, &extra2);
        if (action1 != action2)
          handle(action1, extra1);
        handle(action2, extra2);
        break;
      }

      case SDL_MOUSEMOTION:
        handle(&event.motion);
        break;

      case SDL_MOUSEBUTTONUP:
      case SDL_MOUSEBUTTONDOWN:
        handle(&event.button);
        break;

      case SDL_SYSWMEVENT:
        handle(&event.syswm);
        break;

      case SDL_KEYUP:
      case SDL_KEYDOWN:
      {
        unsigned extra;
        KeyBinds::Action action = _keybinds.translate(&event.key, &extra);
        handle(action, extra);
        break;
      }

      case SDL_WINDOWEVENT:
        handle(&event.window);
        break;
    }
  } while (SDL_PollEvent(&event));

  if (hardcore() != lastHardcore)
  {
    lastHardcore = hardcore();
    updateMenu();
  }
}

void Application::pauseForBadPerformance()
{
  if (hardcore())
  {
    _fsm.pauseGame();

    MessageBox(g_mainWindow, "Game has been paused.\n\n"
      "Your system doesn't appear to be able to run this core at the desired speed. Consider changing some of the settings for the core.",
      "Performance Problem Detected", MB_OK);
  }
}

void Application::runTurbo()
{
  const auto tTurboStart = std::chrono::steady_clock::now();

  // disable per-frame rendering in the toolkit
  RA_SuspendRepaint();

  // don't wait for audio buffer to flush - only fill whatever space is available
  _audio.setBlocking(false);

  // do some frames without video
  const bool playAudio = _config.getAudioWhileFastForwarding();
  const int nSkipFrames = _config.getFastForwardRatio() - 1;
  for (int i = 0; i < nSkipFrames; i++)
  {
    _core.step(false, playAudio);
    RA_DoAchievementsFrame();
  }

  // do a final frame with video
  _core.step(true, playAudio);
  RA_DoAchievementsFrame();

  // allow normal audio processing
  _audio.setBlocking(true);

  // restore per-frame rendering in the toolkit
  RA_ResumeRepaint();

  // check for periodic SRAM flush
  _states.periodicSaveSRAM(&_core);

  const auto tTurboEnd = std::chrono::steady_clock::now();
  const auto tTurboElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tTurboEnd - tTurboStart);

  // throttle turbo to a maximum of 300 frames per second to simulate vsync in case its off
  if (tTurboElapsed < std::chrono::milliseconds(15))
  {
    SDL_Delay((Uint32)((std::chrono::milliseconds(15) - tTurboElapsed).count()));
  }
}

void Application::runSmoothed()
{
  int numFrames = 0;
  auto tFirstFrame = std::chrono::steady_clock::now();
  int numFaults = 0;

  do
  {
    _processingEvents = true;
    processEvents();
    _processingEvents = false;
    if (_fsm.currentState() != Fsm::State::GameRunning)
      return;

    if (_config.getFastForwarding())
    {
      // do five frames without audio
      runTurbo();
      numFrames += 5;
    }
    else
    {
      // do one frame with audio
      _core.step(true, true);
      RA_DoAchievementsFrame();

      _audioGeneratedDuringFastForward = 0;
      ++numFrames;
    }

    if (numFrames > 50)
    {
      const auto tNow = std::chrono::steady_clock::now();
      const auto tElapsed = std::chrono::duration_cast<std::chrono::microseconds>(tNow - tFirstFrame).count();
      const uint32_t fps = (uint32_t)(((long long)numFrames * 1000000) / (tElapsed / 100));

      if (_config.getFastForwarding())
      {
        if (fps < 2000)
        {
          if (++numFaults == 5)
          {
            pauseForBadPerformance();
            numFaults = 0;
          }
        }
        else if (numFaults > 0)
        {
          numFaults--;
        }
      }

#ifdef DISPLAY_FRAMERATE
      char buffer[256];
      GetWindowText(g_mainWindow, buffer, sizeof(buffer));
      char* ptr = buffer + strlen(buffer);
      if (ptr[-3] == 'f' && ptr[-2] == 'p' && ptr[-1] == 's')
      {
        ptr -= 3;
        while (*ptr != '-')
          --ptr;
        ptr--;
      }

      sprintf(ptr, " - %u.%02ufps", fps / 100, fps % 100);
      SetWindowText(g_mainWindow, buffer);
#endif

      numFrames = 0;
      tFirstFrame = tNow;
    }

  } while (true);
}

void Application::run()
{
  _video.clear();

  try
  {
    do
    {
      // handle any pending events
      processEvents();

      switch (_fsm.currentState())
      {
        case Fsm::State::GameRunning:
        {
          runSmoothed();
          continue;
        }

        case Fsm::State::GamePaused:
        case Fsm::State::GamePausedNoOvl:
        default:
          // do no frames
          Sleep(1000 / 60); // assume 60fps

          // if a message is on-screen, force repaint to advance the message animation
          if (_video.hasMessage())
          {
            _video.redraw();

            // no more on-screen messages, force repaint to clear the popups
            if (!_video.hasMessage())
              _video.redraw();
          }
          break;

        case Fsm::State::FrameStep:
          // do one frame without audio
          _core.step(true, false);
          RA_DoAchievementsFrame();

          // set state to GamePaused
          _fsm.resumeGame();
          break;

        case Fsm::State::Quit:
          return;
      }

      // handle overlay navigation
      if (RA_IsOverlayFullyVisible())
      {
        ControllerInput input;
        input.m_bUpPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) != 0;
        input.m_bDownPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0;
        input.m_bLeftPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) != 0;
        input.m_bRightPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0;
        input.m_bConfirmPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) != 0;
        input.m_bCancelPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) != 0;
        input.m_bQuitPressed = _input.read(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) != 0;

        RA_NavigateOverlay(&input);
      }
    } while (true);
  }
  catch (std::exception& ex) {
    _logger.error(TAG "Unhandled exception: %s", ex.what());
  }
}

void Application::saveConfiguration()
{
  std::string json = "{";

  // emulator settings
  json += "\"emulator\":";
  json += _config.serializeEmulatorSettings();

  // recent items
  json += ",\"recent\":";
  json += serializeRecentList();

  // bindings
  json += ",\"bindings\":";
  json += _keybinds.serializeBindings();

  // saves
  json += ",\"saves\":";
  json += _states.serializeSettings();

  // window position
  const Uint32 flags = SDL_GetWindowFlags(_window);
  if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
  {
    // it doesn't make sense to save fullscreen mode as the player must load a game when they restart
    // the application, and the window position/size will be 0,0 and the desktop resolution
  }
  else
  {
    json += ",\"window\":{";

    int x, y;
    SDL_GetWindowPosition(_window, &x, &y);
    json += "\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y);

    SDL_GetWindowSize(_window, &x, &y);
    switch (_video.getRotation())
    {
      case Video::Rotation::Ninety:
      case Video::Rotation::TwoSeventy:
      {
        int tmp = x;
        x = y;
        y = tmp;
        break;
      }

      default:
        break;
    }

    json += ",\"w\":" + std::to_string(x) + ",\"h\":" + std::to_string(y);

    json += "}";
  }

  // video settings
  json.append(",\"video\":");
  json.append(_video.serializeSettings());

  // complete and save
  json += "}";

  util::saveFile(&_logger, getConfigPath(), json.c_str(), json.length());
}

void Application::destroy()
{
  _logger.info(TAG "begin shutdown");

  saveConfiguration();

  RA_Shutdown();

  if (_gameData)
    free(_gameData);

  _video.destroy();
  _keybinds.destroy();
  _input.destroy();
  _config.destroy();
  _microphone.destroy();
  _audio.destroy();
  _fifo.destroy();

  SDL_CloseAudioDevice(_audioDev);
  SDL_DestroyWindow(_window);
  SDL_Quit();

  _allocator.destroy();

  _logger.info(TAG "shutdown complete");
  _logger.destroy();
}

bool Application::loadCore(const std::string& coreName)
{
  _config.reset();
  _input.reset();

  _video.setRotation(Video::Rotation::None);

  /* attempt to update save path before loading the core - some cores get the save directory before loading a game */
  _states.setGame("", _system, coreName, &_core);

  if (!_core.init(&_components))
  {
    return false;
  }

  std::string path = _config.getRootFolder();
  path += "Cores\\";
  path += coreName;
  path += ".dll";

  // open the core and fetch all the hooks
  if (!_core.loadCore(path.c_str()))
  {
    std::string message = "Could not load ";
    message += coreName;
    MessageBox(g_mainWindow, message.c_str(), "Failed", MB_OK);
    return false;
  }

  // let the user know if the core is no longer supported
  const std::string* deprecationMessage = getCoreDeprecationMessage(coreName);
  if (deprecationMessage)
  {
    MessageBox(g_mainWindow, deprecationMessage->c_str(), "Warning", MB_OK | MB_ICONWARNING);
  }

  // read the settings for the core
  size_t size;
  void* data = util::loadFile(&_logger, getCoreConfigPath(coreName), &size);

  if (data != NULL)
  {
    struct Deserialize
    {
      Application* self;
      std::string key;
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
  else
  {
    if (coreName == "ppsspp_libretro")
    {
      // PPSSPP stutters if frame duplication is not enabled
      // see https://github.com/hrydgard/ppsspp/issues/14748
      _config.deserialize("{\"ppsspp_frame_duplication\":\"enabled\"}");
    }
    else if (coreName == "flycast_libretro")
    {
      // flycast crashes on save/load state when using threaded rendering
      _config.deserialize("{\"reicast_threaded_rendering\":\"disabled\"}");
    }
    else if (coreName == "fbneo_libretro")
    {
      // this setting must be off for hardcore mode, but defaults to enabled
      _config.deserialize("{\"fbneo-allow-patched-romsets\":\"disabled\"}");
    }
    else if (coreName == "wasm4_libretro")
    {
      // we don't currently support RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK
      _config.deserialize("{\"wasm4_audio_type\":\"normal\"}");
    }
  }

  // tell the core to startup (must be done after reading configs)
  if (!_core.initCore())
  {
    std::string message = "Could not initialize ";
    message += coreName;
    MessageBox(g_mainWindow, message.c_str(), "Failed", MB_OK);
    return false;
  }

  // success - perform bookkeeping
  _coreName = coreName;

  std::string coreDetail = coreName + "/";
  const char* scan = _core.getSystemInfo()->library_version;
  while (*scan)
  {
    if (*scan == ' ')
      coreDetail.push_back('_');
    else
      coreDetail.push_back(*scan);

    ++scan;
  }
  RA_SetUserAgentDetail(coreDetail.c_str());

  MENUITEMINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = MIIM_TYPE | MIIM_DATA;

  switch (_system)
  {
  case RC_CONSOLE_AMIGA:
  case RC_CONSOLE_AMSTRAD_PC:
  case RC_CONSOLE_APPLE_II:
  case RC_CONSOLE_ATARI_ST:
  case RC_CONSOLE_COMMODORE_64:
  case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
  case RC_CONSOLE_MS_DOS:
  case RC_CONSOLE_MSX:
  case RC_CONSOLE_NINTENDO: // FDS
  case RC_CONSOLE_ORIC:
  case RC_CONSOLE_PC6000:
  case RC_CONSOLE_PC8800:
  case RC_CONSOLE_PC9800:
  case RC_CONSOLE_SHARPX1:
  case RC_CONSOLE_VIC20:
  case RC_CONSOLE_ZX_SPECTRUM:
    info.dwTypeData = (LPSTR)"Insert Disk";
    SetMenuItemInfo(_menu, IDM_CD_OPEN_TRAY, false, &info);
    info.dwTypeData = (LPSTR)"Floppy Drive";
    SetMenuItemInfo(GetSubMenu(_menu, 0), CDROM_MENU_INDEX, true, &info);
    _isDriveFloppy = true;
    break;

  default:
    info.dwTypeData = (LPSTR)"Close Tray";
    SetMenuItemInfo(_menu, IDM_CD_OPEN_TRAY, false, &info);
    info.dwTypeData = (LPSTR)"CD-ROM";
    SetMenuItemInfo(GetSubMenu(_menu, 0), CDROM_MENU_INDEX, true, &info);
    _isDriveFloppy = false;
    break;
  }
  EnableMenuItem(_cdRomMenu, IDM_CD_OPEN_TRAY, MF_DISABLED);

  size_t menuItemCount = GetMenuItemCount(_cdRomMenu);
  while (menuItemCount > 1)
    DeleteMenu(_cdRomMenu, --menuItemCount, MF_BYPOSITION);

  return true;
}

bool Application::validateHardcoreEnablement()
{
  if (_config.validateSettingsForHardcore(_core.getSystemInfo()->library_name, _system, false))
    return true;

#if defined(MINGW) || defined(__MINGW32__) || defined(__MINGW64__)
  RA_DisableHardcore();
#else
  __try
  {
    // This functionality is not available in the 0.78 DLL, but is implemented in such a way that
    // it throws an exception we can catch and handle.
    RA_DisableHardcore();
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    MessageBox(g_mainWindow, "Could not prevent switch to hardcore. Closing game.", "Failed", MB_OK);

    // if this gets called during the load (auto-enable hardcore when achievements are present), the
    // FSM won't transition properly. set a flag so we can deal with it when we get done loading.
    cancelLoad = true;

    _fsm.unloadGame();
  }
#endif

  return false;
}

void Application::updateMenu()
{
  static const UINT all_items[] =
  {
    IDM_LOAD_GAME,
    IDM_PAUSE_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT start_items[] =
  {
    IDM_EXIT, IDM_ABOUT
  };

  static const UINT core_loaded_items[] =
  {
    IDM_LOAD_GAME, IDM_EXIT, IDM_CORE_CONFIG, IDM_ABOUT
  };

  static const UINT game_running_items[] =
  {
    IDM_LOAD_GAME, IDM_PAUSE_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_paused_items[] =
  {
    IDM_LOAD_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
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
    snprintf(msg, sizeof(msg), "%s %s - %s%s", _core.getSystemInfo()->library_name, _core.getSystemInfo()->library_version,
      getSystemName(_system), _keybinds.hasGameFocus() ? " [Game Focus]" : "");
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
  std::string unzippedFileName;
  std::string extension;
  const char* ptr;
  size_t size;
  void* data;
  bool loaded;
  bool iszip = (path.length() > 4 && stricmp(&path.at(path.length() - 4), ".zip") == 0);
  bool issupportedzip = false;

  /* make sure none of the forbidden settings are set */
  if (!_config.validateSettingsForHardcore(_core.getSystemInfo()->library_name, _system, true))
  {
    MessageBox(g_mainWindow, "Game load was canceled.", "Failed", MB_OK);
    return false;
  }

  /* if the core says it wants the full path, we have to see if it supports zip files */
  if (iszip && info->need_fullpath)
  {
    ptr = getEmulatorExtensions(_coreName, _system);
    if (ptr == NULL)
      ptr = info->valid_extensions;

    while (*ptr)
    {
      if (strnicmp(ptr, "zip", 3) == 0 && (ptr[3] == '\0' || ptr[3] == '|'))
      {
        issupportedzip = true;
        break;
      }

      while (*ptr && *ptr != '|')
      {
        ++ptr;
      }
      if (*ptr == '|')
      {
        ++ptr;
      }
    }

    if (!issupportedzip)
    {
      _logger.debug(TAG "%s does not support zip files", info->library_name);

      std::string error = info->library_name;
      error += " does not support zip files";
      MessageBox(g_mainWindow, error.c_str(), "Core Error", MB_OK);

      return false;
    }

    /* when a core needs fullpath for a zip file, RetroArch unzips the zip file (unless blocked by the core) */
    if (!info->block_extract)
    {
      _logger.info(TAG "%s requires uncompressed content - extracting", info->library_name);

      data = util::loadZippedFile(&_logger, path, &size, unzippedFileName);
      if (data == NULL)
      {
        MessageBox(g_mainWindow, "Unable to open file", "Error", MB_OK);
        return false;
      }

      if (unzippedFileName.empty())
      {
        MessageBox(g_mainWindow, "Could not determine which file to extract from zip", "Error", MB_OK);
        return false;
      }

      std::string newPath = util::replaceFileName(path, unzippedFileName.c_str());
      util::saveFile(&_logger, newPath, data, size);
      free(data);

      if (!loadGame(newPath))
      {
        util::deleteFile(newPath);
        return false;
      }

      _gamePathIsTemporary = true;
      return true;
    }
  }

  if (issupportedzip)
  {
    /* user is loading a zip file, and the core supports it */
    size = 0;
    data = NULL;
  }
  else
  {
    extension = util::extension(path);
    if (extension.front() == '.')
      extension.erase(extension.begin()); /* remove leading period */

    if (_core.getNeedsFullPath(extension))
    {
      /* core wants the full path, don't load the data unless we need it to hash */
      size = 0;
      data = NULL;
    }
    else
    {
      if (iszip)
      {
        /* core doesn't support zip files, unzip it into a buffer */
        data = util::loadZippedFile(&_logger, path, &size, unzippedFileName);
      }
      else
      {
        /* load the file into a buffer so we can hash it */
        data = util::loadFile(&_logger, path, &size);
      }

      if (data == NULL)
      {
        MessageBox(g_mainWindow, "Unable to open file", "Error", MB_OK);
        return false;
      }
    }
  }

  if (unzippedFileName.empty())
  {
    /* did not extract a file from an archive, just use the original file name */
    unzippedFileName = util::fileNameWithExtension(path);
  }

  /* must update save path before loading the game */
  _states.setGame(unzippedFileName, _system, _coreName, &_core);

  extension = util::extension(unzippedFileName);
  if (extension.front() == '.')
    extension.erase(extension.begin()); /* remove leading period */

  std::string errorBuffer;
  if (_core.getNeedsFullPath(extension))
  {
    loaded = _core.loadGame(path.c_str(), NULL, 0, &errorBuffer);
  }
  else if (iszip)
  {
    std::string zipPsuedoPath = path + '#' + unzippedFileName;
    loaded = _core.loadGame(zipPsuedoPath.c_str(), data, size, &errorBuffer);
  }
  else
  {
    loaded = _core.loadGame(path.c_str(), data, size, &errorBuffer);
  }

  if (!loaded)
  {
    // The most common cause of failure is missing system files.
    _logger.debug(TAG "Game load failure (%s)", info ? info->library_name : "Unknown");

    if (!errorBuffer.empty())
    {
      errorBuffer = "Game load error.\n\n" + errorBuffer;
      MessageBox(g_mainWindow, errorBuffer.c_str(), "Core Error", MB_OK);
    }
    else
    {
      MessageBox(g_mainWindow, "Game load error.\n\nPlease ensure that required system files are present and restart.", "Core Error", MB_OK);
    }

    if (data)
      free(data);

    // A failure in loadGame typically destroys the core.
    if (_core.getSystemInfo()->library_name == NULL)
      _fsm.unloadCore();

    return false;
  }

  /* calling loadGame may change one or more settings - revalidate */
  if (!_config.validateSettingsForHardcore(_core.getSystemInfo()->library_name, _system, true))
  {
    if (data)
      free(data);

    _core.unloadGame();

    MessageBox(g_mainWindow, "Game load was canceled.", "Failed", MB_OK);
    return false;
  }

  RA_SetConsoleID((unsigned)_system);
  RA_ClearMemoryBanks();

  _gameFileName = unzippedFileName; // store for GetEstimatedTitle callback

  if (!romLoaded(&_core, &_logger, _system, path, data, size, false))
  {
    _discPaths.clear();
    updateDiscMenu(true);

    _gameFileName.clear();

    _core.unloadGame();

    if (data)
      free(data);

    return false;
  }

  if (data)
  {
    if (_core.getPersistData(extension))
    {
      _logger.info(TAG "persisting game data");
      _gameData = data;
    }
    else
    {
      free(data);
    }
  }

  if (cancelLoad)
  {
    cancelLoad = false; // reset for future load attempts

    _fsm.unloadGame();
    return false;
  }

  // reset the vertical sync flag
  _core.resetVsync();

  _numAudioFaults = 0;
  _numAudioRecoveries = 0;
  _vsyncDisabledByAudioFaults = false;
  _audioGeneratedDuringFastForward = 0;

  if (_core.getNumDiscs() == 0)
  {
    _discPaths.clear();
    updateDiscMenu(true);
  }
  else
  {
    cdrom_get_cd_names(path.c_str(), &_discPaths, &_logger);
    updateDiscMenu(true);
  }

  _gamePath = path;
  _gamePathIsTemporary = false;

  for (size_t i = 0; i < _recentList.size(); i++)
  {
    const RecentItem item = _recentList[i];

    if (item.path == path && item.coreName == _coreName && item.system == _system)
    {
      if (i > 0)
      {
        _recentList.erase(_recentList.begin() + i);
        _recentList.insert(_recentList.begin(), item);
        _logger.debug(TAG "Moved recent file %zu to front \"%s\" - %s - %u", i, util::fileName(item.path).c_str(), item.coreName.c_str(), (unsigned)item.system);
      }
      goto moved_recent_item;
    }
  }

  if (_recentList.size() == 10)
  {
    _recentList.pop_back();
    _logger.debug(TAG "Removed last entry in the recent list");
  }

  {
    auto item = _recentList.emplace(_recentList.begin());
    item->path = path;
    item->coreName = _coreName;
    item->system = _system;

    _logger.debug(TAG "Added recent file \"%s\" - %s - %u", util::fileName(item->path).c_str(), item->coreName.c_str(), (unsigned)item->system);
  }

moved_recent_item:
  refreshMemoryMap();

  _validSlots = 0;

  _states.migrateFiles();
  _states.loadSRAM(&_core);

  for (unsigned ndx = 1; ndx <= 10; ndx++)
  {
    if (_states.existsState(ndx))
    {
      _validSlots |= 1 << ndx;
    }
  }

  _input.autoAssign();
  return true;
}

void Application::refreshMemoryMap()
{
  _memory.attachToCore(&_core, _system);
}

void Application::unloadCore()
{
  std::string json;
  json.append("{\"core\":");
  json.append(_config.serialize());
  json.append(",\"input\":");
  json.append(_input.serialize());
  json.append("}");

  util::saveFile(&_logger, getCoreConfigPath(_coreName), json.c_str(), json.length());

  _memory.destroy();
  _core.destroy();
  _video.reset();
  updateSpeedIndicator();
}

void Application::resetGame()
{
  if (isGameActive())
  {
    _logger.info(TAG "resetting game");

    _core.resetGame();
    _video.clear();
    _video.showMessage("Reset", 60);
    refreshMemoryMap();
    RA_OnReset();
  }
}

bool Application::hardcore()
{
  return RA_HardcoreModeIsActive();
}

bool Application::unloadGame()
{
  cancelLoad = false; // reset for future loads

  if (!RA_ConfirmLoadNewRom(true))
    return false;

  _states.saveSRAM(&_core);

  romUnloaded(&_logger);

  _video.clear();

  if (_gamePathIsTemporary)
  {
    _logger.debug(TAG "Deleting temporary content %s", _gamePath.c_str());
    util::deleteFile(_gamePath);
    _gamePathIsTemporary = false;
  }

  _gamePath.clear();
  _gameFileName.clear();
  if (_gameData)
  {
    free(_gameData);
    _gameData = NULL;
  }

  _states.setGame(_gameFileName, 0, _coreName, &_core);

  _validSlots = 0;
  enableSlots();

  return true;
}

void Application::pauseGame(bool pause)
{
  if (!pause)
  {
    _fsm.resumeGame();
    RA_SetPaused(false);
  }
  else if (hardcore())
  {
    _fsm.pauseGame();
    RA_SetPaused(true);
  }
  else
  {
    _fsm.pauseGameNoOvl();
  }

  updateSpeedIndicator();
}

void Application::printf(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  _logger.vprintf(RETRO_LOG_DEBUG, fmt, args);
  va_end(args);
  fflush(stdout);
}

bool Application::isGameActive()
{
  switch (_fsm.currentState())
  {
  case Fsm::State::GameRunning:
  case Fsm::State::GamePaused:
  case Fsm::State::GamePausedNoOvl:
  case Fsm::State::FrameStep:
    return true;

  default:
    return false;
  }
}

bool Application::isPaused() const
{
  switch (_fsm.currentState())
  {
  case Fsm::State::GamePaused:
  case Fsm::State::GamePausedNoOvl:
    return true;

  default:
    return false;
  }
}

void Application::onRotationChanged(Video::Rotation oldRotation, Video::Rotation newRotation)
{
  const Uint32 fullscreen = SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;

  if (!fullscreen)
  {
    /* rotation of 90 or 270 degrees requires resizing window */
    if (((int)oldRotation ^ (int)newRotation) & 1)
    {
      int width = _video.getWindowWidth();
      int height = _video.getWindowHeight();
      resizeWindow(height, width);

      /* immediately call windowResized as rotation may change again before the window message is processed */
      _video.windowResized(height, width);
    }
  }
}

void Application::s_audioCallback(void* udata, Uint8* stream, int len)
{
  Application* app = (Application*)udata;

  if (app->_fsm.currentState() == Fsm::State::GameRunning && !app->_processingEvents)
  {
    size_t avail = app->_fifo.occupied();

    if (avail < (size_t)len)
    {
      app->_fifo.read((void*)stream, avail);
      memset((void*)(stream + avail), 0, len - avail);
      app->_logger.debug("[AUD] Audio hardware requested %d bytes, only %zu available, padding with zeroes", len, avail);

      app->_numAudioRecoveries = 0;
      if (++app->_numAudioFaults > 200)
      {
        if (app->_config.getFastForwarding() && app->_audioGeneratedDuringFastForward < 10)
        {
          // some cores don't generate audio when fast forwarding
        }
        else if (SDL_GL_GetSwapInterval() == 1)
        {
          // try turning off VSYNC to see if we can achieve the target framerate
          SDL_GL_SetSwapInterval(0);
          app->_vsyncDisabledByAudioFaults = true;
        }
        else
        {
          app->pauseForBadPerformance();
        }

        app->_numAudioFaults /= 2;
      }
    }
    else
    {
      if (app->_numAudioFaults > 3)
      {
        app->_numAudioFaults -= 3;
        if (app->_config.getFastForwarding())
          ++app->_audioGeneratedDuringFastForward;
      }
      else if (app->_vsyncDisabledByAudioFaults)
      {
        if (++app->_numAudioRecoveries == 5)
        {
          SDL_GL_SetSwapInterval(1);
          app->_vsyncDisabledByAudioFaults = false;
        }
      }

      app->_fifo.read((void*)stream, len);
#ifdef DEBUG_AUDIO
      app->_logger.debug("[AUD] Audio hardware requested %d bytes", len);
#endif
    }
  }
  else
  {
#ifdef DEBUG_AUDIO
    app->_logger.debug("[AUD] Audio hardware requested %d bytes, game is not running, sending silence", len);
#endif
    memset((void*)stream, 0, len);
  }
}

void Application::loadGame()
{
  const struct retro_system_info* info = _core.getSystemInfo();
  std::string file_types;
  std::string supported_exts;
  const char* ptr = getEmulatorExtensions(_coreName, _system);
  if (ptr == NULL)
    ptr = info->valid_extensions;

  while (*ptr)
  {
    supported_exts += "*.";
    while (*ptr && *ptr != '|')
      supported_exts += *ptr++;

    if (!*ptr)
      break;

    supported_exts += ';';
    ++ptr;
  }

  if (!info->need_fullpath)
  {
    supported_exts += ";*.zip";
  }

  file_types.reserve(supported_exts.size() * 2 + 32);
  file_types.append("Supported Files (");
  file_types.append(supported_exts);
  file_types.append(")");
  file_types.append("\0", 1);
  file_types.append(supported_exts);
  file_types.append("\0", 1);
  file_types.append("All Files (*.*)");
  file_types.append("\0", 1);
  file_types.append("*.*");
  file_types.append("\0", 2);

  std::string path = util::openFileDialog(g_mainWindow, file_types);

  if (!path.empty())
  {
    /* some cores need to be reset to flush any previous state information */
    /* this also ensures the disc menu is reset if the core built it dynamically */
    /* ASSERT: loadCore will unload the current core, even if it's the same core */
    if (_core.gameLoaded())
      _fsm.loadCore(_coreName);

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
    caption += getEmulatorName(_recentList[i].coreName, _recentList[i].system);
    caption += " - ";
    caption += getSystemName(_recentList[i].system);
    caption += ")";

    // escape ampersands so they don't get interpreted as accelerators
    size_t nPos = 0;
    while ((nPos = caption.find('&', nPos)) != std::string::npos)
    {
      caption.insert(caption.begin() + nPos, '&');
      nPos += 2;
    }

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

void Application::toggleTray()
{
  if (_core.getNumDiscs() > 0)
  {
    _core.setTrayOpen(!_core.getTrayOpen());
    updateDiscMenu(false);

    if (_core.getTrayOpen())
    {
      _video.showMessage(_isDriveFloppy ? "Floppy ejected" : "Disc ejected", 200);
    }
    else
    {
      std::string message = "Inserted " + getDiscLabel(_core.getCurrentDiscIndex());
      _video.showMessage(message.c_str(), 200);
    }
  }
}

void Application::readyNextDisc(int offset)
{
  const unsigned numDiscs = _core.getNumDiscs();
  if (numDiscs > 0)
  {
    if (_core.getTrayOpen())
      readyDisc((_core.getCurrentDiscIndex() + numDiscs + offset) % numDiscs);
    else
      _video.showMessage(_isDriveFloppy ? "Cannot change floppy until previous floppy ejected" : "Cannot change disc until previous disc ejected", 200);
  }
}

void Application::readyDisc(unsigned newDiscIndex)
{
  if (_core.getCurrentDiscIndex() != newDiscIndex)
  {
    std::string path;
    if (_core.getDiscPath(newDiscIndex, path))
    {
      if (!romLoaded(&_core, &_logger, _system, path, NULL, 0, true))
        return;
    }
    else if (newDiscIndex < _discPaths.size())
    {
      path = util::replaceFileName(_gamePath, _discPaths.at(newDiscIndex).c_str());
      if (!romLoaded(&_core, &_logger, _system, path, NULL, 0, true))
        return;
    }

    _core.setCurrentDiscIndex(newDiscIndex);
    updateDiscMenu(false);

    std::string message = "Readied " + getDiscLabel(newDiscIndex);
    _video.showMessage(message.c_str(), 200);
  }
}

void Application::updateDiscMenu(bool updateLabels)
{
  size_t i = 0;
  size_t coreDiscCount = _core.getNumDiscs();

  size_t menuItemCount = GetMenuItemCount(_cdRomMenu);
  while (menuItemCount > coreDiscCount + 1)
    DeleteMenu(_cdRomMenu, --menuItemCount, MF_BYPOSITION);

  while (menuItemCount < coreDiscCount + 1)
  {
    AppendMenu(_cdRomMenu, MF_STRING, IDM_CD_DISC_FIRST + menuItemCount - 1, "Empty");
    ++menuItemCount;
  }

  if (coreDiscCount == 0)
  {
    EnableMenuItem(_cdRomMenu, IDM_CD_OPEN_TRAY, MF_DISABLED);
  }
  else
  {
    unsigned selectedDisc = _core.getCurrentDiscIndex();
    bool trayOpen = _core.getTrayOpen();
    std::string discLabel;
    char buffer[128];

    MENUITEMINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    GetMenuItemInfo(_menu, IDM_CD_OPEN_TRAY, false, &info);
    info.fMask = MIIM_TYPE | MIIM_DATA;
    info.dwTypeData = (LPSTR)(_isDriveFloppy ?
      (trayOpen ? "Insert Disk" : "Remove Disk") :
      (trayOpen ? "Close Tray" : "Open Tray"));
    SetMenuItemInfo(_menu, IDM_CD_OPEN_TRAY, false, &info);

    for (; i < coreDiscCount; i++)
    {
      UINT id = IDM_CD_DISC_FIRST + i;

      MENUITEMINFO info;
      memset(&info, 0, sizeof(info));
      info.cbSize = sizeof(info);
      info.fMask = MIIM_TYPE | MIIM_DATA | MIIM_STATE;
      info.dwTypeData = buffer;
      info.cch = sizeof(buffer);
      GetMenuItemInfo(_menu, id, false, &info);

      if (updateLabels)
      {
        discLabel = getDiscLabel(i);
        info.dwTypeData = (char*)discLabel.data();
      }

      info.fState = (i == selectedDisc) ? MFS_CHECKED : MFS_UNCHECKED;
      if (!trayOpen)
        info.fState |= MF_DISABLED;

      SetMenuItemInfo(_menu, id, false, &info);
    }

    EnableMenuItem(_menu, IDM_CD_OPEN_TRAY, MF_ENABLED);
  }
}

std::string Application::getDiscLabel(unsigned index) const
{
  std::string discLabel;

  if (_core.getDiscLabel(index, discLabel))
    return discLabel;

  if (index < _discPaths.size())
  {
    const std::string& path = _discPaths.at(index);
    size_t lastSlashIndex = path.find_last_of('\\');
    if (lastSlashIndex == std::string::npos)
      lastSlashIndex = path.find_last_of('/');

    if (lastSlashIndex != std::string::npos)
      discLabel = path.substr(lastSlashIndex + 1);
    else
      discLabel = path;
  }
  else
  {
    discLabel = "Disc " + index;
  }

  return discLabel;
}

std::string Application::getStatePath(unsigned ndx)
{
  return _states.getStatePath(ndx);
}

std::string Application::getConfigPath()
{
  std::string path = _config.getRootFolder();
  path += "RALibretro.json";
  return path;
}

std::string Application::getCoreConfigPath(const std::string& coreName)
{
  std::string path = _config.getRootFolder();
  path += "Cores\\";
  path += coreName;
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
  if (!_states.saveState(path))
    MessageBox(g_mainWindow, "Failed to create save state.", "Failed to create save state", MB_OK);
}

void Application::saveState(unsigned ndx)
{
  if (ndx == 0)
  {
    saveState();
    return;
  }

  if (!_states.saveState(ndx))
  {
    std::string message = "Failed to create save state.";
    std::string filename = _states.getStatePath(ndx);
    if (filename.length() > MAX_PATH)
      message += "\n\nGenerated path is too long:\n" + filename;

    MessageBox(g_mainWindow, message.c_str(), "Failed to create save state", MB_OK);
    return;
  }

  char message[128];
  snprintf(message, sizeof(message), "Saved state %u", ndx);
  _video.showMessage(message, 60);

  if (ndx <= 10)
  {
    _validSlots |= 1 << ndx;
    enableSlots();
  }
}

void Application::saveState()
{
  std::string extensions = "State Files (*.state)";
  extensions.append("\0", 1);
  extensions.append("*.state");
  extensions.append("\0", 2);
  std::string path = util::saveFileDialog(g_mainWindow, extensions, "state");

  if (!path.empty())
  {
    saveState(path);
  }
}

void Application::loadState(const std::string& path)
{
  if (_states.loadState(path))
  {
    updateDiscMenu(false);
  }
}

void Application::loadState(unsigned ndx)
{
  if (ndx == 0)
  {
    loadState();
    return;
  }

  if (ndx > 10)
  {
    // we don't pre-validate the existance of save states that aren't displayed in the menu
    // also, we can't track more than 32 states that way and allow up to 99 numbered states
  }
  else if ((_validSlots & (1 << ndx)) == 0)
  {
    return;
  }

  if (_states.loadState(ndx))
  {
    char message[128];
    snprintf(message, sizeof(message), "Loaded state %u", ndx);
    _video.showMessage(message, 60);

    updateDiscMenu(false);
  }
}

void Application::loadState()
{
  std::string extensions = "State Files (*.state*)";
  extensions.append("\0", 1);
  extensions.append("*.state*");
  extensions.append("\0", 1);
  extensions.append("All Files (*.*)");
  extensions.append("\0", 1);
  extensions.append("*.*");
  extensions.append("\0", 2);
  std::string path = util::openFileDialog(g_mainWindow, extensions);

  if (!path.empty())
  {
    loadState(path);
  }
}

void Application::changeCurrentState(unsigned ndx)
{
  char message[128];
  snprintf(message, sizeof(message), "Current state set to %u", ndx);
  _video.showMessage(message, 60);
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

  _video.showMessage("Screenshot captured", 60);
}

void Application::aboutDialog()
{
  ::aboutDialog(_logger.contents().c_str());
}

static void buildSystemMenu(HMENU parentMenu, int system, std::string systemName)
{
  std::set<std::string> systemCores;
  getAvailableSystemCores(system, systemCores);

  std::map<std::string, int> systemItems;
  for (const auto& systemCore : systemCores)
  {
    int id = encodeCoreName(systemCore, system);
    systemItems.emplace(getEmulatorName(systemCore, system), id);
  }

  HMENU systemMenu = CreateMenu();
  for (const auto& systemItem : systemItems)
    AppendMenu(systemMenu, MF_STRING, IDM_SYSTEM_FIRST + systemItem.second, systemItem.first.c_str());

  AppendMenu(parentMenu, MF_POPUP | MF_STRING, (UINT_PTR)systemMenu, systemName.c_str());
}

void Application::buildSystemsMenu()
{
  std::set<int> availableSystems;
  getAvailableSystems(availableSystems);
  if (availableSystems.empty())
    return;

  // use map to sort labels
  std::set<std::string> systemCores;
  std::map<std::string, int> systemMap;

  for (auto system : availableSystems)
  {
    systemCores.clear();
    getAvailableSystemCores(system, systemCores);
    if (!systemCores.empty())
      systemMap.emplace(getSystemName(system), system);
  }

  HMENU fileMenu = GetSubMenu(_menu, 0);
  HMENU systemsMenu = GetSubMenu(fileMenu, 0);

  while (GetMenuItemCount(systemsMenu) > 0)
    DeleteMenu(systemsMenu, 0, MF_BYPOSITION);

  if (systemMap.size() > 20)
  {
    std::map<std::string, std::vector<int>> manufacturerMap;
    for (const auto& pair : systemMap)
    {
      int system = pair.second;
      std::string manufacturer = getSystemManufacturer(system);
      manufacturerMap[manufacturer].push_back(system);
    }

    for (const auto& pair : manufacturerMap)
    {
      HMENU manufacturerMenu = CreateMenu();

      for (const auto& systemPair : systemMap)
      {
        int system = systemPair.second;
        for (const auto manufacturerSystem : pair.second)
        {
          if (manufacturerSystem == system)
          {
            buildSystemMenu(manufacturerMenu, system, systemPair.first);
            break;
          }
        }
      }

      AppendMenu(systemsMenu, MF_POPUP | MF_STRING, (UINT_PTR)manufacturerMenu, pair.first.c_str());
    }
  }
  else
  {
    for (const auto& pair : systemMap)
      buildSystemMenu(systemsMenu, pair.second, pair.first);
  }
}

void Application::loadConfiguration(int* window_x, int* window_y, int* window_width, int* window_height)
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
      int x, y;
      int w, h;
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
          else if (event == JSONSAX_STRING && ud->key == "core")
          {
            ud->item.coreName = std::string(str, num);
          }
          else if (event == JSONSAX_NUMBER && ud->key == "emulator")
          {
            // legacy format
            switch (strtoul(str, NULL, 10))
            {
              case 1:  ud->item.coreName = "stella_libretro"; break;
              case 2:  ud->item.coreName = "snes9x_libretro"; break;
              case 3:  ud->item.coreName = "picodrive_libretro"; break;
              case 4:  ud->item.coreName = "genesis_plus_gx_libretro"; break;
              case 5:  ud->item.coreName = "fceumm_libretro"; break;
              case 6:  ud->item.coreName = "handy_libretro"; break;
              case 7:  ud->item.coreName = "mednafen_supergrafx_libretro"; break;
              case 8:  ud->item.coreName = "gambatte_libretro"; break;
              case 9:  ud->item.coreName = "mgba_libretro"; break;
              case 10: ud->item.coreName = "mednafen_psx_libretro"; break;
              case 11: ud->item.coreName = "mednafen_ngp_libretro"; break;
              case 12: ud->item.coreName = "mednafen_vb_libretro"; break;
              case 13: ud->item.coreName = "fbalpha_libretro"; break;
              case 14: ud->item.coreName = "prosystem_libretro"; break;
            }
          }
          else if (event == JSONSAX_NUMBER && ud->key == "system")
          {
            ud->item.system = strtoul(str, NULL, 10);
          }
          else if (event == JSONSAX_OBJECT && num == 0)
          {
            ud->self->_recentList.push_back(ud->item);
            ud->self->_logger.debug(TAG "Added recent file \"%s\" - %s - %u", util::fileName(ud->item.path).c_str(), ud->item.coreName.c_str(), (unsigned)ud->item.system);
          }

          return 0;
        });

        if (res2 != JSONSAX_OK)
        {
          return -1;
        }
      }
      else if (ud->key == "window" && event == JSONSAX_OBJECT)
      {
        ud->x = ud->y = ud->w = ud->h = 0;

        jsonsax_result_t res2 = jsonsax_parse((char*)str, ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
        {
          auto ud = (Deserialize*)udata;

          if (event == JSONSAX_KEY)
          {
            ud->key = std::string(str, num);
          }
          else if (event == JSONSAX_NUMBER)
          {
            if (ud->key == "x")
              ud->x = (int)strtoul(str, NULL, 10);
            else if (ud->key == "y")
              ud->y = (int)strtoul(str, NULL, 10);
            else if (ud->key == "w")
              ud->w = (int)strtoul(str, NULL, 10);
            else if (ud->key == "h")
              ud->h = (int)strtoul(str, NULL, 10);
          }

          return 0;
        });

        if (res2 != JSONSAX_OK)
        {
          return -1;
        }
      }
      else if (ud->key == "bindings" && event == JSONSAX_OBJECT)
      {
        if (!ud->self->_keybinds.deserializeBindings(str))
        {
          return -1;
        }
      }
      else if (ud->key == "saves" && event == JSONSAX_OBJECT)
      {
        if (!ud->self->_states.deserializeSettings(str))
        {
          return -1;
        }
      }
      else if (ud->key == "video" && event == JSONSAX_OBJECT)
      {
        if (!ud->self->_video.deserializeSettings(str))
        {
          return -1;
        }
      }
      else if (ud->key == "emulator" && event == JSONSAX_OBJECT)
      {
        if (!ud->self->_config.deserializeEmulatorSettings(str))
        {
          return -1;
        }
      }

      return 0;
    });

    if (ud.w > 0 && ud.h > 0)
    {
      _logger.debug(TAG "Remembered window position %d,%d (%dx%d)", ud.x, ud.y, ud.w, ud.h);
      *window_x = ud.x;
      *window_y = ud.y;
      *window_width = ud.w;
      *window_height = ud.h;
    }

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

    json += "\",\"core\":\"";
    json += _recentList[i].coreName;

    json += "\",\"system\":";
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)_recentList[i].system);
    json += buffer;

    json += "}";
  }

  json += "]";
  return json;
}

void Application::resizeWindow(unsigned multiplier)
{
  Uint32 fullscreen = SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
  
  if (!fullscreen)
  {
    unsigned width, height;
    enum retro_pixel_format format;

    switch (_video.getRotation())
    {
      default:
        _video.getFramebufferSize(&width, &height, &format);
        break;

      case Video::Rotation::Ninety:
      case Video::Rotation::TwoSeventy:
        _video.getFramebufferSize(&height, &width, &format);
        break;
    }

    resizeWindow(width * multiplier, height * multiplier);
  }
}

void Application::resizeWindow(int width, int height)
{
  int actual_width, actual_height;
  SDL_SetWindowSize(_window, width, height);

  /* SDL_SetWindowSize does not always account for the menu size correctly - especially if it wraps
   * make sure we got the size we wanted */
  SDL_GetWindowSize(_window, &actual_width, &actual_height);
  if (actual_width != width || actual_height != height)
  {
    width += (width - actual_width);
    height += (height - actual_height);
    SDL_SetWindowSize(_window, width, height);
  }
}

void Application::toggleFullscreen()
{
  Uint32 fullscreen = SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
  if (fullscreen)
  {
    SetMenu(g_mainWindow, _menu);
    SDL_ShowCursor(SDL_ENABLE);
  }

  SDL_SetWindowFullscreen(_window, fullscreen ^ SDL_WINDOW_FULLSCREEN_DESKTOP);

  if (!fullscreen)
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
    {
      const auto& recent = _recentList[cmd - IDM_LOAD_RECENT_1];
      _system = recent.system;
      if (_fsm.loadCore(recent.coreName))
        _fsm.loadGame(recent.path);
      else
        _system = 0;
      break;
    }
    
    case IDM_CD_OPEN_TRAY:
      toggleTray();
      break;

    case IDM_PAUSE_GAME:
      pauseGame(true);
      break;

    case IDM_RESUME_GAME:
      pauseGame(false);
      break;
    
    case IDM_TURBO_GAME:
      toggleFastForwarding(2);
      break;

    case IDM_RESET_GAME:
      _fsm.resetGame();

      if (isPaused())
        _fsm.step();
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
      _config.showDialog(_core.getSystemInfo()->library_name, _input);
      if (isGameActive())
        refreshMemoryMap();
      break;

    case IDM_INPUT_CONFIG:
      _keybinds.showHotKeyDialog(_input);
      break;

    case IDM_INPUT_CONTROLLER_1:
      _keybinds.showControllerDialog(_input, 0);
      break;

    case IDM_INPUT_CONTROLLER_2:
      _keybinds.showControllerDialog(_input, 1);
      break;

    case IDM_INPUT_BACKGROUND_INPUT:
      toggleBackgroundInput();
      break;

    case IDM_VIDEO_CONFIG:
      _video.showDialog();
      break;

    case IDM_EMULATOR_CONFIG:
      _config.showEmulatorSettingsDialog();
      updateMouseCapture();
      updateSpeedIndicator();
      _video.redraw();
      break;

    case IDM_SAVING_CONFIG:
      _states.showDialog();
      break;

    case IDM_WINDOW_1X:
    case IDM_WINDOW_2X:
    case IDM_WINDOW_3X:
    case IDM_WINDOW_4X:
    case IDM_WINDOW_5X:
      resizeWindow(cmd - IDM_WINDOW_1X + 1);
      break;

    case IDM_MANAGE_CORES:
      if (showCoresDialog(&_config, &_logger, _coreName, 0))
        buildSystemsMenu();
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
      else if (cmd >= IDM_CD_DISC_FIRST && cmd <= IDM_CD_DISC_LAST)
      {
        unsigned newDiscIndex = cmd - IDM_CD_DISC_FIRST;
        readyDisc(newDiscIndex);
      }
      else if (cmd >= IDM_SYSTEM_FIRST && cmd <= IDM_SYSTEM_LAST)
      {
        const std::string& coreName = getCoreName(cmd - IDM_SYSTEM_FIRST, _system);
        if (coreName != _coreName) // cannot update active core, so don't check if it's outdated
        {
          std::string path = _config.getRootFolder();
          path += "Cores\\";
          path += coreName;
          path += ".dll";

          const time_t coreUpdated = util::fileTime(path);
          const time_t coreAge = time(NULL) - coreUpdated;
          const time_t oneYear = (time_t)365 * 24 * 60 * 60;
          if (coreAge > oneYear)
          {
            std::string message = "The " + getEmulatorName(coreName, _system) + " core hasn't been updated in over a year.\r\n\r\nWould you like to do so now?";
            if (MessageBox(g_mainWindow, message.c_str(), "RALibRetro", MB_YESNO) == IDYES)
            {
              if (showCoresDialog(&_config, &_logger, _coreName, _system))
                buildSystemsMenu();
            }
          }
        }

        if (!_fsm.loadCore(coreName))
          _system = 0;
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

void Application::handle(const SDL_MouseMotionEvent* motion)
{
  const int viewWidth = _video.getViewWidth();
  if (viewWidth)
  {
    int rel_x, rel_y, abs_x, abs_y;

    // convert the window position (motion->x/y) to view-relative positions (viewX/Y)
    const int viewScaledWidth = _video.getViewScaledWidth();
    const int windowWidth = _video.getWindowWidth();
    const int leftMargin = (windowWidth - viewScaledWidth) / 2;
    int viewX = motion->x - leftMargin;
    if (viewX < 0)
      viewX = 0;
    else if (viewX >= viewScaledWidth)
      viewX = viewScaledWidth - 1;

    const int viewHeight = _video.getViewHeight();
    const int viewScaledHeight = _video.getViewScaledHeight();
    const int windowHeight = _video.getWindowHeight();
    const int topMargin = (windowHeight - viewScaledHeight) / 2;
    int viewY = motion->y - topMargin;
    if (viewY < 0)
      viewY = 0;
    else if (viewY >= viewScaledHeight)
      viewY = viewScaledHeight - 1;

    if (SDL_GetRelativeMouseMode())
    {
      _absViewMouseX += motion->xrel;
      _absViewMouseY += motion->yrel;
    }
    else
    {
      _absViewMouseX = viewX;
      _absViewMouseY = viewY;
    }

    switch (_video.getRotation())
    {
      default:
        rel_x = ((viewX << 16) / viewScaledWidth) - 32767;
        rel_y = ((viewY << 16) / viewScaledHeight) - 32767;
        abs_x = (_absViewMouseX * viewWidth) / viewScaledWidth;
        abs_y = (_absViewMouseY * viewHeight) / viewScaledHeight;
        break;

      case Video::Rotation::Ninety:
        rel_x = 32767 - ((viewY << 16) / viewScaledHeight);
        rel_y = ((viewX << 16) / viewScaledWidth) - 32767;
        abs_x = viewWidth - ((_absViewMouseY * viewWidth) / viewScaledHeight) - 1;
        abs_y = (_absViewMouseX * viewHeight) / viewScaledWidth;
        break;

      case Video::Rotation::OneEighty:
        rel_x = 32767 - ((viewX << 16) / viewScaledWidth);
        rel_y = 32767 - ((viewY << 16) / viewScaledHeight);
        abs_x = viewWidth - ((_absViewMouseX * viewWidth) / viewScaledWidth) - 1;
        abs_y = viewHeight - ((_absViewMouseY * viewHeight) / viewScaledHeight) - 1;
        break;

      case Video::Rotation::TwoSeventy:
        rel_x = ((viewY << 16) / viewScaledHeight) - 32767;
        rel_y = 32767 - ((viewX << 16) / viewScaledWidth);
        abs_x = (_absViewMouseY * viewWidth) / viewScaledHeight;
        abs_y = viewHeight - ((_absViewMouseX * viewHeight) / viewScaledWidth) - 1;
        break;
    }

    _input.mouseMoveEvent(rel_x, rel_y, abs_x, abs_y);
  }
}

void Application::handle(const SDL_MouseButtonEvent* button)
{
  bool pressed = (button->state == SDL_PRESSED);
  switch (button->button)
  {
    case SDL_BUTTON_LEFT:   _input.mouseButtonEvent(Input::MouseButton::kLeft, pressed); break;
    case SDL_BUTTON_MIDDLE: _input.mouseButtonEvent(Input::MouseButton::kMiddle, pressed); break;
    case SDL_BUTTON_RIGHT:  _input.mouseButtonEvent(Input::MouseButton::kRight, pressed); break;
  }
}

void Application::handle(const KeyBinds::Action action, unsigned extra)
{
  switch (action)
  {
  case KeyBinds::Action::kNothing:          break;

  // Joypad buttons
  case KeyBinds::Action::kButtonUp:         _input.buttonEvent(extra >> 8, Input::Button::kUp, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonDown:       _input.buttonEvent(extra >> 8, Input::Button::kDown, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonLeft:       _input.buttonEvent(extra >> 8, Input::Button::kLeft, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonRight:      _input.buttonEvent(extra >> 8, Input::Button::kRight, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonX:          _input.buttonEvent(extra >> 8, Input::Button::kX, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonY:          _input.buttonEvent(extra >> 8, Input::Button::kY, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonA:          _input.buttonEvent(extra >> 8, Input::Button::kA, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonB:          _input.buttonEvent(extra >> 8, Input::Button::kB, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonL:          _input.buttonEvent(extra >> 8, Input::Button::kL, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonR:          _input.buttonEvent(extra >> 8, Input::Button::kR, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kAxisL2:           _input.axisEvent(extra >> 16, Input::Axis::kL2, (int16_t)(extra & 0xFFFF)); break;
  case KeyBinds::Action::kAxisR2:           _input.axisEvent(extra >> 16, Input::Axis::kR2, (int16_t)(extra & 0xFFFF)); break;
  case KeyBinds::Action::kButtonL3:         _input.buttonEvent(extra >> 8, Input::Button::kL3, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonR3:         _input.buttonEvent(extra >> 8, Input::Button::kR3, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonSelect:     _input.buttonEvent(extra >> 8, Input::Button::kSelect, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonStart:      _input.buttonEvent(extra >> 8, Input::Button::kStart, (extra & 0xFF) != 0); break;

  // Joypad analog sticks
  case KeyBinds::Action::kAxisLeftX:        _input.axisEvent(extra >> 16, Input::Axis::kLeftAxisX, (int16_t)(extra & 0xFFFF)); break;
  case KeyBinds::Action::kAxisLeftY:        _input.axisEvent(extra >> 16, Input::Axis::kLeftAxisY, (int16_t)(extra & 0xFFFF)); break;
  case KeyBinds::Action::kAxisRightX:       _input.axisEvent(extra >> 16, Input::Axis::kRightAxisX, (int16_t)(extra & 0xFFFF)); break;
  case KeyBinds::Action::kAxisRightY:       _input.axisEvent(extra >> 16, Input::Axis::kRightAxisY, (int16_t)(extra & 0xFFFF)); break;

  // State management
  case KeyBinds::Action::kSaveState:        saveState(extra); break;
  case KeyBinds::Action::kLoadState:        loadState(extra); break;
  case KeyBinds::Action::kChangeCurrentState: changeCurrentState(extra); break;

  // Disc management
  case KeyBinds::Action::kToggleTray:       toggleTray(); break;
  case KeyBinds::Action::kReadyNextDisc:    readyNextDisc(1); break;
  case KeyBinds::Action::kReadyPreviousDisc:readyNextDisc(-1); break;

  // Window size
  case KeyBinds::Action::kSetWindowSize1:   resizeWindow(1); break;
  case KeyBinds::Action::kSetWindowSize2:   resizeWindow(2); break;
  case KeyBinds::Action::kSetWindowSize3:   resizeWindow(3); break;
  case KeyBinds::Action::kSetWindowSize4:   resizeWindow(4); break;
  case KeyBinds::Action::kSetWindowSize5:   resizeWindow(5); break;
  case KeyBinds::Action::kToggleFullscreen: toggleFullscreen(); break;
  case KeyBinds::Action::kRotateRight:      _video.setRotation((Video::Rotation)(((int)_video.getRotation() + 3) & 3)); break;
  case KeyBinds::Action::kRotateLeft:       _video.setRotation((Video::Rotation)(((int)_video.getRotation() + 1) & 3)); break;

  // Emulation speed
  case KeyBinds::Action::kStep:
    _fsm.step();
    updateSpeedIndicator();
    break;

  case KeyBinds::Action::kPauseToggle: /* overlay toggle */
    if (_fsm.currentState() == Fsm::State::GamePaused || RA_IsOverlayFullyVisible())
    {
      // overlay visible. hide and unpause
      pauseGame(false);
    }
    else
    {
      // paused without overlay, or not paused - show overlay and pause
      _fsm.pauseGame();
      RA_SetPaused(true);
    }
    updateSpeedIndicator();
    break;

  case KeyBinds::Action::kPauseToggleNoOvl: /* non-overlay pause toggle */
    if (_fsm.currentState() == Fsm::State::GamePausedNoOvl)
    {
      // paused without overlay, just unpause
      _fsm.resumeGame();
    }
    else if (_fsm.currentState() == Fsm::State::GamePaused)
    {
      // overlay visible. hide and unpause
      pauseGame(false);
    }
    else
    {
      // not paused, pause without overlay (will fail silently in hardcore)
      _fsm.pauseGameNoOvl();
    }
    updateSpeedIndicator();

    break;

  case KeyBinds::Action::kFastForward:
    toggleFastForwarding(extra);
    break;

  case KeyBinds::Action::kReset:
    _fsm.resetGame();

    if (isPaused())
      _fsm.step();
    break;

  case KeyBinds::Action::kScreenshot:
    screenshot();
    break;

  case KeyBinds::Action::kKeyboardInput:
    _input.keyboardEvent(static_cast<enum retro_key>(extra >> 8), static_cast<bool>(extra & 0xFF));
    break;

  case KeyBinds::Action::kGameFocusToggle:
    updateMenu();

    _video.showMessage(_keybinds.hasGameFocus() ? "Game focus enabled" : "Game focus disabled", 60);
    updateMouseCapture();
    break;
  }
}

void Application::updateMouseCapture()
{
  SDL_SetRelativeMouseMode(_keybinds.hasGameFocus() && _config.getGameFocusCaptureMouse() ? SDL_TRUE : SDL_FALSE);
}

void Application::toggleFastForwarding(unsigned extra)
{
  // get the current fast forward selection
  MENUITEMINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = MIIM_STATE;
  GetMenuItemInfo(_menu, IDM_TURBO_GAME, false, &info);
  const bool checked = (info.fState == MFS_CHECKED);

  switch (extra)
  {
    case 0: // FF key released - restore to selection
      _config.setFastForwarding(checked);
      break;

    case 1: // FF key pressed - switch to opposite of selection (but don't change selection)
      _config.setFastForwarding(!checked);
      break;

    case 2: // FF toggle pressed - switch to opposite of selection (and change selection)
      _config.setFastForwarding(!checked);
      info.fState = checked ? MFS_UNCHECKED : MFS_CHECKED;
      SetMenuItemInfo(_menu, IDM_TURBO_GAME, false, &info);
      break;
  }

  updateSpeedIndicator();
}

void Application::updateSpeedIndicator()
{
  if (!_config.getShowSpeedIndicator())
  {
    _video.showSpeedIndicator(Video::Speed::None);
    return;
  }

  switch (_fsm.currentState())
  {
  case Fsm::State::GamePaused:
  case Fsm::State::GamePausedNoOvl:
  case Fsm::State::FrameStep:
    _video.showSpeedIndicator(Video::Speed::Paused);
    _video.redraw();
    break;

  default:
    if (_config.getFastForwarding())
      _video.showSpeedIndicator(Video::Speed::FastForwarding);
    else
      _video.showSpeedIndicator(Video::Speed::None);
  }
}

void Application::toggleBackgroundInput()
{
  const bool enabled = !_config.getBackgroundInput();
  _config.setBackgroundInput(enabled);

  setBackgroundInput(enabled);
}

void Application::setBackgroundInput(bool enabled)
{
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, enabled ? "1" : "0");

  MENUITEMINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = MIIM_STATE;
  info.fState = enabled ? MFS_CHECKED : MFS_UNCHECKED;
  SetMenuItemInfo(_menu, IDM_INPUT_BACKGROUND_INPUT, false, &info);
}

bool Application::handleArgs(int argc, char* argv[])
{
  std::string core; // -c or --core
  int system = 0;   // -s or --system
  std::string game; // -g or --game

  char argCategory = '\0';  // remember the argument category, e.g. "c" for core, "s" for system, "g" for game
  for (int i = 0; i < argc; i++) {
    char *currentArg = argv[i];

    if (strlen(currentArg) == 0) {
      continue;
    }

    if (currentArg[0] == '-') {
      if (strcmp(currentArg, "-c") == 0 || strcmp(currentArg, "--core") == 0) {
        argCategory = 'c';
      } else if (strcmp(currentArg, "-s") == 0 || strcmp(currentArg, "--system") == 0) {
        argCategory = 's';
      }
      else if (strcmp(currentArg, "-g") == 0 || strcmp(currentArg, "--game") == 0) {
        argCategory = 'g';
      } else {
        argCategory = '\0';
      }

      continue;
    }

    switch (argCategory)
    {
      case 'c':
        core.assign(currentArg);
        break;
      case 's':
        try {
          system = std::stoi(currentArg);
        } catch (const std::exception& e) {
          _logger.error(TAG "error while parsing 'system' command line argument '%s' (integer expected)", currentArg);
          system = 0;
        }
        break;
      case 'g':
        game.assign(currentArg);
        break;
      
      default:
        break;
    }
  }

  if (core.empty() || system == 0 || game.empty()) {
    RA_AttemptLogin(false); // start login process, should be done by the time the user loads a game
    return true;
  }

  _logger.info("[APP] cli argument core: %s", core.c_str());
  _logger.info("[APP] cli argument system: %i", system);
  _logger.info("[APP] cli argument game: %s", game.c_str());

  if (!util::exists(game)) {
    std::string message = "File not found provided in 'game' command line argument '" + game + "'";
    
    _logger.error(message.c_str());

    MessageBox(g_mainWindow, message.c_str(), "Failed to load game", MB_OK);
    return false;
  }

  if (!doesCoreSupportSystem(core, system)) {
    std::string message = core + " core does not support system " + std::to_string(system);
    
    _logger.error(message.c_str());

    MessageBox(g_mainWindow, message.c_str(), "Failed to load game", MB_OK);
    return false;
  }

  RA_AttemptLogin(true); // attempt login with blocking, so that we go on exactly after the login was successful

  _system = system;
  if (!_fsm.loadCore(core))
    return false;

  return _fsm.loadGame(game);
}
