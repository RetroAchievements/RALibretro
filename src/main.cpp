#include <SDL.h>
#include <SDL_syswm.h>

#include "libretro/Core.h"
#include "jsonsax/jsonsax.h"

#include "SDLComponents/Allocator.h"
#include "SDLComponents/Audio.h"
#include "SDLComponents/Config.h"
#include "SDLComponents/Input.h"
#include "SDLComponents/Logger.h"
#include "SDLComponents/Video.h"

#include "SDLComponents/Dialog.h"

#include "RA_Integration/RA_Implementation.h"
#include "RA_Integration/RA_Interface.h"
#include "RA_Integration/RA_Resource.h"

#include "KeyBinds.h"

#include "resource.h"

#include <sys/stat.h>
#include <windows.h>
#include <commdlg.h>

#define VERSION "1.0"

HWND g_mainWindow;

static unsigned char memoryRead(unsigned int);
static void memoryWrite(unsigned int, unsigned int);

static unsigned char memoryRead2(unsigned int);
static void memoryWrite2(unsigned int, unsigned int);

class Application
{
protected:
  enum class State
  {
    kError,
    kInitialized,
    kCoreLoaded,
    kGameRunning,
    kGamePaused,
    kGameTurbo
  };

  enum class Emulator
  {
    kNone,
    kStella,
    kSnes9x,
    kPicoDrive,
    kGenesisPlusGx,
    kFceumm,
    kHandy,
    kBeetleSgx,
    kGambatte,
    kMGBA,
    kMednafenPsx,
    kMednafenNgp
  };

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

  State    _state;
  Emulator _emulator;
  System   _system;
  bool     _step;

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
  unsigned    _states;
  unsigned    _slot;

  uint8_t* _memoryData1;
  uint8_t* _memoryData2;
  size_t   _memorySize1;
  size_t   _memorySize2;

  HMENU _menu;

