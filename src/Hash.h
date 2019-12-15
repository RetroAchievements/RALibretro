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

#ifndef _WINDOWS
// prevent interface from attempting to define DLL functions that require Windows APIs
#define RA_EXPORTS 

// explicitly define APIs needed by hasher
typedef unsigned char BYTE;
unsigned int RA_IdentifyHash(const char* sHash);
void RA_ActivateGame(unsigned int nGameId);
void RA_OnLoadNewRom(BYTE* pROMData, unsigned int nROMSize);

#endif

#include "Emulator.h"

bool   romLoaded(Logger* logger, System system, const std::string& path, void* rom, size_t size);
void   romUnloaded(Logger* logger);
