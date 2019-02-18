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

#include <miniz_zip.h>

#define WIN32_LEAN_AND_MEAN
#include <commdlg.h>
#include <shlobj.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x)
#include "stb_image_write.h"

#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TAG "[UTL] "

void* util::loadFile(Logger* logger, const std::string& path, size_t* size)
{
  void* data;
  struct stat statbuf;

  if (stat(path.c_str(), &statbuf) != 0)
  {
    logger->error(TAG "Error getting info from \"%s\": %s", path.c_str(), strerror(errno));
    return NULL;
  }

  *size = statbuf.st_size;
  data = malloc(*size + 1);

  if (data == NULL)
  {
    logger->error(TAG "Out of memory allocating %lu bytes to load \"%s\"", *size, path.c_str());
    return NULL;
  }

  FILE* file = fopen(path.c_str(), "rb");

  if (file == NULL)
  {
    logger->error(TAG "Error opening \"%s\": %s", path.c_str(), strerror(errno));
    free(data);
    return NULL;
  }

  size_t numread = fread(data, 1, *size, file);

  if (numread < 0 || numread != *size)
  {
    logger->error(TAG "Error reading \"%s\": %s", path.c_str(), strerror(errno));
    fclose(file);
    free(data);
    return NULL;
  }

  fclose(file);
  *((uint8_t*)data + *size) = 0;
  logger->info(TAG "Read %zu bytes from \"%s\"", *size, path.c_str());
  return data;
}

void* util::loadZippedFile(Logger* logger, const std::string& path, size_t* size, std::string& unzippedFileName)
{
  mz_bool status;
  mz_zip_archive zip_archive;
  mz_zip_archive_file_stat file_stat;
  void* data;
  int file_count;
  int i;

  memset(&zip_archive, 0, sizeof(zip_archive));

  status = mz_zip_reader_init_file(&zip_archive, path.c_str(), 0);
  if (!status)
  {
    logger->error(TAG "Error opening \"%s\": %s", path.c_str(), strerror(errno));
    return NULL;
  }

  file_count = mz_zip_reader_get_num_files(&zip_archive);
  if (file_count == 0)
  {
    mz_zip_reader_end(&zip_archive);
    logger->error(TAG "Empty zip file \"%s\"", path.c_str());
    return NULL;
  }

  if (file_count > 1)
  {
    mz_zip_reader_end(&zip_archive);
    logger->error(TAG "Zip file \"%s\" contains %d files, determining which to open is not supported - returning entire zip file", path.c_str(), file_count);
    return loadFile(logger, path, size);
  }

  if (mz_zip_reader_is_file_a_directory(&zip_archive, 0))
  {
    mz_zip_reader_end(&zip_archive);
    logger->error(TAG "Zip file \"%s\" only contains a directory", path.c_str());
    return NULL;
  }

  if (!mz_zip_reader_file_stat(&zip_archive, 0, &file_stat))
  {
    mz_zip_reader_end(&zip_archive);
    logger->error(TAG "Error opening file in \"%s\"", path.c_str());
    return NULL;
  }

  *size = file_stat.m_uncomp_size;
  data = malloc(*size);

  status = mz_zip_reader_extract_to_mem(&zip_archive, 0, data, *size, 0);
  if (!status)
  {
    mz_zip_reader_end(&zip_archive);
    logger->error(TAG "Error decompressing file in \"%s\": %s", path.c_str(), strerror(errno));
    free(data);
    return NULL;
  }

  unzippedFileName = file_stat.m_filename;
  logger->error(TAG "Read %zu bytes from \"%s\":\"%s\"", *size, path.c_str(), file_stat.m_filename);
  mz_zip_reader_end(&zip_archive);
  return data;
}

