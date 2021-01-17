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

#include "libretro/Core.h"

#include "Emulator.h"

struct rc_memory_regions_t;

class Memory
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();

  void attachToCore(libretro::Core* core, int consoleId);

protected:
  void registerMemoryRegion(int type, uint8_t* data, size_t size, const char* description);
  void initializeWithoutRegions(libretro::Core* core);
  void initializeFromMemoryMap(const rc_memory_regions_t* regions, const retro_memory_map* mmap);
  void initializeFromUnmappedMemory(const rc_memory_regions_t* regions, libretro::Core* core);
  void installMemoryBanks();

  libretro::LoggerComponent* _logger;
};
