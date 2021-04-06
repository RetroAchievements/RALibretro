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

#include "States.h"

#include "Util.h"

#include "libretro/Core.h"

#include "resource.h"

#include "RA_Interface.h"

#include <jsonsax/jsonsax.h>
#include <rcheevos/include/rconsoles.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#include <commdlg.h>
#include <shlobj.h>

#include <assert.h>
#include <time.h>

#define TAG "[SAV] "

#define RASTATE_VERSION 1
#define RASTATE_MEM_BLOCK "MEM "
#define RASTATE_CHEEVOS_BLOCK "ACHV"
#define RASTATE_SCREEN_BLOCK "SCRN"
#define RASTATE_END_BLOCK "END "

extern HWND g_mainWindow;

bool States::init(Logger* logger, Config* config, Video* video)
{
  _logger = logger;
  _config = config;
  _video = video;
  _core = NULL;
  _lastSave = 0;

  return true;
}

void States::setGame(const std::string& gameFileName, int system, const std::string& coreName, libretro::Core* core)
{
  _gameFileName = gameFileName;
  _system = system;
  _coreName = coreName;
  _core = core;

  _config->setSaveDirectory(buildPath(_sramPath));

  if (_lastSaveData != NULL)
  {
    free(_lastSaveData);
    _lastSaveData = NULL;
  }
}

std::string States::buildPath(Path path) const
{
  std::string savePath = _config->getRootFolder();

  if (path & Path::State)
    savePath += "States\\";
  else
    savePath += "Saves\\";

  if ((path & Path::System) && _system)
  {
    savePath += util::sanitizeFileName(rc_console_name(_system));
    savePath += '\\';
  }

  if ((path & Path::Core) && _core->getSystemInfo()->library_name)
  {
    savePath += util::sanitizeFileName(_core->getSystemInfo()->library_name);
    savePath += '\\';
  }

  if ((path & Path::Game) && !_gameFileName.empty())
    savePath += util::fileName(_gameFileName) + "\\";

  return savePath;
}

std::string States::getSRamPath() const
{
  return getSRamPath(_sramPath);
}

std::string States::getSRamPath(Path path) const
{
  std::string sramPath = buildPath(path);

  if (path == Path::Saves)
    sramPath += _gameFileName;
  else
    sramPath += util::fileName(_gameFileName);

  sramPath += ".sram";
  return sramPath;
}

std::string States::getStatePath(unsigned ndx) const
{
  return getStatePath(ndx, _statePath, false);
}

std::string States::getStatePath(unsigned ndx, Path path, bool bOldFormat) const
{
  std::string statePath = buildPath(path);

  if (path == Path::Saves)
    statePath += _gameFileName;
  else
    statePath += util::fileName(_gameFileName);

  if (!(path & Path::Core))
  {
    statePath += "-";
    statePath += _coreName;
  }

  if (bOldFormat)
  {
    char index[8];
    sprintf(index, ".%03u", ndx);
    statePath += index;
  }

  statePath += ".state";

  if (!bOldFormat)
    statePath += std::to_string(ndx);

  return statePath;
}

static size_t alignSize(size_t size)
{
  /* align to 8-byte boundary */
  return ((size + 7) & ~7);
}

static void writeBlockHeader(uint8_t* output, const char* header, size_t size)
{
  memcpy(output, header, 4);
  output[4] = ((size) & 0xFF);
  output[5] = ((size >> 8) & 0xFF);
  output[6] = ((size >> 16) & 0xFF);
  output[7] = ((size >> 24) & 0xFF);
}

