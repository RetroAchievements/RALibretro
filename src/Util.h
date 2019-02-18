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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace util
{
  void*       loadFile(Logger* logger, const std::string& path, size_t* size);
  void*       loadZippedFile(Logger* logger, const std::string& path, size_t* size, std::string& unzippedFileName);
  bool        saveFile(Logger* logger, const std::string& path, const void* data, size_t size);
  std::string jsonEscape(const std::string& str);
  std::string jsonUnescape(const std::string& str);
  std::string fileName(const std::string& path);
  std::string fileNameWithExtension(const std::string& path);
  std::string extension(const std::string& path);
  std::string openFileDialog(HWND hWnd, const char* extensionsFilter);
  std::string saveFileDialog(HWND hWnd, const char* extensionsFilter);
  const void* toRgb(Logger* logger, const void* data, unsigned width, unsigned height, unsigned pitch, enum retro_pixel_format format);
  void        saveImage(Logger* logger, const std::string& path, const void* data, unsigned width, unsigned height, unsigned pitch, enum retro_pixel_format format);
  const void* fromRgb(Logger* logger, const void* data, unsigned width, unsigned height, unsigned* pitch, enum retro_pixel_format format);
  const void* loadImage(Logger* logger, const std::string& path, unsigned* width, unsigned* height, unsigned* pitch);
}
