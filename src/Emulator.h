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

#include "libretro/Core.h"

#include <RA_Interface.h>

#include "Util.h"

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
  kMednafenNgp,
  kMednafenVb,
  kFBAlpha,
  kProSystem
};

enum class System
{
  kNone           = UnknownConsoleID,
  kAtari2600      = Atari2600,
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
  kNeoGeoPocket   = NeoGeoPocket,
  kVirtualBoy     = VirtualBoy,
  kGameGear       = GameGear,
  kArcade         = Arcade,
  kAtari7800      = Atari7800
};

const char* getEmulatorName(Emulator emulator);
const char* getEmulatorFileName(Emulator emulator);
const char* getEmulatorExtensions(Emulator emulator);
const char* getSystemName(System system);

System getSystem(Emulator emulator, const std::string game_path, libretro::Core* core);
bool   romLoaded(Logger* logger, System system, const std::string& path, void* rom, size_t size);