bool States::saveState(const std::string& path)
{
  _logger->info(TAG "Saving state to %s", path.c_str());

  util::ensureDirectoryExists(util::directory(path));

  /* make sure the core supports save states */
  const size_t coreSize = _core->serializeSize();
  if (coreSize == 0)
  {
    _logger->warn(TAG "Core returned 0 for save state size");
    MessageBox(g_mainWindow, "Core does not support save states", "RALibretro", MB_OK);
    return false;
  }

  /* capture the screen data so we can restore it if the state is loaded while paused */
  const void* pngData = NULL;
  int pngSize = 0;
  unsigned width, height, pitch;
  enum retro_pixel_format format;
  const void* rawFramebuffer = _video->getFramebuffer(&width, &height, &pitch, &format);
  if (rawFramebuffer != NULL)
  {
    pngData = util::toPng(_logger, rawFramebuffer, width, height, pitch, format, &pngSize);
    free((void*)rawFramebuffer);
  }

  /* determine how much space is needed for achievement data */
  const size_t rapSize = RA_CaptureState(NULL, 0);

  /* 8-byte identifier, 8-byte block header, content, 8-byte terminator */
  size_t totalSize = 8 + 8 + alignSize(coreSize) + 8;

  if (rapSize > 0)
    totalSize += 8 + alignSize(rapSize); /* 8-byte header + content */

  if (pngSize > 0)
    totalSize += 8 + alignSize(pngSize); /* 8-byte header + content */

  bool result = false;
  void* data = malloc(totalSize);
  if (data == NULL)
  {
    _logger->error(TAG "Out of memory allocating %lu bytes for the game state", totalSize);
  }
  else
  {
    uint8_t* output = (uint8_t*)data;
    memcpy(output, "RASTATE", 7);
    output[7] = RASTATE_VERSION;
    output += 8;

    writeBlockHeader(output, RASTATE_MEM_BLOCK, coreSize);
    output += 8;

    if (!_core->serialize(output, coreSize))
    {
      _logger->error(TAG "Core serialize failed");
    }
    else
    {
      output += alignSize(coreSize);

      if (rapSize > 0)
      {
        writeBlockHeader(output, RASTATE_CHEEVOS_BLOCK, rapSize);
        output += 8;

        RA_CaptureState((char*)output, rapSize);
        output += alignSize(rapSize);
      }

      if (pngSize > 0)
      {
        writeBlockHeader(output, RASTATE_SCREEN_BLOCK, pngSize);
        output += 8;

        memcpy(output, pngData, pngSize);
        output += alignSize(pngSize);
      }

      writeBlockHeader(output, RASTATE_END_BLOCK, 0);
      output += 8;

      const size_t actualSize = (output - (uint8_t*)data);
      assert(actualSize <= totalSize);

      result = util::saveFile(_logger, path.c_str(), data, actualSize);
    }

    free(data);
  }

  if (pngData)
    free((void*)pngData);

  return result;
}

void States::saveState(unsigned ndx)
{
  std::string path = getStatePath(ndx, _statePath, false);
  if (saveState(path))
  {
    path = getStatePath(ndx, _statePath, true);
    if (util::exists(path))
    {
      util::deleteFile(path);
      util::deleteFile(path + ".png");
      util::deleteFile(path + ".rap");
    }
  }
}

bool States::loadRAState1(unsigned char* input, size_t size)
{
  unsigned char* stop = input + size;
  unsigned char* marker;
  bool ret = true;

  input += 8;
  while (input < stop && ret)
  {
    size_t block_size = (input[7] << 24 | input[6] << 16 | input[5] << 8 | input[4]);
    marker = input;
    input += 8;

    if (memcmp(marker, RASTATE_MEM_BLOCK, 4) == 0)
    {
      ret = _core->unserialize(input, block_size);
    }
    else if (memcmp(marker, RASTATE_CHEEVOS_BLOCK, 4) == 0)
    {
      RA_RestoreState((const char*)input);
    }
    else if (memcmp(marker, RASTATE_SCREEN_BLOCK, 4) == 0)
    {
      unsigned width, height, pitch;
      const void* png = util::fromPng(_logger, input, block_size, &width, &height, &pitch);
      if (png)
      {
        restoreFrameBuffer(png, width, height, pitch);
        free((void*)png);
      }
    }
    else if (memcmp(marker, RASTATE_END_BLOCK, 4) == 0)
    {
      break;
    }

    input += alignSize(block_size);
  }

  return ret;
}

