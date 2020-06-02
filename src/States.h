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

#pragma once

#include "components/Config.h"
#include "components/Logger.h"
#include "components/Video.h"

#include "libretro/Core.h"

class States
{
public:
  bool init(Logger* logger, Config* config, Video* video);

  void setGame(const std::string& gameFileName, const std::string& coreName, libretro::Core* core);

  std::string getSRamPath();
  std::string getStatePath(unsigned ndx);

  void        saveState(const std::string& path);
  void        saveState(unsigned ndx);
  bool        loadState(const std::string& path);
  bool        loadState(unsigned ndx);

  bool        existsState(unsigned ndx);

protected:
  Logger* _logger;
  Config* _config;
  Video* _video;

  std::string _gameFileName;
  std::string _coreName;
  libretro::Core* _core;
};
