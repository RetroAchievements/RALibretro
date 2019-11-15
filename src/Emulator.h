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

#include <set>
#include <string>

#include <RA_Interface.h>

enum class System
{
  kNone           = UnknownConsoleID,
  kAtari2600      = Atari2600,
  kAtariLynx      = Lynx,
  kAtariJaguar    = Jaguar,
  kMasterSystem   = MasterSystem,
  kMegaDrive      = MegaDrive,
  kSegaCD         = SegaCD,
  kSega32X        = Sega32X,
  kSaturn         = Saturn,
  kNintendo       = NES,
  kPCEngine       = PCEngine,
  kSuperNintendo  = SNES,
  kGameBoy        = GB,
  kGameBoyColor   = GBC,
  kGameBoyAdvance = GBA,
  kNintendo64     = N64,
  kPlayStation1   = PlayStation,
  kNeoGeoPocket   = NeoGeoPocket,
  kVirtualBoy     = VirtualBoy,
  kGameGear       = GameGear,
  kArcade         = Arcade,
  kAtari7800      = Atari7800,
  kPokemonMini    = PokemonMini,
  kColecovision   = Colecovision,
  kSG1000         = SG1000,
  kWonderSwan     = WonderSwan
};

const std::string& getEmulatorName(const std::string& coreName, System system);
const char* getEmulatorExtensions(const std::string& coreName, System system);
const char* getSystemName(System system);

class Config;
class Logger;
bool   loadCores(Config* config, Logger* logger);

void   getAvailableSystems(std::set<System>& systems);
void   getAvailableSystemCores(System system, std::set<std::string>& coreNames);
int    encodeCoreName(const std::string& coreName, System system);
const std::string& getCoreName(int encoded, System& system);
const std::string* getCoreDeprecationMessage(const std::string& coreName);

bool   showCoresDialog(Config* config, Logger* logger);