bool States::loadState(const std::string& path)
{
  if (!RA_WarnDisableHardcore("load a state"))
  {
    _logger->warn(TAG "Hardcore mode is active, can't load state");
    return false;
  }

  size_t size;
  void* data = util::loadFile(_logger, path.c_str(), &size);

  if (data == NULL)
  {
    return false;
  }

  if (memcmp(data, "#RZIPv", 6) == 0)
  {
    _logger->error(TAG "Compressed save states not supported");
    MessageBox(g_mainWindow, "Compressed save states not supported", "RALibretro", MB_OK);
    return false;
  }

  bool ret = true;
  if (memcmp(data, "RASTATE", 7) != 0)
  {
    /* old format is just core data, load it directly */
    ret = _core->unserialize(data, size);
    if (ret)
      RA_OnLoadState(path.c_str());
  }
  else
  {
    unsigned char* input = (unsigned char*)data;
    switch (input[7]) /* version */
    {
      case 1:
        ret = loadRAState1(input, size);
        break;

      default:
        ret = false;
        break;
    }
  }

  free(data);
  if (!ret)
  {
    _logger->error(TAG "Error loading savestate");
    return false;
  }

  unsigned image_width, image_height, pitch;
  const void* pixels = util::loadImage(_logger, path + ".png", &image_width, &image_height, &pitch);
  if (pixels == NULL)
  {
    _logger->error(TAG "Error loading savestate screenshot");
    return true; /* state was still loaded even if the frame buffer wasn't updated */
  }

  restoreFrameBuffer(pixels, image_width, image_height, pitch);
  free((void*)pixels);
  return true;
}

void States::restoreFrameBuffer(const void* pixels, unsigned image_width, unsigned image_height, unsigned pitch)
{
  unsigned width, height;
  enum retro_pixel_format format;
  _video->getFramebufferSize(&width, &height, &format);

  if (image_width != width)
  {
    /* if the widths aren't the same, the stride differs and we can't just load the pixels */
    _logger->warn(TAG "Ignoring savestate screenshot. width differs: %u != %u", image_width, width);
  }
  else
  {
    void* converted = util::fromRgb(_logger, pixels, image_width, image_height, &pitch, format);
    if (converted == NULL)
    {
      _logger->error(TAG "Error converting savestate screenshot to the framebuffer format");
    }
    else
    {
      /* prevent frame buffer corruption */
      if (height > image_height)
        height = image_height;

      _video->setFramebuffer(converted, width, height, pitch);

      free((void*)converted);
    }
  }
}

bool States::loadState(unsigned ndx)
{
  std::string path = getStatePath(ndx, _statePath, false);
  if (!util::exists(path))
    path = getStatePath(ndx, _statePath, true);

  return loadState(path);
}

bool States::existsState(unsigned ndx)
{
  std::string path = getStatePath(ndx, _statePath, false);
  if (util::exists(path))
    return true;

  path = getStatePath(ndx, _statePath, true);
  return util::exists(path);
}

const int States::_saveIntervals[] =
{
  0,
  10,
  30,
  60,
  120,
  300,
  600
};

const States::Path States::_sramPaths[] =
{
  (States::Path)(States::Path::Saves),
  (States::Path)(States::Path::Saves | States::Path::Core),
  (States::Path)(States::Path::Saves | States::Path::Core | States::Path::Game),
  (States::Path)(States::Path::Saves | States::Path::System),
  (States::Path)(States::Path::Saves | States::Path::System | States::Path::Game),
  (States::Path)(States::Path::Saves | States::Path::System | States::Path::Core),
  (States::Path)(States::Path::Saves | States::Path::System | States::Path::Core | States::Path::Game),
};

const States::Path States::_statePaths[] =
{
  (States::Path)(States::Path::Saves),
  (States::Path)(States::Path::Saves | States::Path::Core),
  (States::Path)(States::Path::Saves | States::Path::Core | States::Path::Game),
  (States::Path)(States::Path::Saves | States::Path::System),
  (States::Path)(States::Path::Saves | States::Path::System | States::Path::Game),
  (States::Path)(States::Path::Saves | States::Path::System | States::Path::Core),
  (States::Path)(States::Path::Saves | States::Path::System | States::Path::Core | States::Path::Game),
  (States::Path)(States::Path::State),
  (States::Path)(States::Path::State | States::Path::Core),
  (States::Path)(States::Path::State | States::Path::Core | States::Path::Game),
  (States::Path)(States::Path::State | States::Path::System),
  (States::Path)(States::Path::State | States::Path::System | States::Path::Game),
  (States::Path)(States::Path::State | States::Path::System | States::Path::Core),
  (States::Path)(States::Path::State | States::Path::System | States::Path::Core | States::Path::Game),
};

