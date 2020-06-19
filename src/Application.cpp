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
#include <sys/stat.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#include <commdlg.h>
#include <shlobj.h>

#define TAG "[APP] "

//#define DISPLAY_FRAMERATE 1
//#define DEBUG_FRAMERATE 1

HWND g_mainWindow;
Application app;

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
    kWindowInited,
    kGlInited,
    kAudioDeviceInited,
    kFifoInited,
    kAudioInited,
    kInputInited,
    kKeyBindsInited,
    kVideoContextInited,
    kVideoInited
  }
  inited = kNothingInited;

  if (!_logger.init())
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

  if (!_videoContext.init(&_logger, _window))
  {
    goto error;
  }

  inited = kVideoContextInited;

  if (!_video.init(&_logger, &_videoContext, &_config))
  {
    goto error;
  }
  _video.setRotationChangedHandler(s_onRotationChanged);

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
    _cdRomMenu = GetSubMenu(GetSubMenu(_menu, 0), 10);
    assert(GetMenuItemID(_cdRomMenu, 0) == IDM_CD_OPEN_TRAY);

    SetMenu(g_mainWindow, _menu);

    if (!loadCores(&_config, &_logger))
    {
      MessageBox(g_mainWindow, "Could not open Cores\\cores.json.", "Initialization failed", MB_OK);
      goto error;
    }

    buildSystemsMenu();
    loadConfiguration();

    extern void RA_Init(HWND hwnd);
    RA_Init(g_mainWindow);
  }

  if (!_memory.init(&_logger))
  {
    goto error;
  }

  if (!_states.init(&_logger, &_config, &_video))
  {
    goto error;
  }

  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  _coreName.clear();
  _validSlots = 0;
  lastHardcore = hardcore();
  updateMenu();
  updateCDMenu(NULL, 0, true);
  return true;