bool util::saveFile(Logger* logger, const std::string& path, const void* data, size_t size)
{
  FILE* file = fopen(path.c_str(), "wb");

  if (file == NULL)
  {
    logger->error(TAG "Error opening file \"%s\": %s", path.c_str(), strerror(errno));
    return false;
  }

  if (fwrite(data, 1, size, file) != size)
  {
    logger->error(TAG "Error writing file \"%s\": %s", path.c_str(), strerror(errno));
    fclose(file);
    return false;
  }

  fclose(file);
  logger->info(TAG "Wrote %zu bytes to \"%s\"", size, path.c_str());
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

std::string util::fileNameWithExtension(const std::string& path)
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
  return std::string(name);
}

std::string util::fileName(const std::string& path)
{
  std::string filename = fileNameWithExtension(path);
  int ndx = filename.find_last_of('.');
  if (ndx != std::string::npos);
    filename.resize(ndx);

  return filename;
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

const void* util::toRgb(Logger* logger, const void* data, unsigned width, unsigned height, unsigned pitch, enum retro_pixel_format format)
{
  void* pixels = malloc(width * height * 3);

  if (pixels == NULL)
  {
    logger->error(TAG "Error allocating memory for the screenshot");
    return NULL;
  }

  if (format == RETRO_PIXEL_FORMAT_RGB565)
  {
    logger->info(TAG "Pixel format is RGB565, converting to 24-bits RGB");

    uint16_t* source_rgba5650 = (uint16_t*)data;
    uint8_t* target_rgba8880 = (uint8_t*)pixels;

    for (unsigned y = 0; y < height; y++)
    {
      uint8_t* row = (uint8_t*)source_rgba5650;

      for (unsigned x = 0; x < width; x++)
      {
        uint16_t rgba5650 = *source_rgba5650++;

        *target_rgba8880++ = (rgba5650 >> 11) * 255 / 31;
        *target_rgba8880++ = ((rgba5650 >> 5) & 0x3f) * 255 / 63;
        *target_rgba8880++ = (rgba5650 & 0x1f) * 255 / 31;
      }

      source_rgba5650 = (uint16_t*)(row + pitch);
    }
  }
  else if (format == RETRO_PIXEL_FORMAT_0RGB1555)
  {
    logger->info(TAG "Pixel format is 0RGB1565, converting to 24-bits RGB");

    uint16_t* source_argb1555 = (uint16_t*)data;
    uint8_t* target_rgba8880 = (uint8_t*)pixels;

    for (unsigned y = 0; y < height; y++)
    {
      uint8_t* row = (uint8_t*)source_argb1555;

      for (unsigned x = 0; x < width; x++)
      {
        uint16_t argb1555 = *source_argb1555++;

        *target_rgba8880++ = (argb1555 >> 10) * 255 / 31;
        *target_rgba8880++ = ((argb1555 >> 5) & 0x1f) * 255 / 31;
        *target_rgba8880++ = (argb1555 & 0x1f) * 255 / 31;
      }

      source_argb1555 = (uint16_t*)(row + pitch);
    }
  }
  else if (format == RETRO_PIXEL_FORMAT_XRGB8888)
  {
    logger->info(TAG "Pixel format is XRGB8888, converting to 24-bits RGB");

    uint32_t* source_argb8888 = (uint32_t*)data;
    uint8_t* target_rgba8880 = (uint8_t*)pixels;

    for (unsigned y = 0; y < height; y++)
    {
      uint8_t* row = (uint8_t*)source_argb8888;

      for (unsigned x = 0; x < width; x++)
      {
        uint32_t argb8888 = *source_argb8888++;

        *target_rgba8880++ = argb8888 >> 16;
        *target_rgba8880++ = argb8888 >> 8;
        *target_rgba8880++ = argb8888;
      }

      source_argb8888 = (uint32_t*)(row + pitch);
    }
  }
  else
  {
    logger->error(TAG "Unknown pixel format");
    free(pixels);
    return NULL;
  }

  return pixels;
}

void util::saveImage(Logger* logger, const std::string& path, const void* data, unsigned width, unsigned height, unsigned pitch, enum retro_pixel_format format)
{
  const void* pixels = util::toRgb(logger, data, width, height, pitch, format);

  if (pixels == NULL)
  {
    return;
  }

  stbi_write_png(path.c_str(), width, height, 3, pixels, 0);
  free((void*)pixels);

  logger->info(TAG "Wrote image %u x %u to %s", width, height, path.c_str());
}

const void* util::fromRgb(Logger* logger, const void* data, unsigned width, unsigned height, unsigned* pitch, enum retro_pixel_format format)
{
  void* pixels;

  if (format == RETRO_PIXEL_FORMAT_RGB565)
  {
    logger->info(TAG "Converting from 24-bits RGB to RGB565");

    pixels = malloc(width * height * 2);

    if (pixels == NULL)
    {
      logger->error(TAG "Error allocating memory for the screenshot");
      return NULL;
    }

    uint8_t* source_rgb888 = (uint8_t*)data;
    uint16_t* target_rgba5650 = (uint16_t*)pixels;

    for (unsigned y = 0; y < height; y++)
    {
      uint8_t* row = source_rgb888;

      for (unsigned x = 0; x < width; x++)
      {
        uint8_t r = *source_rgb888++ >> 3;
        uint8_t g = *source_rgb888++ >> 2;
        uint8_t b = *source_rgb888++ >> 3;

        *target_rgba5650++ = r << 11 | g << 5 | b;
      }

      source_rgb888 = row + *pitch;
    }

    *pitch = width * 2;
  }
  else if (format == RETRO_PIXEL_FORMAT_0RGB1555)
  {
    logger->info(TAG "Converting from 24-bits RGB to 0RGB1565");

    pixels = malloc(width * height * 2);

    if (pixels == NULL)
    {
      logger->error(TAG "Error allocating memory for the screenshot");
      return NULL;
    }

    uint8_t* source_rgb888 = (uint8_t*)data;
    uint16_t* target_argb1555 = (uint16_t*)pixels;

    for (unsigned y = 0; y < height; y++)
    {
      uint8_t* row = source_rgb888;

      for (unsigned x = 0; x < width; x++)
      {
        uint8_t r = *source_rgb888++ >> 3;
        uint8_t g = *source_rgb888++ >> 3;
        uint8_t b = *source_rgb888++ >> 3;

        *target_argb1555++ = r << 10 | g << 5 | b;
      }

      source_rgb888 = row + *pitch;
    }

    *pitch = width * 2;
  }
  else if (format == RETRO_PIXEL_FORMAT_XRGB8888)
  {
    logger->info(TAG "Converting from 24-bits RGB to XRGB8888");

    pixels = malloc(width * height * 4);

    if (pixels == NULL)
    {
      logger->error(TAG "Error allocating memory for the screenshot");
      return NULL;
    }

    uint8_t* source_rgb888 = (uint8_t*)data;
    uint8_t* target_rgba8880 = (uint8_t*)pixels;

    for (unsigned y = 0; y < height; y++)
    {
      uint8_t* row = source_rgb888;

      for (unsigned x = 0; x < width; x++)
      {
        *target_rgba8880++ = *source_rgb888++;
        *target_rgba8880++ = *source_rgb888++;
        *target_rgba8880++ = *source_rgb888++;
        *target_rgba8880++ = 255;
      }

      source_rgb888 = row + *pitch;
    }

    *pitch = width * 4;
  }
  else
  {
    logger->error(TAG "Unknown pixel format");
    return NULL;
  }

  return pixels;
}

const void* util::loadImage(Logger* logger, const std::string& path, unsigned* width, unsigned* height, unsigned* pitch)
{
  int w, h;
  void* rgb888 = stbi_load(path.c_str(), &w, &h, NULL, STBI_rgb);

  logger->info(TAG "Read image %u x %u from %s", w, h, path.c_str());

  *width = w;
  *height = h;
  *pitch = w * 3;

  return rgb888;
}
