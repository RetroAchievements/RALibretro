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

#include "CdRom.h"

#include "Util.h"

#include <stdint.h>
#include <stdlib.h>

#include <string.h>

static bool cdrom_open_cue(cdrom_t& cdrom, const char* filename, int track)
{
  FILE* fp;
  char buffer[1024], *file, *ptr, *ptr2;
  char buffer2[_MAX_PATH];
  int num;

  memset(&cdrom, 0, sizeof(cdrom_t));

  fp = fopen(filename, "r");
  if (!fp)
    return false;

  fread(buffer, 1, sizeof(buffer), fp);
  fclose(fp);

  for (ptr = buffer; *ptr; ++ptr)
  {
    while (*ptr == ' ')
      ++ptr;

    if (strncmp(ptr, "FILE ", 5) == 0)
    {
      file = ptr + 5;
    }
    else if (strncmp(ptr, "TRACK ", 6) == 0)
    {
      ptr += 6;
      num = atoi(ptr);
      if (num == track)
      {
        // open bin file
        ptr2 = file;
        if (*ptr2 == '"')
        {
          ++file;
          do
          {
            ++ptr2;
          } while (*ptr2 != '\n' && *ptr2 != '"');
        }
        else
        {
          do
          {
            ++ptr2;            
          } while (*ptr2 != '\n' && *ptr2 != ' ');
        }
        *ptr2 = '\0';

        std::string binFileName = util::replaceFileName(filename, file);
        cdrom.fp = fopen(binFileName.c_str(), "rb");
        if (!cdrom.fp)
          return false;

        // determine frame size
        while (*ptr != ' ')
          ++ptr;
        while (*ptr == ' ')
          ++ptr;

        cdrom.sector_size = 2352;
        if (strncmp(ptr, "MODE", 4) == 0)
          cdrom.sector_size = atoi(ptr + 6);

        return true;
      }
    }

    while (*ptr && *ptr != '\n')
      ++ptr;
  }

  return false;
}

static int cdrom_get_cd_names_m3u(const char* filename, char names[][128], int max_names)
{
  int count = 0;
  char buffer[2048], *ptr, *start;
  char first_file[_MAX_PATH];
  FILE* fp = fopen(filename, "r");
  if (fp)
  {
    fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    buffer[sizeof(buffer) - 1] = '\0';
    ptr = buffer;
    do
    {
      start = ptr;
      while (*ptr && *ptr != '\n')
        ++ptr;

      if (!*ptr)
        break;

      *ptr = '\0';
      strcpy(names[count], start);
      ++count;
      if (count == max_names)
        break;

      ++ptr;
    } while (true);
  }

  return count;
}

int cdrom_get_cd_names(const char* filename, char names[][128], int max_names)
{
  int len = strlen(filename);
  if (len > 4 && stricmp(&filename[len - 4], ".m3u") == 0)
    return cdrom_get_cd_names_m3u(filename, names, max_names);

  std::string filename_path = util::fileNameWithExtension(filename);
  strcpy(names[0], filename_path.c_str());
  return 1;
}

static bool cdrom_open_m3u(cdrom_t& cdrom, const char* filename, int disc, int track)
{
  char files[1][128];
  if (!cdrom_get_cd_names_m3u(filename, files, 1))
    return false;

  std::string cueFileName = util::replaceFileName(filename, files[0]);
  return cdrom_open(cdrom, cueFileName.c_str(), disc, track);
}

bool cdrom_open(cdrom_t& cdrom, const char* filename, int disc, int track)
{
  int len = strlen(filename);
  if (len > 4 && stricmp(&filename[len - 4], ".m3u") == 0)
    return cdrom_open_m3u(cdrom, filename, disc, track);

  return cdrom_open_cue(cdrom, filename, track);
}

void cdrom_close(cdrom_t& cdrom)
{
  if (cdrom.fp)
  {
    fclose(cdrom.fp);
    cdrom.fp = NULL;
  }
}

static int cdrom_sector_start(cdrom_t& cdrom, int sector)
{
  // each sector has a 12-byte header (that's included in sector_size).
  // the entire file also has a 12-byte header
  return sector * cdrom.sector_size + 12 + 12;
}

static void cdrom_seek_sector(cdrom_t& cdrom, int sector)
{
  cdrom.sector_start = cdrom_sector_start(cdrom, sector);
  fseek(cdrom.fp, cdrom.sector_start, SEEK_SET);
  cdrom.sector_remaining = 2048;
}

bool cdrom_seek_file(cdrom_t& cdrom, const char* filename)
{
  uint8_t buffer[2048], *tmp;
  int sector;
  int filename_length = strlen(filename);

  if (!cdrom.fp)
    return false;

  // find the cd information (always 16 frames in)
  cdrom_seek_sector(cdrom, 16);
  fread(buffer, 1, 256, cdrom.fp);

  // the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
  sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);
  cdrom_seek_sector(cdrom, sector);

  // process the table of contents
  cdrom_read(cdrom, buffer, sizeof(buffer));
  tmp = buffer;

  while (tmp < buffer + sizeof(buffer))
  {
    if (!*tmp)
      return false;

    // filename is 33 bytes into the record and the format is "FILENAME;version"
    if (tmp[33 + filename_length] == ';' && !strnicmp((const char*)(tmp + 33), filename, filename_length))
    {
      sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
      cdrom_seek_sector(cdrom, sector);
      return true;
    }

    // the first byte of the record is the length of the record
    tmp += *tmp;
  }

  return false;
}

int cdrom_read(cdrom_t& cdrom, void* buffer, int num_bytes)
{
  int bytes_read;

  if (!cdrom.fp)
    return 0;

  bytes_read = 0;
  while (num_bytes > cdrom.sector_remaining)
  {
    fread(buffer, 1, cdrom.sector_remaining, cdrom.fp);
    buffer = ((uint8_t*)buffer) + cdrom.sector_remaining;
    bytes_read += cdrom.sector_remaining;
    cdrom.sector_remaining = 2048;
    cdrom.sector_start += cdrom.sector_size;
    fseek(cdrom.fp, cdrom.sector_start, SEEK_SET);
  }

  fread(buffer, 1, num_bytes, cdrom.fp);
  cdrom.sector_remaining -= num_bytes;
  bytes_read += num_bytes;

  return bytes_read;
}
