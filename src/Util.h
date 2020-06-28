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

#include "components/Logger.h"

#include <stddef.h>
#include <string>

// _WINDOWS says we're building _for_ Windows
#ifdef _WINDOWS
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#endif

// _WIN32 says we're building _on_ Windows
#ifdef _WIN32
 #define strcasecmp _stricmp
 #define strncasecmp _strnicmp
#else
 #define sprintf_s sprintf
#endif

namespace util
{
  time_t      fileTime(const std::string& path);
  bool        exists(const std::string& path);

  FILE*       openFile(Logger* logger, const std::string& path, const char* mode);
  std::string loadFile(Logger* logger, const std::string& path);
  void*       loadFile(Logger* logger, const std::string& path, size_t* size);

#ifndef NO_MINIZ
  void*       loadZippedFile(Logger* logger, const std::string& path, size_t* size, std::string& unzippedFileName);
  bool        unzipFile(Logger* logger, const std::string& zipPath, const std::string& archiveFileName, const std::string& unzippedPath);
#endif

  bool        saveFile(Logger* logger, const std::string& path, const void* data, size_t size);
  void        deleteFile(const std::string& path);

#ifndef _CONSOLE
  bool        downloadFile(Logger* logger, const std::string& url, const std::string& path);
#endif

  std::string jsonEscape(const std::string& str);
  std::string jsonUnescape(const std::string& str);

  std::string fullPath(const std::string& path);
  std::string fileName(const std::string& path);
  std::string fileNameWithExtension(const std::string& path);
  std::string extension(const std::string& path);
  std::string replaceFileName(const std::string& originalPath, const char* newFileName);
  std::string sanitizeFileName(const std::string& fileName);

  std::string directory(const std::string& path);

#ifdef _WINDOWS
  void        ensureDirectoryExists(const std::string& path);
#endif

#ifdef _WINDOWS
  std::string openFileDialog(HWND hWnd, const std::string& extensionsFilter);
  std::string saveFileDialog(HWND hWnd, const std::string& extensionsFilter);
#endif

  const void* toRgb(Logger* logger, const void* data, unsigned width, unsigned height, unsigned pitch, enum retro_pixel_format format);
  void        saveImage(Logger* logger, const std::string& path, const void* data, unsigned width, unsigned height, unsigned pitch, enum retro_pixel_format format);
  void*       fromRgb(Logger* logger, const void* data, unsigned width, unsigned height, unsigned* pitch, enum retro_pixel_format format);
  void*       loadImage(Logger* logger, const std::string& path, unsigned* width, unsigned* height, unsigned* pitch);

#ifdef _WINDOWS
  std::string ucharToUtf8(const std::wstring& unicodeString);
  std::wstring utf8ToUChar(const std::string& utf8String);
#endif
}
