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
along with RALibRetro.  If not, see <http://www.gnu.org/licenses/>.
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

#define TAG "[SAV] "

extern HWND g_mainWindow;

bool States::init(Logger* logger, Config* config, Video* video)
{
  _logger = logger;
  _config = config;
  _video = video;
  _core = NULL;

  return true;
}

void States::setGame(const std::string& gameFileName, int system, const std::string& coreName, libretro::Core* core)
{
  _gameFileName = gameFileName;
  _system = system;
  _coreName = coreName;
  _core = core;

  _config->setSaveDirectory(buildPath(_sramPath));
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
    savePath += rc_console_name(_system);
    savePath += '\\';
  }

  if ((path & Path::Core) && _core->getSystemInfo()->library_name)
  {
    savePath += _core->getSystemInfo()->library_name;
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
  return getStatePath(ndx, _statePath);
}

std::string States::getStatePath(unsigned ndx, Path path) const
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
  
  char index[8];
  sprintf(index, ".%03u", ndx);

  statePath += index;
  statePath += ".state";

  return statePath;
}

void States::saveState(const std::string& path)
{
  _logger->info(TAG "Saving state to %s", path.c_str());

  util::ensureDirectoryExists(util::directory(path));

  size_t size = _core->serializeSize();
  void* data = malloc(size);

  if (data == NULL)
  {
    _logger->error(TAG "Out of memory allocating %lu bytes for the game state", size);
    return;
  }

  if (!_core->serialize(data, size))
  {
    free(data);
    return;
  }

  unsigned width, height, pitch;
  enum retro_pixel_format format;
  const void* pixels = _video->getFramebuffer(&width, &height, &pitch, &format);

  if (pixels == NULL)
  {
    free(data);
    return;
  }

  if (!util::saveFile(_logger, path.c_str(), data, size))
  {
    free((void*)pixels);
    free(data);
    return;
  }

  util::saveImage(_logger, path + ".png", pixels, width, height, pitch, format);
  RA_OnSaveState(path.c_str());

  free((void*)pixels);
  free(data);
}

void States::saveState(unsigned ndx)
{
  saveState(getStatePath(ndx));
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
    MessageBox(g_mainWindow, "Compressed save states not supported", "RALibRetro", MB_OK);
    return false;
  }

  _core->unserialize(data, size);
  free(data);
  RA_OnLoadState(path.c_str());

  unsigned width, height, pitch;
  enum retro_pixel_format format;
  _video->getFramebufferSize(&width, &height, &format);

  unsigned image_width, image_height;
  const void* pixels = util::loadImage(_logger, path + ".png", &image_width, &image_height, &pitch);
  if (pixels == NULL)
  {
    _logger->error(TAG "Error loading savestate screenshot");
    return true; /* state was still loaded even if the frame buffer wasn't updated */
  }

  /* if the widths aren't the same, the stride differs and we can't just load the pixels */
  if (image_width == width)
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

  free((void*)pixels);
  return true;
}

bool States::loadState(unsigned ndx)
{
  return loadState(getStatePath(ndx));
}

bool States::existsState(unsigned ndx)
{
  std::string path = getStatePath(ndx);
  return util::exists(path);
}

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
        testPath = (Path)i;
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
      if (MessageBox(g_mainWindow, message.c_str(), "RALibRetro", MB_YESNO) == IDYES)
      {
        util::ensureDirectoryExists(util::directory(sramPath));

        MoveFile(getSRamPath(testPath).c_str(), sramPath.c_str());
      }
    }
  }

  for (unsigned ndx = 1; ndx <= 10; ndx++)
  {
    std::string statePath = getStatePath(ndx, _statePath);
    if (util::exists(statePath))
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
      statePath = getStatePath(ndx, _statePaths[i]);
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
      if (MessageBox(g_mainWindow, message.c_str(), "RALibRetro", MB_YESNO) == IDYES)
      {
        util::ensureDirectoryExists(util::directory(getStatePath(1, _statePath)));

        for (unsigned ndx = 1; ndx <= 10; ndx++)
        {
          oldPath = getStatePath(ndx, testPath);
          newPath = getStatePath(ndx, _statePath);
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

    return 0;
  });

  return (res == JSONSAX_OK);
}

/* helper class to access protected data in class via static callback function */
class StatesPathAccessor : public States
{
public:
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
};

static std::string s_pathOptions[16];

const char* s_getSramPathOptions(int index, void* udata)
{
  StatesPathAccessor accessor;
  index = accessor.getSramPathOption(index);
  if (index < 0)
    return NULL;

  return s_pathOptions[index].c_str();
}

const char* s_getStatePathOptions(int index, void* udata)
{
  StatesPathAccessor accessor;
  index = accessor.getStatePathOption(index);
  if (index < 0)
    return NULL;

  return s_pathOptions[index].c_str();
}

void States::showDialog()
{
  if (!_gameFileName.empty())
  {
    MessageBox(g_mainWindow, "Saving settings cannot be changed with a game loaded.", "RALibRetro", MB_OK);
    return;
  }

  const WORD WIDTH = 180;
  const WORD LINE = 15;

  Dialog db;
  db.init("Saving Settings");

  if (s_pathOptions[0].empty())
  {
    for (unsigned i = 0; i < sizeof(s_pathOptions) / sizeof(s_pathOptions[0]); ++i)
    {
      std::string& option = s_pathOptions[i];
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
  }

  WORD y = 0;

  int sramPath = 0;
  for (unsigned i = 0; i < sizeof(_sramPaths) / sizeof(_sramPaths[0]); ++i)
  {
    if (_sramPaths[i] == _sramPath)
    {
      sramPath = i;
      break;
    }
  }
  db.addLabel("SRAM Path", 51001, 0, y, 50, 8);
  db.addCombobox(51002, 55, y - 2, WIDTH - 55, 12, 140, s_getSramPathOptions, NULL, &sramPath);
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
  db.addLabel("Save State Path", 51003, 0, y, 50, 8);
  db.addCombobox(51004, 55, y - 2, WIDTH - 55, 12, 140, s_getStatePathOptions, NULL, &statePath);
  y += LINE;

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  if (db.show())
  {
    _sramPath = _sramPaths[sramPath];
    _statePath = _statePaths[statePath];
  }
}