void States::loadSRAM(libretro::Core* core)
{
  const size_t sramSize = core->getMemorySize(RETRO_MEMORY_SAVE_RAM);
  if (sramSize != 0)
  {
    std::string sramPath = getSRamPath();
    size_t fileSize;

    void* data = util::loadFile(_logger, sramPath, &fileSize);
    if (data != NULL)
    {
      if (fileSize == sramSize)
      {
        void* memory = core->getMemoryData(RETRO_MEMORY_SAVE_RAM);
        memcpy(memory, data, sramSize);
        _logger->info(TAG "Loaded %lu bytes of Save RAM from disk", fileSize);
      }
      else
      {
        _logger->error(TAG "Save RAM size mismatch, wanted %lu, got %lu from disk", sramSize, fileSize);
      }

      if (_saveInterval > 0)
        _lastSaveData = data;
      else
        free(data);
    }
  }

  _lastSave = time(NULL);
}

void States::saveSRAM(void* sramData, size_t sramSize)
{
  std::string sramPath = getSRamPath();
  util::ensureDirectoryExists(util::directory(sramPath));
  util::saveFile(_logger, sramPath, sramData, sramSize);
}

void States::saveSRAM(libretro::Core* core)
{
  size_t sramSize = core->getMemorySize(RETRO_MEMORY_SAVE_RAM);
  if (sramSize != 0)
  {
    void* sramData = core->getMemoryData(RETRO_MEMORY_SAVE_RAM);
    saveSRAM(sramData, sramSize);
  }
}

void States::periodicSaveSRAM(libretro::Core* core)
{
  if (_saveInterval == 0)
    return;

  time_t now = time(NULL);
  if (now - _lastSave >= _saveInterval)
  {
    size_t sramSize = core->getMemorySize(RETRO_MEMORY_SAVE_RAM);
    if (sramSize != 0)
    {
      void* data = core->getMemoryData(RETRO_MEMORY_SAVE_RAM);
      if (_lastSaveData == NULL || memcmp(data, _lastSaveData, sramSize) != 0)
      {
        if (_lastSaveData == NULL)
          _lastSaveData = malloc(sramSize);

        memcpy(_lastSaveData, data, sramSize);

        // TODO: offload this to a background thread?
        saveSRAM(_lastSaveData, sramSize);
      }
    }

    _lastSave = now;
  }
}

void States::migrateFiles()
{
  Path testPath;

  // if the sram file exists, don't move anything
  std::string sramPath = getSRamPath();
  if (!util::exists(sramPath))
  {
    testPath = _sramPath;
    for (unsigned i = 0; i < sizeof(_sramPaths) / sizeof(_sramPaths[0]); ++i)
    {
      std::string path = getSRamPath(_sramPaths[i]);
      if (util::exists(path))
      {
        testPath = _sramPaths[i];
        break;
      }
    }

    if (testPath != _sramPath)
    {
      std::string oldPath = buildPath(testPath);
      std::string newPath = buildPath(_sramPath);
      const size_t rootFolderLength = strlen(_config->getRootFolder());
      oldPath.erase(0, rootFolderLength);
      newPath.erase(0, rootFolderLength);

      std::string message = "Found SRAM data in " + oldPath + ".\n\nMove to " + newPath + "?";
      if (MessageBox(g_mainWindow, message.c_str(), "RALibretro", MB_YESNO) == IDYES)
      {
        util::ensureDirectoryExists(util::directory(sramPath));

        MoveFile(getSRamPath(testPath).c_str(), sramPath.c_str());
      }
    }
  }

  for (unsigned ndx = 1; ndx <= 10; ndx++)
  {
    if (existsState(ndx))
      return;
  }

  testPath = _statePath;
  for (unsigned i = 0; i < sizeof(_statePaths) / sizeof(_statePaths[0]); ++i)
  {
    std::string statePath = buildPath(_statePaths[i]);
    if (!util::exists(statePath))
      continue;

    for (unsigned ndx = 1; ndx <= 10; ndx++)
    {
      statePath = getStatePath(ndx, _statePaths[i], false);
      if (util::exists(statePath))
      {
        testPath = _statePaths[i];
        break;
      }

      statePath = getStatePath(ndx, _statePaths[i], true);
      if (util::exists(statePath))
      {
        testPath = _statePaths[i];
        break;
      }
    }

    if (testPath != _statePath)
    {
      std::string oldPath = buildPath(testPath);
      std::string newPath = buildPath(_statePath);
      const size_t rootFolderLength = strlen(_config->getRootFolder());
      oldPath.erase(0, rootFolderLength);
      newPath.erase(0, rootFolderLength);

      std::string message = "Found save state data in " + oldPath + ".\n\nMove to " + newPath + "?";
      if (MessageBox(g_mainWindow, message.c_str(), "RALibretro", MB_YESNO) == IDYES)
      {
        util::ensureDirectoryExists(util::directory(getStatePath(1, _statePath, true)));

        for (unsigned ndx = 1; ndx <= 10; ndx++)
        {
          oldPath = getStatePath(ndx, testPath, true);
          newPath = getStatePath(ndx, _statePath, true);
          if (util::exists(oldPath))
            MoveFile(oldPath.c_str(), newPath.c_str());
        }

        for (unsigned ndx = 1; ndx <= 10; ndx++)
        {
          oldPath = getStatePath(ndx, testPath, false);
          newPath = getStatePath(ndx, _statePath, false);
          if (util::exists(oldPath))
          {
            MoveFile(oldPath.c_str(), newPath.c_str());

            std::string oldRap = oldPath + ".rap";
            std::string newRap = newPath + ".rap";
            MoveFile(oldRap.c_str(), newRap.c_str());

            std::string oldPng = oldPath + ".png";
            std::string newPng = newPath + ".png";
            MoveFile(oldPng.c_str(), newPng.c_str());
          }
        }
      }

      break;
    }
  }
}

