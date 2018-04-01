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

#include "Util.h"

#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <commdlg.h>
#include <shlobj.h>

size_t util::nextPow2(size_t v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return v + 1;
}

void* util::loadFile(Logger* logger, const std::string& path, size_t* size)
{
  void* data;
  struct stat statbuf;

  if (stat(path.c_str(), &statbuf) != 0)
  {
    logger->printf(RETRO_LOG_ERROR, "Error getting info from \"%s\": %s", path.c_str(), strerror(errno));
    return NULL;
  }

  *size = statbuf.st_size;
  data = malloc(*size + 1);

  if (data == NULL)
  {
    logger->printf(RETRO_LOG_ERROR, "Out of memory allocating %lu bytes to load \"%s\"", *size, path.c_str());
    return NULL;
  }

  FILE* file = fopen(path.c_str(), "rb");

  if (file == NULL)
  {
    logger->printf(RETRO_LOG_ERROR, "Error opening \"%s\": %s", path.c_str(), strerror(errno));
    free(data);
    return NULL;
  }

  size_t numread = fread(data, 1, *size, file);

  if (numread < 0 || numread != *size)
  {
    logger->printf(RETRO_LOG_ERROR, "Error reading \"%s\": %s", path.c_str(), strerror(errno));
    fclose(file);
    free(data);
    return NULL;
  }

  fclose(file);
  *((uint8_t*)data + *size) = 0;
  logger->printf(RETRO_LOG_INFO, "Read %zu bytes from \"%s\"", *size, path.c_str());
  return data;
}

bool util::saveFile(Logger* logger, const std::string& path, const void* data, size_t size)
{
  FILE* file = fopen(path.c_str(), "wb");

  if (file == NULL)
  {
    logger->printf(RETRO_LOG_ERROR, "Error opening file \"%s\": %s", path.c_str(), strerror(errno));
    return false;
  }

  if (fwrite(data, 1, size, file) != size)
  {
    logger->printf(RETRO_LOG_ERROR, "Error writing file \"%s\": %s", path.c_str(), strerror(errno));
    fclose(file);
    return false;
  }

  fclose(file);
  logger->printf(RETRO_LOG_INFO, "Wrote %zu bytes to \"%s\"", size, path.c_str());
  return true;
}

std::string util::jsonEscape(const std::string& str)
{
  std::string res;
  res.reserve(str.length());

  for (size_t i = 0; i < str.length(); i++)
  {
    switch (str[i])
    {
    case '"':  res += "\\\""; break;
    case '\\': res += "\\\\"; break;
    case '/':  res += "\\/"; break;
    case '\b': res += "\\b"; break;
    case '\f': res += "\\f"; break;
    case '\n': res += "\\n"; break;
    case '\r': res += "\\r"; break;
    case '\t': res += "\\t"; break;
    default:   res += str[i];
    }
  }

  return res;
}

std::string util::jsonUnescape(const std::string& str)
{
  std::string res;
  res.reserve(str.length());

  for (size_t i = 0; i < str.length(); i++)
  {
    if (str[i] == '\\' && (i + 1) < str.length())
    {
      switch (str[++i])
      {
      case '"':  res += "\""; break;
      case '\\': res += "\\"; break;
      case '/':  res += "/"; break;
      case 'b':  res += "\b"; break;
      case 'f':  res += "\f"; break;
      case 'n':  res += "\n"; break;
      case 'r':  res += "\r"; break;
      case 't':  res += "\t"; break;
      }
    }
    else
    {
      res += str[i];
    }
  }

  return res;
}

std::string util::fileName(const std::string& path)
{
  const char* str = path.c_str();
  const char* name = strrchr(str, '/');
  const char* bs = strrchr(str, '\\');

  if (name == NULL)
  {
    name = str - 1;
  }

  if (bs == NULL)
  {
    bs = str - 1;
  }

  if (bs > name)
  {
    name = bs;
  }

  name++;

  const char* dot = strrchr(name, '.');

  if (dot == NULL)
  {
    return std::string(name);
  }
  else
  {
    return std::string(name, dot - name);
  }
}

std::string util::extension(const std::string& path)
{
  const char* str = path.c_str();
  const char* dot = strrchr(str, '.');

  if (dot == NULL)
  {
    return "";
  }
  else
  {
    return path.substr(dot - str);
  }
}

std::string util::openFileDialog(HWND hWnd, const char* extensionsFilter)
{
  char path[_MAX_PATH];
  path[0] = 0;

  OPENFILENAME cfg;

  cfg.lStructSize = sizeof(cfg);
  cfg.hwndOwner = hWnd;
  cfg.hInstance = NULL;
  cfg.lpstrFilter = extensionsFilter;
  cfg.lpstrCustomFilter = NULL;
  cfg.nMaxCustFilter = 0;
  cfg.nFilterIndex = 2;
  cfg.lpstrFile = path;
  cfg.nMaxFile = sizeof(path);
  cfg.lpstrFileTitle = NULL;
  cfg.nMaxFileTitle = 0;
  cfg.lpstrInitialDir = NULL;
  cfg.lpstrTitle = "Load";
  cfg.Flags = OFN_FILEMUSTEXIST;
  cfg.nFileOffset = 0;
  cfg.nFileExtension = 0;
  cfg.lpstrDefExt = NULL;
  cfg.lCustData = 0;
  cfg.lpfnHook = NULL;
  cfg.lpTemplateName = NULL;

  if (GetOpenFileName(&cfg) == TRUE)
  {
    return path;
  }
  else
  {
    return "";
  }
}

std::string util::saveFileDialog(HWND hWnd, const char* extensionsFilter)
{
  char path[_MAX_PATH];
  path[0] = 0;

  OPENFILENAME cfg;

  cfg.lStructSize = sizeof(cfg);
  cfg.hwndOwner = hWnd;
  cfg.hInstance = NULL;
  cfg.lpstrFilter = extensionsFilter;
  cfg.lpstrCustomFilter = NULL;
  cfg.nMaxCustFilter = 0;
  cfg.nFilterIndex = 2;
  cfg.lpstrFile = path;
  cfg.nMaxFile = sizeof(path);
  cfg.lpstrFileTitle = NULL;
  cfg.nMaxFileTitle = 0;
  cfg.lpstrInitialDir = NULL;
  cfg.lpstrTitle = "Save";
  cfg.Flags = OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT;
  cfg.nFileOffset = 0;
  cfg.nFileExtension = 0;
  cfg.lpstrDefExt = NULL;
  cfg.lCustData = 0;
  cfg.lpfnHook = NULL;
  cfg.lpTemplateName = NULL;

  if (GetSaveFileName(&cfg) == TRUE)
  {
    return path;
  }
  else
  {
    return "";
  }
}
