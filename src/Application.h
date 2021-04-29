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

#pragma once

#include <string>
#include <vector>

#include <SDL.h>
#include "Fsm.h"

#include <RA_Interface.h>

#include "components/Allocator.h"
#include "components/Audio.h"
#include "components/Config.h"
#include "components/Input.h"
#include "components/Logger.h"
#include "components/VideoContext.h"
#include "components/Video.h"

#include "Emulator.h"
#include "KeyBinds.h"
#include "Memory.h"
#include "States.h"

class Application
{
public:
  Application();

  // Lifecycle
  bool init(const char* title, int width, int height);
  void run();
  void destroy();

  // FSM
  bool loadCore(const std::string& coreName);
  void updateMenu();
  bool loadGame(const std::string& path);
  void unloadCore();
  void resetGame();
  bool hardcore();
  bool unloadGame();
  void pauseGame(bool pause);

  bool isTurbo() {
    return _config.getFastForwarding();
  }

  void printf(const char* fmt, ...);
  Logger& logger() { return _logger; }

  // RA_Integration
  bool isGameActive();
  const std::string& gameName() const { return _gameFileName; }
  bool validateHardcoreEnablement();

  void onRotationChanged(Video::Rotation oldRotation, Video::Rotation newRotation);

  void refreshMemoryMap();

protected:
  struct RecentItem
  {
    std::string path;
    std::string coreName;
    int system;
  };

  // Called by SDL from the audio thread
  static void s_audioCallback(void* udata, Uint8* stream, int len);

  // Helpers
  void        processEvents();
  void        runSmoothed();
  void        runTurbo();

  void        loadGame();
  void        enableItems(const UINT* items, size_t count, UINT enable);
  void        enableSlots();
  void        enableRecent();
  void        updateCDMenu(const char names[][128], int count, bool updateLabels);
  std::string getStatePath(unsigned ndx);
  std::string getConfigPath();
  std::string getCoreConfigPath(const std::string& coreName);
  std::string getScreenshotPath();
  void        saveState(const std::string& path);
  void        saveState(unsigned ndx);
  void        saveState();
  void        loadState(const std::string& path);
  void        loadState(unsigned ndx);
  void        loadState();
  void        screenshot();
  void        aboutDialog();
  void        resizeWindow(unsigned multiplier);
  void        resizeWindow(int width, int height);
  void        toggleFullscreen();
  void        handle(const SDL_SysWMEvent* syswm);
  void        handle(const SDL_WindowEvent* window);
  void        handle(const SDL_MouseMotionEvent* motion);
  void        handle(const SDL_MouseButtonEvent* button);
  void        handle(const KeyBinds::Action action, unsigned extra);
  void        buildSystemsMenu();
  void        loadConfiguration();
  void        saveConfiguration();
  std::string serializeRecentList();

  Fsm _fsm;
  bool lastHardcore;
  bool cancelLoad;

  std::string _coreName;
  int         _system;

  SDL_Window*       _window;
  SDL_AudioSpec     _audioSpec;
  SDL_AudioDeviceID _audioDev;

  Fifo         _fifo;
  Logger       _logger;
  Config       _config;
  VideoContext _videoContext;
  Video        _video;
  Audio        _audio;
  Input        _input;
  Memory       _memory;
  States       _states;

  KeyBinds _keybinds;
  std::vector<RecentItem> _recentList;

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;
  libretro::Core       _core;

  std::string _gamePath;
  std::string _gameFileName;
  unsigned    _validSlots;

  HMENU _menu;
  HMENU _cdRomMenu;
};