  static void s_audioCallback(void* udata, Uint8* stream, int len)
  {
    Application* app = (Application*)udata;

    if (app->_state != State::kGameTurbo)
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

  void draw()
  {
    if (isGameActive())
    {
      _video.draw();
    }
  }

  size_t nextPow2(size_t v)
  {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
  }

  void signalRomLoadedWithPadding(void* rom, size_t size, size_t max_size, int fill)
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

  void signalRomLoadedNes(void* rom, size_t size)
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

  void signalRomLoadPsx(const char* path)
  {
    (void)path;
  }

  void signalRomLoaded(const char* path, void* rom, size_t size)
  {
    if (_system != System::kPlayStation1 && rom == NULL)
    {
      rom = loadFile(path, &size);
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

  void* loadFile(const char* path, size_t* size)
  {
    void* data;
    struct stat statbuf;

    if (stat(path, &statbuf) != 0)
    {
      _logger.printf(RETRO_LOG_ERROR, "Error getting info from \"%s\": %s", path, strerror(errno));
      return NULL;
    }

    *size = statbuf.st_size;
    data = malloc(*size + 1);

    if (data == NULL)
    {
      _logger.printf(RETRO_LOG_ERROR, "Out of memory allocating %lu bytes to load \"%s\"", *size, path);
      return NULL;
    }

    FILE* file = fopen(path, "rb");

    if (file == NULL)
    {
      _logger.printf(RETRO_LOG_ERROR, "Error opening \"%s\": %s", path, strerror(errno));
      free(data);
      return NULL;
    }

    size_t numread = fread(data, 1, *size, file);

    if (numread < 0 || numread != *size)
    {
      _logger.printf(RETRO_LOG_ERROR, "Error reading \"%s\": %s", path, strerror(errno));
      fclose(file);
      free(data);
      return NULL;
    }

    fclose(file);
    *((uint8_t*)data + *size) = 0;
    return data;
  }

  void saveFile(const char* path, const void* data, size_t size)
  {
    FILE* file = fopen(path, "wb");

    if (file == NULL)
    {
      _logger.printf(RETRO_LOG_ERROR, "Error opening file \"%s\": %s", path, strerror(errno));
      return;
    }

    if (fwrite(data, 1, size, file) != size)
    {
      _logger.printf(RETRO_LOG_ERROR, "Error writing file \"%s\": %s", path, strerror(errno));
      return;
    }

    fclose(file);
  }

  std::string getLogContents() const
  {
    return _logger.contents();
  }

  void updateMenu(const UINT* items, size_t count)
  {
    static const UINT all_items[] =
    {
      IDM_LOAD_GAME,
      IDM_PAUSE_GAME, IDM_RESUME_GAME, IDM_RESET_GAME, IDM_CLOSE_GAME,
      IDM_SAVE_STATE_0, IDM_SAVE_STATE_1, IDM_SAVE_STATE_2, IDM_SAVE_STATE_3, IDM_SAVE_STATE_4,
      IDM_SAVE_STATE_5, IDM_SAVE_STATE_6, IDM_SAVE_STATE_7, IDM_SAVE_STATE_8, IDM_SAVE_STATE_9,
      IDM_LOAD_STATE_0, IDM_LOAD_STATE_1, IDM_LOAD_STATE_2, IDM_LOAD_STATE_3, IDM_LOAD_STATE_4,
      IDM_LOAD_STATE_5, IDM_LOAD_STATE_6, IDM_LOAD_STATE_7, IDM_LOAD_STATE_8, IDM_LOAD_STATE_9,
      IDM_EXIT,

      IDM_CORE, IDM_INPUT, IDM_TURBO,

      IDM_ABOUT
    };

    for (size_t i = 0; i < sizeof(all_items) / sizeof(all_items[0]); i++)
    {
      EnableMenuItem(_menu, all_items[i], MF_DISABLED);
    }

    for (size_t i = 0; i < count; i++)
    {
      EnableMenuItem(_menu, items[i], MF_ENABLED);
    }
  }

  void updateMenu(State state, Emulator system)
  {
    static const UINT all_system_items[] =
    {
      IDM_SYSTEM_STELLA, IDM_SYSTEM_SNES9X, IDM_SYSTEM_PICODRIVE, IDM_SYSTEM_GENESISPLUSGX, IDM_SYSTEM_FCEUMM,
      IDM_SYSTEM_HANDY, IDM_SYSTEM_BEETLESGX, IDM_SYSTEM_GAMBATTE, IDM_SYSTEM_MGBA, IDM_SYSTEM_MEDNAFENPSX,
      IDM_SYSTEM_MEDNAFENNGP
    };

    static const Emulator all_systems[] =
    {
      Emulator::kStella, Emulator::kSnes9x, Emulator::kPicoDrive, Emulator::kGenesisPlusGx, Emulator::kFceumm,
      Emulator::kHandy, Emulator::kBeetleSgx, Emulator::kGambatte, Emulator::kMGBA, Emulator::kMednafenPsx,
      Emulator::kMednafenNgp
    };

    static const UINT error_items[] =
    {
      IDM_EXIT, IDM_ABOUT
    };

    static const UINT initialized_items[] =
    {
      IDM_EXIT, IDM_ABOUT
    };

    static const UINT core_loaded_items[] =
    {
      IDM_LOAD_GAME, IDM_EXIT, IDM_ABOUT
    };

    static const UINT running_items[] =
    {
      IDM_PAUSE_GAME, IDM_RESET_GAME, IDM_CLOSE_GAME,
      IDM_SAVE_STATE_0, IDM_SAVE_STATE_1, IDM_SAVE_STATE_2, IDM_SAVE_STATE_3, IDM_SAVE_STATE_4,
      IDM_SAVE_STATE_5, IDM_SAVE_STATE_6, IDM_SAVE_STATE_7, IDM_SAVE_STATE_8, IDM_SAVE_STATE_9,
      IDM_EXIT,

      IDM_CORE, IDM_INPUT, IDM_TURBO,

      IDM_ABOUT
    };

    static const UINT paused_items[] =
    {
      IDM_LOAD_GAME,
      IDM_RESUME_GAME, IDM_CLOSE_GAME,
      IDM_SAVE_STATE_0, IDM_SAVE_STATE_1, IDM_SAVE_STATE_2, IDM_SAVE_STATE_3, IDM_SAVE_STATE_4,
      IDM_SAVE_STATE_5, IDM_SAVE_STATE_6, IDM_SAVE_STATE_7, IDM_SAVE_STATE_8, IDM_SAVE_STATE_9,
      IDM_EXIT,

      IDM_CORE, IDM_INPUT, IDM_TURBO,

      IDM_ABOUT
    };

    static const UINT load_state_items[] =
    {
      IDM_LOAD_STATE_0, IDM_LOAD_STATE_1, IDM_LOAD_STATE_2, IDM_LOAD_STATE_3, IDM_LOAD_STATE_4,
      IDM_LOAD_STATE_5, IDM_LOAD_STATE_6, IDM_LOAD_STATE_7, IDM_LOAD_STATE_8, IDM_LOAD_STATE_9
    };

    const UINT* items;
    size_t count;

    switch (state)
    {
    case State::kError:
      items = error_items;
      count = sizeof(error_items) / sizeof(error_items[0]);
      break;

    case State::kInitialized:
      items = initialized_items;
      count = sizeof(initialized_items) / sizeof(initialized_items[0]);
      break;
    
    case State::kCoreLoaded:
      items = core_loaded_items;
      count = sizeof(core_loaded_items) / sizeof(core_loaded_items[0]);
      break;
    
    case State::kGameRunning:
    case State::kGameTurbo:
      items = running_items;
      count = sizeof(running_items) / sizeof(running_items[0]);
      break;
    
    case State::kGamePaused:
      items = paused_items;
      count = sizeof(paused_items) / sizeof(paused_items[0]);
      break;
    
    default:
      return;
    }

    updateMenu(items, count);

    if (isGameActive())
    {
      for (unsigned ndx = 0; ndx < 10; ndx++)
      {
        if ((_states & (1 << ndx)) != 0)
        {
          EnableMenuItem(_menu, load_state_items[ndx], MF_ENABLED);
        }
        else
        {
          EnableMenuItem(_menu, load_state_items[ndx], MF_DISABLED);
        }
      }
    }
    else
    {
      for (unsigned ndx = 0; ndx < 10; ndx++)
      {
        EnableMenuItem(_menu, load_state_items[ndx], MF_DISABLED);
      }
    }

    for (size_t i = 0; i < sizeof(all_systems) / sizeof(all_systems[0]); i++)
    {
      EnableMenuItem(_menu, all_system_items[i], _emulator != all_systems[i] ? MF_ENABLED : MF_DISABLED);
    }
  }

  void initCore()
  {
    _config.reset();
    _input.reset();

    if (!_core.init(&_components))
    {
      return;
    }

    std::string path = _config.getRootFolder();
    path += "Cores\\";
    path += getCoreFileName();
    path += ".dll";

    if (!_core.loadCore(path.c_str()))
    {
      _core.destroy();
      return;
    }
  }

  void loadGame()
  {
    initCore();

    char filter[128];

    {
      char* aux = filter;
      const char* end = filter + sizeof(filter);

      const struct retro_system_info* info = _core.getSystemInfo();
      const char* ext = info->valid_extensions;

      aux += sprintf(aux, "All Files") + 1;
      aux += sprintf(aux, "*.*") + 1;
      aux += sprintf(aux, "Supported Files") + 1;

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
      loadGame(game_path, false);
    }
  }

  void loadGame(const char* game_path, bool init_core)
  {
    if (init_core)
    {
      initCore();
    }

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
      data = loadFile(game_path, &size);

      if (data == NULL)
      {
        return;
      }
    }
      
    if (!_core.loadGame(game_path, data, size))
    {
      free(data);
      return;
    }

    int game_path_len = strlen(game_path);

    switch (_emulator)
    {
    case Emulator::kNone:
      return;

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
      if (!strcasecmp(game_path + game_path_len - 4, ".gbc"))
      {
        _system = System::kGameBoyColor;
      }
      else if (!strcasecmp(game_path + game_path_len - 4, ".gba"))
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
    signalRomLoaded(game_path, data, size);
    free(data);
    _gamePath = game_path;

    if (_core.getMemorySize(RETRO_MEMORY_SAVE_RAM) != 0)
    {
      std::string sram = getSRAMPath();
      data = loadFile(sram.c_str(), &size);

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
      _memoryData1 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      _memorySize1 = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
      RA_InstallMemoryBank(0,	(void*)::memoryRead, (void*)::memoryWrite, _memorySize1);
      break;

    case Emulator::kSnes9x:
    case Emulator::kFceumm:
      _memoryData1 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      _memorySize1 = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
      RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, _memorySize1);

      _memoryData2 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
      _memorySize2 = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);
      RA_InstallMemoryBank(1, (void*)::memoryRead2, (void*)::memoryWrite2, _memorySize2);

      break;
    
    case Emulator::kMGBA:
      if (_system == System::kGameBoyAdvance)
      {
        const struct retro_memory_map* mmap = _core.getMemoryMap();

        for (unsigned i = 0; i < mmap->num_descriptors; i++)
        {
          if (mmap->descriptors[i].start == 0x03000000U)
          {
            _memoryData1 = (uint8_t*)mmap->descriptors[i].ptr;
            _memorySize1 = mmap->descriptors[i].len;
            RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, _memorySize1);
          }
          else if (mmap->descriptors[i].start == 0x02000000U)
          {
            _memoryData2 = (uint8_t*)mmap->descriptors[i].ptr;
            _memorySize2 = mmap->descriptors[i].len;
            RA_InstallMemoryBank(1, (void*)::memoryRead2, (void*)::memoryWrite2, _memorySize2);
          }
        }
      }
      else
      {
        _memoryData1 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
        _memorySize1 = 32768;
        RA_InstallMemoryBank(0,	(void*)::memoryRead, (void*)::memoryWrite, _memorySize1);
      }

      break;
    }

    _states = 0;

    for (unsigned ndx = 0; ndx < 10; ndx++)
    {
      std::string path = getStatePath(ndx);
      struct stat statbuf;

      if (stat(path.c_str(), &statbuf) == 0)
      {
        _states |= 1 << ndx;
      }
    }

    data = loadFile(getCoreConfigPath().c_str(), &size);

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

    _state = State::kGameRunning;
    updateMenu(_state, _emulator);
  }

