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

#include "components/Logger.h"

#include <stddef.h>
#include <string>

size_t      nextPow2(size_t v);
void*       loadFile(Logger* logger, const std::string& path, size_t* size);
bool        saveFile(Logger* logger, const std::string& path, const void* data, size_t size);
std::string jsonEscape(const std::string& str);
std::string jsonUnescape(const std::string& str);
