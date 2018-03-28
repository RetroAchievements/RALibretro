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
#include "components/Video.h"

#include "Emulator.h"
#include "KeyBinds.h"

class Application
{
public:
  Application();

  // Lifecycle
  bool init(const char* title, int width, int height);
  void run();
  void destroy();

  // FSM
  bool loadCore(Emulator emulator);
  void updateMenu();
  bool loadGame(const std::string& path);
  void unloadCore();
  void resetGame();
  bool hardcore();
  void setTurbo(bool turbo);
  bool unloadGame();
  void pauseGame(bool pause);

  void printf(const char* fmt, ...);

  // RA_Integration
  unsigned char memoryRead(unsigned int addr);
  void memoryWrite(unsigned int addr, unsigned int value);
  bool isGameActive();

protected:
  struct MemoryRegion
  {
    uint8_t* data;
    size_t size;
  };

  struct RecentItem
  {
    std::string path;
    Emulator emulator;
    System system;
  };

  // Called by SDL from the audio thread
  static void s_audioCallback(void* udata, Uint8* stream, int len);

  // Helpers
  void        loadGame();
  void        enableItems(const UINT* items, size_t count, UINT enable);
  void        enableSlots();
  void        enableRecent();
  void        registerMemoryRegion(void* data, size_t size);
  std::string getSRamPath();
  std::string getStatePath(unsigned ndx);
  std::string getConfigPath();
  std::string getCoreConfigPath(Emulator emulator);
  std::string getScreenshotPath();
  void        saveState(unsigned ndx);
  void        loadState(unsigned ndx);
  void        screenshot();
  void        aboutDialog();
  void        loadRecentList();
  std::string serializeRecentList();
  void        handle(const SDL_SysWMEvent* syswm);
  void        handle(const SDL_WindowEvent* window);
  void        handle(const SDL_KeyboardEvent* key);

  Fsm _fsm;
  bool lastHardcore;

  Emulator _emulator;
  System   _system;

  SDL_Window*       _window;
  SDL_AudioSpec     _audioSpec;
  SDL_AudioDeviceID _audioDev;

  Fifo   _fifo;
  Logger _logger;
  Config _config;
  Video  _video;
  Audio  _audio;
  Input  _input;

  KeyBinds _keybinds;
  std::vector<RecentItem> _recentList;

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;
  libretro::Core       _core;

  std::string _gamePath;
  unsigned    _validSlots;

  MemoryRegion _memoryRegions[64];
  unsigned     _memoryRegionCount;

  HMENU _menu;
};
