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

#include "components/Config.h"
#include "components/Logger.h"
#include "components/Video.h"

#include "libretro/Core.h"

class States
{
public:
  bool init(Logger* logger, Config* config, Video* video);

  void setGame(const std::string& gameFileName, int system, const std::string& coreName, libretro::Core* core);

  std::string getSRamPath() const;
  std::string getStatePath(unsigned ndx) const;

  bool        saveState(const std::string& path);
  void        saveState(unsigned ndx);
  bool        loadState(const std::string& path);
  bool        loadState(unsigned ndx);

  void        loadSRAM(libretro::Core* core);
  void        saveSRAM(libretro::Core* core);
  void        periodicSaveSRAM(libretro::Core* core);

  void        migrateFiles();
  bool        existsState(unsigned ndx);

  std::string serializeSettings() const;
  bool        deserializeSettings(const char* json);

  void        showDialog();

protected:
  enum Path
  {
    Saves = 0x00,
    State = 0x01,
    System = 0x02,
    Core = 0x04,
    Game = 0x08
  };
  Path _sramPath = Path::Saves;
  Path _statePath = Path::Saves;

  static const States::Path _sramPaths[];
  static const States::Path _statePaths[];
  static const int _saveIntervals[];

  Logger* _logger;
  Config* _config;
  Video* _video;

  std::string _gameFileName;
  int _system = 0;
  std::string _coreName;
  libretro::Core* _core = NULL;
  int _saveInterval = 0;
  void* _lastSaveData = NULL;
  time_t _lastSave = 0;

private:
  std::string buildPath(Path path) const;
  static std::string encodePath(Path path);
  static Path decodePath(const std::string& shorthand);

  std::string getSRamPath(Path path) const;
  std::string getStatePath(unsigned ndx, Path path, bool bOldFormat) const;

  void saveSRAM(void* sramData, size_t sramSize);
  void restoreFrameBuffer(const void* pixels, unsigned image_width, unsigned image_height, unsigned pitch);

  bool loadRAState1(unsigned char* input, size_t size);
};
