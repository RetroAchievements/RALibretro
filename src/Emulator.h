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

const std::string& getEmulatorName(const std::string& coreName, int system);
const char* getEmulatorExtensions(const std::string& coreName, int system);
const char* getSystemName(int system);

class Config;
class Logger;
bool   loadCores(Config* config, Logger* logger);

void   getAvailableSystems(std::set<int>& systems);
void   getAvailableSystemCores(int system, std::set<std::string>& coreNames);
int    encodeCoreName(const std::string& coreName, int system);
const std::string& getCoreName(int encoded, int& systemOut);
const std::string* getCoreDeprecationMessage(const std::string& coreName);

bool   showCoresDialog(Config* config, Logger* logger, const std::string& loadedCoreName);