  void unloadGame()
  {
    size_t size = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);

    if (size != 0)
    {
      void* data = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
      std::string sram = getSRAMPath();
      saveFile(sram.c_str(), data, size);
    }
  }

  void unloadCore()
  {
    if (isGameActive())
    {
      RA_ClearMemoryBanks();
      unloadGame();

      std::string json;
      json.append("{\"core\":");
      json.append(_config.serialize());
      json.append(",\"input\":");
      json.append(_input.serialize());
      json.append("}");

      saveFile(getCoreConfigPath().c_str(), json.c_str(), json.length());

      _core.destroy();
    }
  }

  std::string getSRAMPath()
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

  std::string getStatePath(unsigned ndx)
  {
    std::string path = _config.getSaveDirectory();
    mkdir(path.c_str());

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
    path += getCoreFileName();
    
    char index[64];
    sprintf(index, ".%03u", ndx);

    path += index;
    path += ".state";

    return path;
  }

  const char* getCoreFileName()
  {
    switch (_emulator)
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

    return NULL;
  }

  std::string getCoreConfigPath()
  {
    std::string path = _config.getRootFolder();
    path += "Cores\\";
    path += getCoreFileName();
    path += ".json";
    return path;
  }

  void saveState(unsigned ndx)
  {
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
    saveFile(path.c_str(), data, size);
    free(data);

    _states |= 1 << ndx;
    updateMenu(_state, _emulator);
  }

  void loadState(unsigned ndx)
  {
    if ((_states & (1 << ndx)) != 0)
    {
      std::string path = getStatePath(ndx);
      size_t size;
      void* data = loadFile(path.c_str(), &size);
      
      if (data != NULL)
      {
        _core.unserialize(data, size);
        free(data);
      }
    }
  }

  void configureCore()
  {
    _config.showDialog();
  }

  void configureInput()
  {
    _input.showDialog();
  }

  void showAbout()
  {
    const WORD WIDTH = 280;
    const WORD LINE = 15;

    Dialog db;
    db.init("About");

    WORD y = 0;

    db.addLabel("RALibretro " VERSION " \u00A9 2017-2018 Andre Leiradella @leiradel", 0, y, WIDTH, 8);
    y += LINE;

    db.addEditbox(40000, 0, y, WIDTH, LINE, 12, (char*)getLogContents().c_str(), 0, true);
    y += LINE * 12;

    db.addButton("OK", IDOK, WIDTH - 50, y, 50, 14, true);
    db.show();
  }

  void handle(const SDL_Event* event, bool* done)
  {
    if (event->syswm.msg->msg.win.msg == WM_PAINT)
    {
      RA_OnPaint(g_mainWindow);
    }
    else if (event->syswm.msg->msg.win.msg == WM_COMMAND)
    {
      WORD cmd = LOWORD(event->syswm.msg->msg.win.wParam);

      switch (cmd)
      {
      case IDM_SYSTEM_STELLA:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kStella;
        updateMenu(_state, _emulator);
        break;
      
      case IDM_SYSTEM_SNES9X:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kSnes9x;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_PICODRIVE:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kPicoDrive;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_GENESISPLUSGX:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kGenesisPlusGx;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_FCEUMM:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kFceumm;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_HANDY:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kHandy;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_BEETLESGX:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kBeetleSgx;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_GAMBATTE:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kGambatte;
        updateMenu(_state, _emulator);
        break;

      case IDM_SYSTEM_MGBA:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kMGBA;
        updateMenu(_state, _emulator);
        break;
      
      case IDM_SYSTEM_MEDNAFENPSX:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kMednafenPsx;
        updateMenu(_state, _emulator);
        break;
      
      case IDM_SYSTEM_MEDNAFENNGP:
        unloadCore();
        _state = State::kCoreLoaded;
        _emulator = Emulator::kMednafenNgp;
        updateMenu(_state, _emulator);
        break;

      case IDM_LOAD_GAME:
        loadGame();
        break;
      
      case IDM_PAUSE_GAME:
        _state = State::kGamePaused;
        RA_SetPaused(true);
        updateMenu(_state, _emulator);
        break;

      case IDM_RESUME_GAME:
        _state = State::kGameRunning;
        RA_SetPaused(false);
        updateMenu(_state, _emulator);
        break;
      
      case IDM_RESET_GAME:
        _core.resetGame();
        break;
      
      case IDM_CLOSE_GAME:
        unloadCore();
        _state = State::kCoreLoaded;
        updateMenu(_state, _emulator);
        break;

      case IDM_SAVE_STATE_0:
        saveState(0);
        break;

      case IDM_SAVE_STATE_1:
        saveState(1);
        break;
        
      case IDM_SAVE_STATE_2:
        saveState(2);
        break;
        
      case IDM_SAVE_STATE_3:
        saveState(3);
        break;
        
      case IDM_SAVE_STATE_4:
        saveState(4);
        break;
        
      case IDM_SAVE_STATE_5:
        saveState(5);
        break;
        
      case IDM_SAVE_STATE_6:
        saveState(6);
        break;
        
      case IDM_SAVE_STATE_7:
        saveState(7);
        break;
        
      case IDM_SAVE_STATE_8:
        saveState(8);
        break;
        
      case IDM_SAVE_STATE_9:
        saveState(9);
        break;
        
      case IDM_LOAD_STATE_0:
        loadState(0);
        break;
        
      case IDM_LOAD_STATE_1:
        loadState(1);
        break;
        
      case IDM_LOAD_STATE_2:
        loadState(2);
        break;
        
      case IDM_LOAD_STATE_3:
        loadState(3);
        break;
        
      case IDM_LOAD_STATE_4:
        loadState(4);
        break;
        
      case IDM_LOAD_STATE_5:
        loadState(5);
        break;
        
      case IDM_LOAD_STATE_6:
        loadState(6);
        break;
        
      case IDM_LOAD_STATE_7:
        loadState(7);
        break;
        
      case IDM_LOAD_STATE_8:
        loadState(8);
        break;
        
      case IDM_LOAD_STATE_9:
        loadState(9);
        break;
        
      case IDM_CORE:
        configureCore();
        break;

      case IDM_INPUT:
        configureInput();
        break;
      
      case IDM_TURBO:
        if (_state == State::kGameRunning || _state == State::kGamePaused)
        {
          _state = State::kGameTurbo;
        }
        else if (_state == State::kGameTurbo)
        {
          _state = State::kGameRunning;
        }

        break;

      case IDM_EXIT:
        unloadCore();
        *done = true;
        break;
        
      case IDM_ABOUT:
        showAbout();
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

  void shortcut(const SDL_Event* event, bool* done)
  {
    unsigned extra;

    switch (_keybinds.translate(event, &extra))
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
    // State state management
    case KeyBinds::Action::kSetStateSlot: _slot = extra; break;
    case KeyBinds::Action::kSaveState:    saveState(_slot); break;
    case KeyBinds::Action::kLoadState:    loadState(_slot); break;
    case KeyBinds::Action::kStep:         _state = State::kGamePaused; _step = true; break;
    // Emulation speed
    case KeyBinds::Action::kPauseToggle:
      if (_state == State::kGameRunning)
      {
        _state = State::kGamePaused;
        RA_SetPaused(true);
        updateMenu(_state, _emulator);
      }
      else if (_state == State::kGamePaused)
      {
        _state = State::kGameRunning;
        RA_SetPaused(false);
        updateMenu(_state, _emulator);
      }

      break;

    case KeyBinds::Action::kFastForward:
      if (extra != 0 && _state == State::kGameRunning)
      {
        if (_state == State::kGamePaused)
        {
          RA_SetPaused(false);
        }

        _state = State::kGameTurbo;
        updateMenu(_state, _emulator);
      }
      else if (extra == 0 && _state == State::kGameTurbo)
      {
        _state = State::kGameRunning;
        updateMenu(_state, _emulator);
      }

      break;
    }
  }

public:
  Application()
  {
    _components._logger    = &_logger;
    _components._config    = &_config;
    _components._video     = &_video;
    _components._audio     = &_audio;
    _components._input     = &_input;
    _components._allocator = &_allocator;
  }

  bool init(const char* title, int width, int height)
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

    _state = State::kError;
    _emulator = Emulator::kNone;
    _step = false;

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
    _window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

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

      RA_Init(g_mainWindow, RA_Libretro, "1.0");
      RA_InitShared();
      RA_InitDirectX();
      RA_UpdateAppTitle( "" );
      RebuildMenu();
      RA_AttemptLogin(true);
      RebuildMenu();
    }

    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    _state = State::kInitialized;
    _emulator = Emulator::kNone;
    _slot = 0;
    updateMenu(_state, _emulator);
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

    _state = State::kError;
    _emulator = Emulator::kNone;
    updateMenu(_state, _emulator);
    return false;
  }

  void destroy()
  {
    unloadCore();

    if (_state == State::kInitialized)
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
  }

  void run()
  {
    bool done = false;

    do
    {
      SDL_Event event;

      while (SDL_PollEvent(&event))
      {
        switch (event.type)
        {
        case SDL_QUIT:
          done = true;
          break;

        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERAXISMOTION:
          _input.processEvent(&event);
          break;
        
        case SDL_SYSWMEVENT:
          handle(&event, &done);
          break;
        
        case SDL_KEYUP:
        case SDL_KEYDOWN:
          shortcut(&event, &done);
          break;
        }
      }

      int limit = 0;
      bool audio = false;

      if (_state == State::kGameRunning)
      {
        limit = 1;
        audio = true;
      }
      else if (_state == State::kGamePaused && _step)
      {
        limit = 1;
        audio = false;
        _step = false;
      }
      else if (_state == State::kGameTurbo)
      {
        limit = 5;
        audio = false;
      }

      for (int i = 0; i < limit; i++)
      {
        _core.step(audio);
      }

      SDL_RenderClear(_renderer);
      draw();
      SDL_RenderPresent(_renderer);
    	RA_DoAchievementsFrame();
	    RA_HandleHTTPResults();

      {
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

            RA_UpdateRenderOverlay(hdc, &input, (t1 - t0) / 1000.0f, &size, false, _state == State::kGamePaused);
            ReleaseDC(g_mainWindow, hdc);
          }

          t0 = t1;
        }
      }

      SDL_Delay(1);
    }
    while (!done);
  }

  unsigned char memoryRead(unsigned int addr)
  {
    return _memoryData1[addr];
  }

  void memoryWrite(unsigned int addr, unsigned int value)
  {
    _memoryData1[addr] = value;
  }

  unsigned char memoryRead2(unsigned int addr)
  {
    return _memoryData2[addr];
  }

  void memoryWrite2(unsigned int addr, unsigned int value)
  {
    _memoryData2[addr] = value;
  }

  bool isGameActive() const
  {
    return _state == State::kGameRunning || _state == State::kGamePaused || _state == State::kGameTurbo;
  }

  void pause()
  {
    _state = State::kGamePaused;
    RA_SetPaused(true);
    updateMenu(_state, _emulator);
  }

  void resume()
  {
    _state = State::kGameRunning;
    RA_SetPaused(false);
    updateMenu(_state, _emulator);
  }

  void reset()
  {
    _core.resetGame();
  }

  void loadROM(const char* path)
  {
    loadGame(path, true);
  }
};

static Application app;

static unsigned char memoryRead(unsigned int addr)
{
	return app.memoryRead(addr);
}

static void memoryWrite(unsigned int addr, unsigned int value)
{
	app.memoryWrite(addr, value);
}

static unsigned char memoryRead2(unsigned int addr)
{
	return app.memoryRead2(addr);
}

static void memoryWrite2(unsigned int addr, unsigned int value)
{
	app.memoryWrite2(addr, value);
}

bool isGameActive()
{
  return app.isGameActive();
}

void pause()
{
  app.pause();
}

void resume()
{
  app.resume();
}

void reset()
{
  app.reset();
}

void loadROM(const char* path)
{
  app.loadROM(path);
}

int main(int argc, char* argv[])
{
  bool ok = app.init("RALibretro", 640, 480);

  if (ok)
  {
    app.run();
  }

  app.destroy();
  return ok ? 0 : 1;
}