std::string States::encodePath(Path path)
{
  std::string settings;

  settings += (path & Path::State) ? 'T' : 'S';
  if (path & Path::System)
    settings += 'Y';
  if (path & Path::Core)
    settings += 'C';
  if (path & Path::Game)
    settings += 'G';

  return settings;
}

std::string States::serializeSettings() const
{
  std::string settings = "{";

  settings += "\"saveInterval\":";
  settings += std::to_string(_saveInterval);
  settings += ",";

  settings += "\"sramPath\":\"";
  settings += encodePath(_sramPath);
  settings += "\",";

  settings += "\"statePath\":\"";
  settings += encodePath(_statePath);
  settings += "\"";

  settings += "}";
  return settings;
}

States::Path States::decodePath(const std::string& shorthand)
{
  int path = 0;
  for (auto c : shorthand)
  {
    switch (c)
    {
      case 'S': path |= (int)Path::Saves; break;
      case 'T': path |= (int)Path::State; break;
      case 'Y': path |= (int)Path::System; break;
      case 'C': path |= (int)Path::Core; break;
      case 'G': path |= (int)Path::Game; break;
      default: break;
    }
  }

  return (States::Path)path;
}

bool States::deserializeSettings(const char* json)
{
  struct Deserialize
  {
    States* self;
    std::string key;
  };
  Deserialize ud;
  ud.self = this;

  jsonsax_result_t res = jsonsax_parse((char*)json, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
  {
    auto* ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);
    }
    else if (event == JSONSAX_STRING)
    {
      if (ud->key == "sramPath")
        ud->self->_sramPath = decodePath(std::string(str, num));
      else if (ud->key == "statePath")
        ud->self->_statePath = decodePath(std::string(str, num));
    }
    else if (event == JSONSAX_NUMBER)
    {
      if (ud->key == "saveInterval")
        ud->self->_saveInterval = (int)strtoul(str, NULL, 10);
    }

    return 0;
  });

  return (res == JSONSAX_OK);
}

/* helper class to access protected data in class via static callback function */
class StatesPathAccessor : public States
{
public:
  StatesPathAccessor()
  {
    if (_pathOptions[0].empty())
    {
      for (unsigned i = 0; i < sizeof(_pathOptions) / sizeof(_pathOptions[0]); ++i)
      {
        std::string& option = _pathOptions[i];
        if (i & States::Path::State)
          option = "States";
        else
          option = "Saves";

        if (i & States::Path::System)
          option += "\\[System]";
        if (i & States::Path::Core)
          option += "\\[Core]";
        if (i & States::Path::Game)
          option += "\\[Game]";
      }

      _intervalOptions[0] = "None";
      for (unsigned i = 1; i < sizeof(_saveIntervals) / sizeof(_saveIntervals[0]); ++i)
      {
        int seconds = _saveIntervals[i];
        if (seconds == 60)
          _intervalOptions[i] = "1 minute";
        else if ((seconds % 60) == 0)
          _intervalOptions[i] = std::to_string(seconds / 60) + " minutes";
        else
          _intervalOptions[i] = std::to_string(seconds) + " seconds";
      }
    }
  }

