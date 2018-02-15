#pragma once

#include <SDL.h>
#include "Fsm.h"

#include "RA_Integration/RA_Interface.h"

#include "SDLComponents/Allocator.h"
#include "SDLComponents/Audio.h"
#include "SDLComponents/Config.h"
#include "SDLComponents/Input.h"
#include "SDLComponents/Logger.h"
#include "SDLComponents/Video.h"

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
  bool canQuit();
  bool loadGame(const std::string& path);
  bool canUnloadCore();
  void unloadCore();
  void resetGame();
  bool hardcore();
  void setTurbo(bool turbo);
  bool canUnloadGame();
  void unloadGame();
  void pauseGame(bool pause);

  void printf(const char* fmt, ...);

  // RA_Integration
  unsigned char memoryRead(unsigned int addr);
  void memoryWrite(unsigned int addr, unsigned int value);
  bool isGameActive();

protected:
  enum class System
  {
    kAtari2600      = VCS,
    kAtariLynx      = Lynx,
    kMasterSystem   = MasterSystem,
    kMegaDrive      = MegaDrive,
    kNintendo       = NES,
    kPCEngine       = PCEngine,
    kSuperNintendo  = SNES,
    kGameBoy        = GB,
    kGameBoyColor   = GBC,
    kGameBoyAdvance = GBA,
    kPlayStation1   = PlayStation,
    kNeoGeoPocket   = NeoGeo
  };

  struct MemoryRegion
  {
    uint8_t* data;
    size_t size;
  };

  // Called by SDL from the audio thread
  static void s_audioCallback(void* udata, Uint8* stream, int len);

  // Helpers
  void        signalRomLoadedWithPadding(void* rom, size_t size, size_t max_size, int fill);
  void        signalRomLoadedNes(void* rom, size_t size);
  void        signalRomLoadPsx(const std::string& path);
  void        signalRomLoaded(const std::string& path, void* rom, size_t size);
  void        loadGame();
  void        enableItems(const UINT* items, size_t count, UINT enable);
  void        enableSlots();
  void        registerMemoryRegion(void* data, size_t size);
  std::string getSRamPath();
  std::string getStatePath(unsigned ndx);
  std::string getCoreConfigPath(Emulator emulator);
  std::string getCoreFileName(Emulator emulator);
  void        saveState(unsigned ndx);
  void        loadState(unsigned ndx);
  void        handle(const SDL_Event* event);

  Fsm _fsm;

  Emulator _emulator;
  System   _system;

  SDL_Window*       _window;
  SDL_Renderer*     _renderer;
  SDL_AudioSpec     _audioSpec;
  SDL_AudioDeviceID _audioDev;

  Fifo   _fifo;
  Logger _logger;
  Config _config;
  Video  _video;
  Audio  _audio;
  Input  _input;

  KeyBinds _keybinds;

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;
  libretro::Core       _core;

  std::string _gamePath;
  unsigned    _validSlots;
  unsigned    _currentSlot;

  MemoryRegion _memoryRegions[64];
  unsigned     _memoryRegionCount;

  HMENU _menu;
};
