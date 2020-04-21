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

#include <rconsoles.h>

enum class System
{
  kNone           = 0,
  kAtari2600      = RC_CONSOLE_ATARI_2600,
  kAtariLynx      = RC_CONSOLE_ATARI_LYNX,
  kAtariJaguar    = RC_CONSOLE_ATARI_JAGUAR,
  kMasterSystem   = RC_CONSOLE_MASTER_SYSTEM,
  kMegaDrive      = RC_CONSOLE_MEGA_DRIVE,
  kSegaCD         = RC_CONSOLE_SEGA_CD,
  kSega32X        = RC_CONSOLE_SEGA_32X,
  kSaturn         = RC_CONSOLE_SATURN,
  kNintendo       = RC_CONSOLE_NINTENDO,
  kNintendoDS     = RC_CONSOLE_NINTENDO_DS,
  kPCEngine       = RC_CONSOLE_PC_ENGINE,
  kSuperNintendo  = RC_CONSOLE_SUPER_NINTENDO,
  kGameBoy        = RC_CONSOLE_GAMEBOY,
  kGameBoyColor   = RC_CONSOLE_GAMEBOY_COLOR,
  kGameBoyAdvance = RC_CONSOLE_GAMEBOY_ADVANCE,
  kNintendo64     = RC_CONSOLE_NINTENDO_64,
  kPlayStation1   = RC_CONSOLE_PLAYSTATION,
  kNeoGeoPocket   = RC_CONSOLE_NEOGEO_POCKET,
  kVirtualBoy     = RC_CONSOLE_VIRTUAL_BOY,
  kGameGear       = RC_CONSOLE_GAME_GEAR,
  kArcade         = RC_CONSOLE_ARCADE,
  kAtari7800      = RC_CONSOLE_ATARI_7800,
  kPokemonMini    = RC_CONSOLE_POKEMON_MINI,
  kColecovision   = RC_CONSOLE_COLECOVISION,
  kSG1000         = RC_CONSOLE_SG1000,
  kWonderSwan     = RC_CONSOLE_WONDERSWAN
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