  int getSramPathOption(int index)
  {
    if ((unsigned)index >= sizeof(_sramPaths) / sizeof(_sramPaths[0]))
      return -1;

    return _sramPaths[index];
  }

  int getStatePathOption(int index)
  {
    if ((unsigned)index >= sizeof(_statePaths) / sizeof(_statePaths[0]))
      return -1;

    return _statePaths[index];
  }

  const char* getPathOption(int option) const
  {
    return _pathOptions[option].c_str();
  }

  const char* getIntervalOption(int option) const
  {
    if (option < 0 || option >= (int)(sizeof(_saveIntervals) / sizeof(_saveIntervals[0])))
      return NULL;

    return _intervalOptions[option].c_str();
  }

private:
  static std::string _pathOptions[16];
  static std::string _intervalOptions[sizeof(_saveIntervals) / sizeof(_saveIntervals[0])];
};

std::string StatesPathAccessor::_pathOptions[];
std::string StatesPathAccessor::_intervalOptions[];

const char* s_getSramPathOptions(int index, void* udata)
{
  StatesPathAccessor accessor;
  index = accessor.getSramPathOption(index);
  if (index < 0)
    return NULL;

  return accessor.getPathOption(index);
}

const char* s_getStatePathOptions(int index, void* udata)
{
  StatesPathAccessor accessor;
  index = accessor.getStatePathOption(index);
  if (index < 0)
    return NULL;

  return accessor.getPathOption(index);
}

const char* s_getSaveIntervalOptions(int index, void* udata)
{
  StatesPathAccessor accessor;
  return accessor.getIntervalOption(index);
}

void States::showDialog()
{
  if (!_gameFileName.empty())
  {
    MessageBox(g_mainWindow, "Saving settings cannot be changed with a game loaded.", "RALibretro", MB_OK);
    return;
  }

  const WORD WIDTH = 180;
  const WORD LINE = 15;

  Dialog db;
  db.init("Saving Settings");

  WORD y = 0;

  int saveInterval = 0;
  for (unsigned i = 0; i < sizeof(_saveIntervals) / sizeof(_saveIntervals[0]); ++i)
  {
    if (_saveIntervals[i] == _saveInterval)
    {
      saveInterval = i;
      break;
    }
  }
  db.addLabel("SRAM Save Interval", 51001, 0, y, 60, 8);
  db.addCombobox(51002, 65, y - 2, WIDTH - 65, 12, 80, s_getSaveIntervalOptions, NULL, &saveInterval);
  y += LINE;

  int sramPath = 0;
  for (unsigned i = 0; i < sizeof(_sramPaths) / sizeof(_sramPaths[0]); ++i)
  {
    if (_sramPaths[i] == _sramPath)
    {
      sramPath = i;
      break;
    }
  }
  db.addLabel("SRAM Path", 51003, 0, y, 60, 8);
  db.addCombobox(51004, 65, y - 2, WIDTH - 65, 12, 140, s_getSramPathOptions, NULL, &sramPath);
  y += LINE;

  int statePath = 0;
  for (unsigned i = 0; i < sizeof(_statePaths) / sizeof(_statePaths[0]); ++i)
  {
    if (_statePaths[i] == _statePath)
    {
      statePath = i;
      break;
    }
  }
  db.addLabel("Save State Path", 51005, 0, y, 60, 8);
  db.addCombobox(51006, 65, y - 2, WIDTH - 65, 12, 140, s_getStatePathOptions, NULL, &statePath);
  y += LINE;

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  if (db.show())
  {
    _saveInterval = _saveIntervals[saveInterval];
    _sramPath = _sramPaths[sramPath];
    _statePath = _statePaths[statePath];
  }
}