error:
  switch (inited)
  {
  case kVideoInited:        _video.destroy();
  case kVideoContextInited: _videoContext.destroy();
  case kKeyBindsInited:     _keybinds.destroy();
  case kInputInited:        _input.destroy();
  case kAudioInited:        _audio.destroy();
  case kFifoInited:         _fifo.destroy();
  case kAudioDeviceInited:  SDL_CloseAudioDevice(_audioDev);
  case kGlInited:           // nothing to undo
  case kWindowInited:       SDL_DestroyWindow(_window);
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
        _input.processEvent(&event, &_keybinds);
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

void Application::runTurbo()
{
  const auto tTurboStart = std::chrono::steady_clock::now();

  // do four frames without video or audio
  for (int i = 0; i < 4; i++)
  {
    _core.step(false, false);
    RA_DoAchievementsFrame();
  }

  // do a final frame with video and no audio
  _core.step(true, false);
  RA_DoAchievementsFrame();

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
  const unsigned int TARGET_FRAMES = (int)round(_core.getSystemAVInfo()->timing.fps * 100);
  const unsigned int SMOOTHING_FRAMES = 64;
  uint32_t frameMicroseconds[SMOOTHING_FRAMES];
#ifdef DEBUG_FRAMERATE
  char renders[SMOOTHING_FRAMES + 1];
#endif
  uint32_t totalMicroseconds;
  int frameIndex = 0;
  int nFaults = 0;
  int nRecoveries = 0;
  bool canVsync = true;
  bool vsyncOn = true;

  const unsigned int SMOOTHING_RATES = 1;
  int skipRates[SMOOTHING_RATES];
  int totalSkipRate = 0;
  int skipIndex = 0;
  int skipRate = 0;
  int nextSkip = 0;

  constexpr int SKIP_DROPPED = 1;
  constexpr int SKIP_PLANNED = 2;
  int skipFrame = 0;

  int skippedFrames = 0;
  int totalDroppedFrames = 0;
  int totalSkippedFrames = 0;

#ifdef DEBUG_FRAMERATE
  memset(renders, 0, sizeof(renders));
#endif

  for (unsigned int i = 0; i < SMOOTHING_RATES; ++i)
    skipRates[i] = SMOOTHING_FRAMES * 100;
  totalSkipRate = SMOOTHING_FRAMES * 100 * SMOOTHING_RATES;

  // if the system wants to render more frames than the display is capable of, turn off vsync
  {
    SDL_DisplayMode displayMode;

    int refreshRate = 60; // assume 60Hz if we can't get an actual value
    if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0 && displayMode.refresh_rate > 0)
      refreshRate = displayMode.refresh_rate;

    if (TARGET_FRAMES / 100 > (unsigned)refreshRate + 1) // allow 60.1Hz NES to vsync on 59.97 monitor
    {
      canVsync = false;
      vsyncOn = false;
      SDL_GL_SetSwapInterval(0);
    }

    _logger.info(TAG "attempting to achieve %u.%02u fps (vsync %s)", TARGET_FRAMES / 100, TARGET_FRAMES % 100,
      (SDL_GL_GetSwapInterval() == 1) ? "on" : "off");
  }

  const auto tFirstFrameStart = std::chrono::steady_clock::now();

  // do a few frames with audio (and no event processing) to establish a baseline
  constexpr int STARTUP_FRAMES = 2;
  for (int i = 0; i < STARTUP_FRAMES; ++i)
  {
    _core.step(true, true);
    RA_DoAchievementsFrame();
  }

  const auto tFirstFrameEnd = std::chrono::steady_clock::now();
  const auto tFirstFrameElapsed = std::chrono::duration_cast<std::chrono::microseconds>(tFirstFrameEnd - tFirstFrameStart);

  const auto firstFrameElapsedMicroseconds = (uint32_t)tFirstFrameElapsed.count() / STARTUP_FRAMES;
  for (unsigned int i = 0; i < SMOOTHING_FRAMES; ++i)
    frameMicroseconds[i] = firstFrameElapsedMicroseconds;
  totalMicroseconds = firstFrameElapsedMicroseconds * SMOOTHING_FRAMES;

#ifdef DEBUG_FRAMERATE
  {
    const unsigned int fps = (SMOOTHING_FRAMES * 1000000) / (totalMicroseconds / 100);
    _logger.info(TAG "FPS: initial frames, fps:%u.%02u", fps / 100, fps % 100);
  }
#endif

  // our rolling window has been populated with data from the startup frames, start the processing loop
  do
  {
    auto tFrameStart = std::chrono::steady_clock::now();

    processEvents();
    if (_fsm.currentState() != Fsm::State::GameRunning)
    {
      // don't kick out of smoothing routine for turbo
      if (_fsm.currentState() == Fsm::State::GameTurbo)
      {
        runTurbo();
        continue;
      }

      // state is not running or turbo - return to outer handler
      break;
    }

    // state is running - do one frame with audio
    if (!skipFrame)
    {
      // render it
      _core.step(true, true);

      skippedFrames = 0;

#ifdef DEBUG_FRAMERATE
      renders[frameIndex] = '-';
#endif
    }
    else
    {
#ifdef DEBUG_FRAMERATE
      renders[frameIndex] = (skipFrame == SKIP_PLANNED) ? '.' : '_';
#endif

      if (skipFrame == SKIP_PLANNED)
        ++totalSkippedFrames;
      else
        ++totalDroppedFrames;

      // four skipped frames in a row is a fault - render anyway
      if (++skippedFrames == 4)
      {
        skippedFrames = 0;

        // render it
        _core.step(true, true);

#ifdef DEBUG_FRAMERATE
        renders[frameIndex] = 'F';
#endif
      }
      else
      {
        // dont render it
        _core.step(false, true);
      }
    }

    RA_DoAchievementsFrame();

    const auto tFrameEnd = std::chrono::steady_clock::now();
    const auto tFrameElapsed = std::chrono::duration_cast<std::chrono::microseconds>(tFrameEnd - tFrameStart);

    if (tFrameElapsed > std::chrono::milliseconds(500))
    {
      // processEvents will block while the user is interacting with the UI. treat this frame
      // as a fault and ignore it. If the framerate returns to normal, it'll be canceled out.
#ifdef DEBUG_FRAMERATE
      renders[frameIndex] = 'X';
#endif
      skipFrame = 0;
      continue;
    }

    totalMicroseconds -= frameMicroseconds[frameIndex];
    const auto frameElapsedMicroseconds = (uint32_t)tFrameElapsed.count();
    frameMicroseconds[frameIndex] = frameElapsedMicroseconds;
    totalMicroseconds += frameElapsedMicroseconds;

    frameIndex++;
    frameIndex %= SMOOTHING_FRAMES;

    // determine our current frame rate
    //   captured frames / total microseconds = n frames / second (1000000 micro seconds)
    //   so: n fames = captured frames * 1000000 / total microseconds
    // however, our fps is acurrate to two places after the decimal, so we need to multiply the
    // result by 100. this overflows the size of an int, so instead of multiplying by an
    // additional 100, we divide totalMicroseconds by 100.
    const unsigned int fps = (SMOOTHING_FRAMES * 1000000) / (totalMicroseconds / 100);
    static_assert(SMOOTHING_FRAMES * 1000000 <= 0xFFFFFFFFU, "SMOOTHING_FRAMES multiplication exceeds sizeof(uint)");
    skipFrame = (fps < TARGET_FRAMES - 200) ? SKIP_DROPPED : 0; // TARGET_FRAMES is in hundredths of fps

    if (frameIndex == 0)
    {
#if DISPLAY_FRAMERATE
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
#ifdef DEBUG_FRAMERATE
      _logger.info(TAG "FPS: rendered frames %s (vsync %s %d)", renders, (SDL_GL_GetSwapInterval() == 1) ? "on" : "off", nRecoveries);
      _logger.info(TAG "FPS: %u.%02u (%d skipped, %d dropped, %d faults)", fps / 100, fps % 100, totalSkippedFrames, totalDroppedFrames, nFaults);
#endif

      if (fps < TARGET_FRAMES * 3 / 4)
      {
        // if we drop below 75% target fps for 5 frames in a row, inform the user they need a better machine
        if (++nFaults == 5)
        {
          if (SDL_GL_GetSwapInterval() == 1)
          {
            // try turning off VSYNC to see if we can achieve the target framerate
#ifdef DEBUG_FRAMERATE
            _logger.info(TAG "FPS: disabling vsync");
#endif
            SDL_GL_SetSwapInterval(0);
            vsyncOn = false;
          }
          else if (hardcore())
          {
            _fsm.pauseGame();

            MessageBox(g_mainWindow, "Game has been paused.\n\nYour system doesn't appear to be able to run this core at the desired speed. Consider changing some of the settings for the core.", "Performance Problem Detected", MB_OK);
          }

          nFaults = 0;
        }
      }
      else if (nFaults > 0)
      {
        --nFaults;
        if (nFaults > 0 && fps > TARGET_FRAMES * 9 / 10)
          --nFaults;
      }

      // always try to back off skipped frames to see if they were added due to a particularly demanding chunk of code
      if (totalSkippedFrames > 2)
      {
        totalSkippedFrames -= 2;
        totalDroppedFrames += totalSkippedFrames / 2;
      }
      else
      {
        totalSkippedFrames = 0;
      }

      // determine how frequently we should skip a frame
      // in the next interval - 233 would indicate we should skip (on average) one frame of every 2 1/3 frames
      if (totalDroppedFrames > 0)
      {
        // if we dropped more than 25% of the frames this interval, try disabling VSYNC
        if ((unsigned)totalDroppedFrames > SMOOTHING_FRAMES / 4)
        {
          if (SDL_GL_GetSwapInterval() == 1)
          {
#ifdef DEBUG_FRAMERATE
            _logger.info(TAG "FPS: disabling vsync");
#endif
            SDL_GL_SetSwapInterval(0);
            vsyncOn = false;
          }
        }

        skipRate = SMOOTHING_FRAMES * 100 / totalDroppedFrames;

        nRecoveries = 0;
      }
      else
      {
        skipRate = SMOOTHING_FRAMES * 100;

        if (!vsyncOn && canVsync)
        {
          if (++nRecoveries == 5)
          {
#ifdef DEBUG_FRAMERATE
            _logger.info(TAG "FPS: re-enabling vsync");
#endif
            SDL_GL_SetSwapInterval(1);
            vsyncOn = true;
          }
        }
      }

      totalSkipRate -= skipRates[skipIndex];
      skipRates[skipIndex] = skipRate;
      totalSkipRate += skipRate;

      skipIndex++;
      skipIndex %= SMOOTHING_RATES;

      skipRate = totalSkipRate / SMOOTHING_RATES;
      if (skipRate == SMOOTHING_FRAMES * 100)
      {
        nextSkip = 0;
      }
      else
      {
        nextSkip = skipRate;
#ifdef DEBUG_FRAMERATE
        _logger.info(TAG "FPS: skip rate set at %d.%02d", skipRate / 100, skipRate % 100);
#endif
      }

      totalSkippedFrames = 0;
      totalDroppedFrames = 0;
    }

    if (nextSkip > 0)
    {
      if (nextSkip <= 100)
      {
        skipFrame = SKIP_PLANNED;
        nextSkip += skipRate;
      }

      nextSkip -= 100;
    }

  } while (true);
}

