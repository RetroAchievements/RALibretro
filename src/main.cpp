#include <SDL.h>
#include <SDL_syswm.h>

#include "libretro/Core.h"

#include "SDLComponents/Allocator.h"
#include "SDLComponents/Audio.h"
#include "SDLComponents/Config.h"
#include "SDLComponents/Input.h"
#include "SDLComponents/Logger.h"
#include "SDLComponents/Video.h"

#include "RA_Integration/RA_Implementation.h"
#include "RA_Integration/RA_Interface.h"
#include "RA_Integration/RA_Resource.h"

#include "resource.h"

#include <sys/stat.h>
#include <windows.h>
#include <commdlg.h>

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

  enum class System
  {
    kNone,
    kStella,
    kSnes9x,
    kPicoDrive,
    kGenesisPlusGx
  };

  State  _state;
  System _system;

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

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;
  libretro::Core       _core;

  std::string _gamePath;
  unsigned    _states;

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

  static INT_PTR CALLBACK s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
  {
    Application* self;

    switch (msg)
    {
    case WM_INITDIALOG:
      self = (Application*)lparam;
      SetDlgItemText(hwnd, IDC_LOG, self->getLogContents().c_str());
      return TRUE;
    
    case WM_SETFONT:
      return TRUE;

    case WM_COMMAND:
      if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)
      {
        EndDialog(hwnd, 0);
        return TRUE;
      }

      break;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return TRUE;
    }

    return FALSE;
  }

  void draw()
  {
    if (isGameActive())
    {
      _video.draw();
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

  void updateMenu(State state, System system)
  {
    static const UINT all_system_items[] =
    {
      IDM_SYSTEM_STELLA, IDM_SYSTEM_SNES9X, IDM_SYSTEM_PICODRIVE, IDM_SYSTEM_GENESISPLUSGX
    };

    static const System all_systems[] =
    {
      System::kStella, System::kSnes9x, System::kPicoDrive, System::kGenesisPlusGx
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
      EnableMenuItem(_menu, all_system_items[i], _system != all_systems[i] ? MF_ENABLED : MF_DISABLED);
    }
  }

  void initCore()
  {
    const char* core_path;

    switch (_system)
    {
    case System::kNone:
      return;

    case System::kStella:
      core_path = "stella_libretro.dll";
      break;

    case System::kSnes9x:
      core_path = "snes9x_libretro.dll";
      break;

    case System::kPicoDrive:
      core_path = "picodrive_libretro.dll";
      break;
    
    case System::kGenesisPlusGx:
      core_path = "genesis_plus_gx_libretro.dll";
      break;
    }

    _config.reset();

    if (!_core.init(&_components))
    {
      return;
    }

    if (!_core.loadCore(core_path))
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

    size_t size;
    void* data = loadFile(game_path, &size);

    if (data == NULL)
    {
      return;
    }
    
    if (!_core.loadGame(game_path, data, size))
    {
      free(data);
      return;
    }

    switch (_system)
    {
    case System::kNone:
      return;

    case System::kStella:
      RA_SetConsoleID(VCS);
      break;

    case System::kSnes9x:
      RA_SetConsoleID(SNES);
      break;

    case System::kPicoDrive:
    case System::kGenesisPlusGx:
      if (_core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM) == 0x2000)
      {
        RA_SetConsoleID(MasterSystem);
      }
      else
      {
        RA_SetConsoleID(MegaDrive);
      }

      break;
    }

    RA_ClearMemoryBanks();
    RA_OnLoadNewRom((BYTE*)data, size);
    free(data);
    _gamePath = game_path;

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

    switch (_system)
    {
    case System::kNone:
      break;

    case System::kStella:
      _memoryData1 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      _memorySize1 = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
      RA_InstallMemoryBank(0,	(void*)::memoryRead, (void*)::memoryWrite, _memorySize1);
      break;

    case System::kSnes9x:
      _memoryData1 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      _memorySize1 = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
      RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, _memorySize1);
      _memoryData2 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SAVE_RAM);
      _memorySize2 = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);
      RA_InstallMemoryBank(1, (void*)::memoryRead2, (void*)::memoryWrite2, _memorySize2);
      break;
    
    case System::kPicoDrive:
    case System::kGenesisPlusGx:
      _memoryData1 = (uint8_t*)_core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM);
      _memorySize1 = _core.getMemorySize(RETRO_MEMORY_SYSTEM_RAM);
      RA_InstallMemoryBank(0, (void*)::memoryRead, (void*)::memoryWrite, _memorySize1);
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
      _config.unserialize((char*)data);
      free(data);
    }

    _state = State::kGameRunning;
    _input.setDefaultController();
    updateMenu(_state, _system);
  }

  void unloadGame()
  {
    size_t size = _core.getMemorySize(RETRO_MEMORY_SAVE_RAM);
    void* data = _core.getMemoryData(RETRO_MEMORY_SAVE_RAM);

    std::string sram = getSRAMPath();
    saveFile(sram.c_str(), data, size);
  }

  std::string getSRAMPath()
  {
    std::string path = ".\\Saves\\";
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
    
    path += ".sram";
    return path;
  }

  std::string getStatePath(unsigned ndx)
  {
    std::string path = ".\\Saves\\";
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
    
    char index[64];
    sprintf(index, ".%03u", ndx);

    path += index;
    path += ".state";

    return path;
  }

  std::string getCoreConfigPath()
  {
    std::string path = _config.getSystemPath();

    switch (_system)
    {
    case System::kNone:
      break;

    case System::kStella:
      path.append("stella_libretro.json");
      break;

    case System::kSnes9x:
      path.append("snes9x_libretro.json");
      break;
    
    case System::kPicoDrive:
      path.append("picodrive_libretro.json");
      break;

    case System::kGenesisPlusGx:
      path.append("genesis_plus_gx_libretro.json");
      break;
    }

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
    updateMenu(_state, _system);
  }

  void loadState(unsigned ndx)
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

  void configureCore()
  {
    _config.showDialog();
  }

  void configureInput()
  {

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
        if (isGameActive())
        {
          RA_ClearMemoryBanks();
          unloadGame();
          _core.destroy();
        }

        _state = State::kCoreLoaded;
        _system = System::kStella;
        updateMenu(_state, _system);
        break;
      
      case IDM_SYSTEM_SNES9X:
        if (isGameActive())
        {
          RA_ClearMemoryBanks();
          unloadGame();
          _core.destroy();
        }
        
        _state = State::kCoreLoaded;
        _system = System::kSnes9x;
        updateMenu(_state, _system);
        break;

      case IDM_SYSTEM_PICODRIVE:
        if (isGameActive())
        {
          RA_ClearMemoryBanks();
          unloadGame();
          _core.destroy();
        }
        
        _state = State::kCoreLoaded;
        _system = System::kPicoDrive;
        updateMenu(_state, _system);
        break;

      case IDM_SYSTEM_GENESISPLUSGX:
        if (isGameActive())
        {
          RA_ClearMemoryBanks();
          unloadGame();
          _core.destroy();
        }
        
        _state = State::kCoreLoaded;
        _system = System::kGenesisPlusGx;
        updateMenu(_state, _system);
        break;

      case IDM_LOAD_GAME:
        loadGame();
        break;
      
      case IDM_PAUSE_GAME:
        _state = State::kGamePaused;
        RA_SetPaused(true);
        updateMenu(_state, _system);
        break;

      case IDM_RESUME_GAME:
        _state = State::kGameRunning;
        RA_SetPaused(false);
        updateMenu(_state, _system);
        break;
      
      case IDM_RESET_GAME:
        _core.resetGame();
        break;
      
      case IDM_CLOSE_GAME:
        _state = State::kCoreLoaded;
        RA_ClearMemoryBanks();
        unloadGame();
        _core.destroy();
        updateMenu(_state, _system);
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
        *done = true;
        break;
        
      case IDM_ABOUT:
        DialogBoxParam(NULL, "ABOUT", g_mainWindow, s_dialogProc, (LPARAM)this);
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
      kVideoInited
    }
    inited = kNothingInited;

    _state = State::kError;
    _system = System::kNone;

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
    _system = System::kNone;
    updateMenu(_state, _system);
    return true;

  error:
    switch (inited)
    {
    case kVideoInited:       _video.destroy();
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
    _system = System::kNone;
    updateMenu(_state, _system);
    return false;
  }

  void destroy()
  {
    std::string config = _config.serialize();
    saveFile(getCoreConfigPath().c_str(), config.c_str(), config.length());

    if (isGameActive())
    {
      unloadGame();
      _core.destroy();
    }

    if (_state == State::kInitialized)
    {
      _video.destroy();
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
        }
      }

      if (_state == State::kGameRunning)
      {
        _core.step();
      }
      else if (_state == State::kGameTurbo)
      {
        for (int i = 0; i < 5; i++)
        {
          _core.step();

          for (;;)
          {
            char dummy[512];
            size_t avail = _fifo.occupied();

            if (avail == 0)
            {
              break;
            }
            else if (avail > sizeof(dummy))
            {
              avail = sizeof(dummy);
            }

            _fifo.read((void*)dummy, avail);
          }
        }
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
    updateMenu(_state, _system);
  }

  void resume()
  {
    _state = State::kGameRunning;
    RA_SetPaused(false);
    updateMenu(_state, _system);
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

int main(int, char**)
{
  bool ok = app.init("RALibretro", 640, 480);

  if (ok)
  {
    app.run();
  }

  app.destroy();
  return ok ? 0 : 1;
}
