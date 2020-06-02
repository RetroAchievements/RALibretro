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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#include <commdlg.h>
#include <shlobj.h>

#define TAG "[SAV] "

bool States::init(Logger* logger, Config* config, Video* video)
{
  _logger = logger;
  _config = config;
  _video = video;
  _core = NULL;

  return true;
}

void States::setGame(const std::string& gameFileName, const std::string& coreName, libretro::Core* core)
{
  _gameFileName = gameFileName;
  _coreName = coreName;
  _core = core;
}

std::string States::getSRamPath()
{
  std::string path = _config->getSaveDirectory();
  path += _gameFileName;
  path += ".sram";
  return path;
}

std::string States::getStatePath(unsigned ndx)
{
  std::string path = _config->getSaveDirectory();
  path += _gameFileName;

  path += "-";
  path += _coreName;
  
  char index[16];
  sprintf(index, ".%03u", ndx);

  path += index;
  path += ".state";

  return path;
}

void States::saveState(const std::string& path)
{
  _logger->info(TAG "Saving state to %s", path.c_str());
  
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
  struct stat statbuf;

  return (stat(path.c_str(), &statbuf) == 0);
}