void Application::run()
{
  _video.clear();

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
        break;

      case Fsm::State::FrameStep:
        // do one frame without audio
        _core.step(true, false);
        RA_DoAchievementsFrame();

        // set state to GamePaused
        _fsm.resumeGame();
        break;

      case Fsm::State::GameTurbo:
        // do five frames without audio
        runTurbo();
        continue;

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

void Application::saveConfiguration()
{
  std::string json = "{";

  // recent items
  json += "\"recent\":";
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

  // complete and save
  json += "}";

  util::saveFile(&_logger, getConfigPath(), json.c_str(), json.length());
}

void Application::destroy()
{
  saveConfiguration();

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
        else if (ud->key == "video")
        {
          ud->self->_video.deserialize(str);
        }
      }

      return 0;
    });

    free(data);
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

  return true;
}

void Application::updateMenu()
{
  static const UINT all_items[] =
  {
    IDM_LOAD_GAME,
    IDM_PAUSE_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT start_items[] =
  {
    IDM_EXIT, IDM_ABOUT
  };

  static const UINT core_loaded_items[] =
  {
    IDM_LOAD_GAME, IDM_EXIT, IDM_CORE_CONFIG, IDM_VIDEO_CONFIG, IDM_ABOUT
  };

  static const UINT game_running_items[] =
  {
    IDM_LOAD_GAME, IDM_PAUSE_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_paused_items[] =
  {
    IDM_LOAD_GAME, IDM_RESUME_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
  };

  static const UINT game_turbo_items[] =
  {
    IDM_LOAD_GAME, IDM_PAUSE_GAME, IDM_RESET_GAME,
    IDM_EXIT,

    IDM_CORE_CONFIG, IDM_VIDEO_CONFIG, IDM_TURBO_GAME, IDM_ABOUT
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
  std::string unzippedFileName;
  const char* ptr;
  size_t size;
  void* data;
  bool loaded;
  bool iszip = (path.length() > 4 && stricmp(&path.at(path.length() - 4), ".zip") == 0);
  bool issupportedzip = false;

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
  }

  if (issupportedzip)
  {
    /* user is loading a zip file, and the core supports it */
    size = 0;
    data = NULL;
  }
  else if (info->need_fullpath)
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

  if (unzippedFileName.empty())
  {
    /* did not extract a file from an archive, just use the original file name */
    unzippedFileName = util::fileNameWithExtension(path);
  }

  /* must update save path before loading the game */
  _states.setGame(unzippedFileName, _system, _coreName, &_core);

  if (info->need_fullpath)
  {
    loaded = _core.loadGame(path.c_str(), NULL, 0);
  }
  else if (iszip)
  {
    std::string zipPsuedoPath = path + '#' + unzippedFileName;
    loaded = _core.loadGame(zipPsuedoPath.c_str(), data, size);
  }
  else
  {
    loaded = _core.loadGame(path.c_str(), data, size);
  }

  if (!loaded)
  {
    // The most common cause of failure is missing system files.
    _logger.debug(TAG "Game load failure (%s)", info ? info->library_name : "Unknown");

    MessageBox(g_mainWindow, "Game load error. Please ensure that required system files are present and restart.", "Core Error", MB_OK);

    if (data)
    {
      free(data);
    }

    // A failure in loadGame typically destroys the core.
    if (_core.getSystemInfo()->library_name == NULL)
      _fsm.unloadCore();

    return false;
  }

  RA_SetConsoleID((unsigned)_system);
  RA_ClearMemoryBanks();

  _gameFileName = unzippedFileName; // store for GetEstimatedTitle callback

  if (!romLoaded(&_logger, _system, path, data, size))
  {
    updateCDMenu(NULL, 0, true);

    _gameFileName.clear();

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

  // reset the vertical sync flag
  SDL_GL_SetSwapInterval(1);

  if (_core.getNumDiscs() == 0)
  {
    updateCDMenu(NULL, 0, true);
  }
  else
  {
    char names[10][128];
    int count = cdrom_get_cd_names(path.c_str(), names, sizeof(names) / sizeof(names[0]), &_logger);
    updateCDMenu(names, count, true);
  }

  _gamePath = path;

  for (size_t i = 0; i < _recentList.size(); i++)
  {
    const RecentItem item = _recentList[i];

    if (item.path == path && item.coreName == _coreName && item.system == _system)
    {
      _recentList.erase(_recentList.begin() + i);
      _recentList.insert(_recentList.begin(), item);
      _logger.debug(TAG "Moved recent file %zu to front \"%s\" - %s - %u", i, util::fileName(item.path).c_str(), item.coreName.c_str(), (unsigned)item.system);
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
    item.coreName = _coreName;
    item.system = _system;

    _recentList.insert(_recentList.begin(), item);
    _logger.debug(TAG "Added recent file \"%s\" - %s - %u", util::fileName(item.path).c_str(), item.coreName.c_str(), (unsigned)item.system);
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

  _memory.attachToCore(&_core, _system);

  _validSlots = 0;

  _states.migrateFiles();

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

  util::saveFile(&_logger, getCoreConfigPath(_coreName), json.c_str(), json.length());

  _memory.destroy();
  _core.destroy();
}

void Application::resetGame()
{
  if (isGameActive())
  {
    _core.resetGame();
    _video.clear();
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

  romUnloaded(&_logger);

  _video.clear();

  _gamePath.clear();
  _gameFileName.clear();
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
  case Fsm::State::GameTurbo:
  case Fsm::State::FrameStep:
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
      SDL_SetWindowSize(_window, height, width);

      /* immediately call windowResized as rotation may change again before the window message is processed */
      _video.windowResized(height, width);
    }
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
  file_types.append("All Files (*.*)");
  file_types.append("\0", 1);
  file_types.append("*.*");
  file_types.append("\0", 1);
  file_types.append("Supported Files (");
  file_types.append(supported_exts);
  file_types.append(")");
  file_types.append("\0", 1);
  file_types.append(supported_exts);
  file_types.append("\0", 1);

  std::string path = util::openFileDialog(g_mainWindow, file_types);

  if (!path.empty())
  {
    /* disc-based system may append discs to the playlist, have to reload core to work around that */
    if (_core.getNumDiscs() > 0)
    {
      std::string coreName = _coreName;
      _fsm.unloadCore();
      _fsm.loadCore(coreName);
    }

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

void Application::updateCDMenu(const char names[][128], int count, bool updateLabels)
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
    char buffer[128];

    MENUITEMINFO info;
    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    GetMenuItemInfo(_menu, IDM_CD_OPEN_TRAY, false, &info);
    info.fMask = MIIM_TYPE | MIIM_DATA;
    info.dwTypeData = (LPSTR)(trayOpen ? "Close Tray" : "Open Tray");
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
        if ((int)i < count)
        {
          info.dwTypeData = (char*)names[i];
        }
        else
        {
          sprintf(buffer, "Disc %d", (int)(i + 1));
          info.dwTypeData = buffer;
        }
      }

      info.fState = (i == selectedDisc) ? MFS_CHECKED : MFS_UNCHECKED;
      if (!trayOpen)
        info.fState |= MF_DISABLED;

      SetMenuItemInfo(_menu, id, false, &info);
    }

    EnableMenuItem(_menu, IDM_CD_OPEN_TRAY, MF_ENABLED);
  }
}

std::string Application::getSRamPath()
{
  return _states.getSRamPath();
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
  _states.saveState(path);
}

void Application::saveState(unsigned ndx)
{
  _states.saveState(ndx);

  _validSlots |= 1 << ndx;
  enableSlots();
}

void Application::saveState()
{
  std::string extensions = "*.STATE";
  extensions.append("\0", 1);
  std::string path = util::saveFileDialog(g_mainWindow, extensions);

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
  if (_states.loadState(path))
  {
    updateCDMenu(NULL, 0, false);
  }
}

void Application::loadState(unsigned ndx)
{
  if ((_validSlots & (1 << ndx)) == 0)
  {
    return;
  }

  if (_states.loadState(ndx))
  {
    updateCDMenu(NULL, 0, false);
  }
}

void Application::loadState()
{
  std::string extensions = "*.STATE";
  extensions.append("\0", 1);
  std::string path = util::openFileDialog(g_mainWindow, extensions);

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

void Application::buildSystemsMenu()
{
  std::set<int> availableSystems;
  getAvailableSystems(availableSystems);
  if (availableSystems.empty())
    return;

  // use map to sort labels
  std::set<std::string> systemCores;
  std::map<std::string, int> systemMap;
  std::map<std::string, int> systemItems;
  for (auto system : availableSystems)
    systemMap.emplace(getSystemName(system), system);

  HMENU fileMenu = GetSubMenu(_menu, 0);
  HMENU systemsMenu = GetSubMenu(fileMenu, 0);

  while (GetMenuItemCount(systemsMenu) > 0)
    DeleteMenu(systemsMenu, 0, MF_BYPOSITION);

  for (const auto& pair : systemMap)
  {
    int system = pair.second;
    systemCores.clear();
    getAvailableSystemCores(system, systemCores);
    if (systemCores.empty())
      continue;

    systemItems.clear();
    for (const auto& systemCore : systemCores)
    {
      int id = encodeCoreName(systemCore, pair.second);
      systemItems.emplace(getEmulatorName(systemCore, system), id);
    }

    HMENU systemMenu = CreateMenu();
    for (const auto& systemItem : systemItems)
      AppendMenu(systemMenu, MF_STRING, IDM_SYSTEM_FIRST + systemItem.second, systemItem.first.c_str());

    AppendMenu(systemsMenu, MF_POPUP | MF_STRING, (UINT_PTR)systemMenu, pair.first.c_str());
  }
}

void Application::loadConfiguration()
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
          else if (event == JSONSAX_OBJECT && num == 0)
          {
            if (ud->w > 0 && ud->h > 0)
            {
              SDL_SetWindowPosition(ud->self->_window, ud->x, ud->y);
              SDL_SetWindowSize(ud->self->_window, ud->w, ud->h);
              ud->self->_logger.debug(TAG "Remembered window position %d,%d (%dx%d)", ud->x, ud->y, ud->w, ud->h);
            }
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
    _video.getFramebufferSize(&width, &height, &format);

    switch (_video.getRotation())
    {
      default:
        SDL_SetWindowSize(_window, width * multiplier, height * multiplier);
        break;

      case Video::Rotation::Ninety:
      case Video::Rotation::TwoSeventy:
        SDL_SetWindowSize(_window, height * multiplier, width * multiplier);
        break;
    }
  }
}

void Application::toggleFullscreen()
{
  Uint32 fullscreen = SDL_GetWindowFlags(_window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
  SDL_SetWindowFullscreen(_window, fullscreen ^ SDL_WINDOW_FULLSCREEN_DESKTOP);
  
  if (fullscreen)
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
      _core.setTrayOpen(!_core.getTrayOpen());
      updateCDMenu(NULL, 0, false);
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
      _config.showDialog(_core.getSystemInfo()->library_name, _input);
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

    case IDM_VIDEO_CONFIG:
      _video.showDialog();
      break;
    
    case IDM_SAVING_CONFIG:
      _states.showDialog();
      break;

    case IDM_WINDOW_1X:
    case IDM_WINDOW_2X:
    case IDM_WINDOW_3X:
    case IDM_WINDOW_4X:
      resizeWindow(cmd - IDM_WINDOW_1X + 1);
      break;

    case IDM_MANAGE_CORES:
      if (showCoresDialog(&_config, &_logger))
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
        if (_core.getCurrentDiscIndex() != newDiscIndex)
        {
          _core.setCurrentDiscIndex(newDiscIndex);

          char buffer[128];
          MENUITEMINFO info;
          memset(&info, 0, sizeof(info));
          info.cbSize = sizeof(info);
          info.fMask = MIIM_TYPE | MIIM_DATA;
          info.dwTypeData = buffer;
          info.cch = sizeof(buffer);
          GetMenuItemInfo(_menu, cmd, false, &info);

          std::string path = util::replaceFileName(_gamePath, buffer);
          romLoaded(&_logger, _system, path, NULL, 0);

          updateCDMenu(NULL, 0, false);
        }
      }
      else if (cmd >= IDM_SYSTEM_FIRST && cmd <= IDM_SYSTEM_LAST)
      {
        const std::string& coreName = getCoreName(cmd - IDM_SYSTEM_FIRST, _system);
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
  if (_video.getViewWidth())
  {
    int rel_x, rel_y, abs_x, abs_y;

    switch (_video.getRotation())
    {
      default:
        rel_x = ((motion->x << 16) / _video.getWindowWidth()) - 32767;
        rel_y = ((motion->y << 16) / _video.getWindowHeight()) - 32767;
        abs_x = (motion->x * _video.getViewWidth()) / _video.getWindowWidth();
        abs_y = (motion->y * _video.getViewHeight()) / _video.getWindowHeight();
        break;

      case Video::Rotation::Ninety:
        rel_x = 32767 - ((motion->y << 16) / _video.getWindowHeight());
        rel_y = ((motion->x << 16) / _video.getWindowWidth()) - 32767;
        abs_x = _video.getViewWidth() - ((motion->y * _video.getViewWidth()) / _video.getWindowHeight()) - 1;
        abs_y = (motion->x * _video.getViewHeight()) / _video.getWindowWidth();
        break;

      case Video::Rotation::OneEighty:
        rel_x = 32767 - ((motion->x << 16) / _video.getWindowWidth());
        rel_y = 32767 - ((motion->y << 16) / _video.getWindowHeight());
        abs_x = _video.getViewWidth() - ((motion->x * _video.getViewWidth()) / _video.getWindowWidth()) - 1;
        abs_y = _video.getViewHeight() - ((motion->y * _video.getViewHeight()) / _video.getWindowHeight()) - 1;
        break;

      case Video::Rotation::TwoSeventy:
        rel_x = ((motion->y << 16) / _video.getWindowHeight());
        rel_y = 32767 - ((motion->x << 16) / _video.getWindowWidth());
        abs_x = (motion->y * _video.getViewWidth()) / _video.getWindowHeight();
        abs_y = _video.getViewWidth() - ((motion->x * _video.getViewHeight()) / _video.getWindowWidth()) - 1;
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
  case KeyBinds::Action::kButtonL2:         _input.buttonEvent(extra >> 8, Input::Button::kL2, (extra & 0xFF) != 0); break;
  case KeyBinds::Action::kButtonR2:         _input.buttonEvent(extra >> 8, Input::Button::kR2, (extra & 0xFF) != 0); break;
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

  // Window size
  case KeyBinds::Action::kSetWindowSize1:   resizeWindow(1); break;
  case KeyBinds::Action::kSetWindowSize2:   resizeWindow(2); break;
  case KeyBinds::Action::kSetWindowSize3:   resizeWindow(3); break;
  case KeyBinds::Action::kSetWindowSize4:   resizeWindow(4); break;
  case KeyBinds::Action::kToggleFullscreen: toggleFullscreen(); break;
  case KeyBinds::Action::kRotateRight:      _video.setRotation((Video::Rotation)(((int)_video.getRotation() + 3) & 3)); break;
  case KeyBinds::Action::kRotateLeft:       _video.setRotation((Video::Rotation)(((int)_video.getRotation() + 1) & 3)); break;

  // Emulation speed
  case KeyBinds::Action::kStep:
    _fsm.step();
    break;

  case KeyBinds::Action::kPauseToggle:
    if (_fsm.currentState() == Fsm::State::GamePaused)
    {
      _fsm.resumeGame();
      RA_SetPaused(false);
    }
    else if (_fsm.currentState() == Fsm::State::GamePausedNoOvl)
    {
      _fsm.resumeGame();
    }
    else
    {
      _fsm.pauseGame();
      RA_SetPaused(true);
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
    if (extra)
    {
      _fsm.turbo();
    }
    else
    {
      _fsm.normal();
    }

    break;
  
  case KeyBinds::Action::kScreenshot:
    screenshot();
    break;

  case KeyBinds::Action::kKeyboardInput:
    _input.keyboardEvent(static_cast<enum retro_key>(extra >> 8), static_cast<bool>(extra & 0xFF));
    break;
  }
}

