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

static void cdrom_determine_sector_size(cdrom_t& cdrom)
{
  // Attempt to determine the sector and header sizes. The CUE file may be lying.
  // Look for the sync pattern using each of the supported sector sizes.
  // Then check for the presence of "CD001", which is gauranteed to be in either the
  // boot record or primary volume descriptor, one of which is always at sector 16.
  const unsigned char sync_pattern[] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
  };

  unsigned char header[32];
  cdrom.sector_size = 0;

  fseek(cdrom.fp, 16 * 2352, SEEK_SET);
  fread(header, 1, sizeof(header), cdrom.fp);

  if (memcmp(header, sync_pattern, 12) == 0)
  {
    cdrom.sector_size = 2352;

    if (memcmp(&header[25], "CD001", 5) == 0)
      cdrom.sector_header_size = 24;
    else if (memcmp(&header[17], "CD001", 5) == 0)
      cdrom.sector_header_size = 16;
  }
  else
  {
    fseek(cdrom.fp, 16 * 2336, SEEK_SET);
    fread(header, 1, sizeof(header), cdrom.fp);

    if (memcmp(header, sync_pattern, 12) == 0)
    {
      cdrom.sector_size = 2336;

      if (memcmp(&header[25], "CD001", 5) == 0)
        cdrom.sector_header_size = 24;
      else if (memcmp(&header[17], "CD001", 5) == 0)
        cdrom.sector_header_size = 16;
    }
    else
    {
      fseek(cdrom.fp, 16 * 2048, SEEK_SET);
      fread(header, 1, sizeof(header), cdrom.fp);

      if (memcmp(&header[1], "CD001", 5) == 0)
      {
        cdrom.sector_size = 2048;
        cdrom.sector_header_size = 0;
      }
    }
  }

  /* no recognizable header - attempt to determine sector size from stream size */
  if (cdrom.sector_size == 0)
  {
    fseek(cdrom.fp, 0, SEEK_END);
    long size = ftell(cdrom.fp);
    if ((size % 2352) == 0)
    {
      /* audio tracks use all 2352 bytes without a header */
      cdrom.sector_size = 2352;
      cdrom.sector_header_size = 0;
    }
    else if ((size % 2048) == 0)
    {
      /* cooked tracks eliminate all header/footer data */
      cdrom.sector_size = 2048;
      cdrom.sector_header_size = 0;
    }
    else if ((size % 2336) == 0)
    {
      /* MODE 2 format without 16-byte sync data */
      cdrom.sector_size = 2336;
      cdrom.sector_header_size = 8;
    }
  }
}

static bool cdrom_open_cue(cdrom_t& cdrom, const char* filename, int track, Logger* logger)
{
  FILE* fp;
  char buffer[1024], *file = buffer, *ptr, *ptr2;
  int num;

  memset(&cdrom, 0, sizeof(cdrom_t));

  fp = util::openFile(logger, filename, "r");
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
        cdrom.fp = util::openFile(logger, binFileName, "rb");
        if (!cdrom.fp)
          return false;

        // determine frame size
        while (*ptr != ' ')
          ++ptr;
        while (*ptr == ' ')
          ++ptr;

        cdrom_determine_sector_size(cdrom);

        // could not determine, which means we'll probably have more issues later
        // but use the CUE provided information anyway
        if (cdrom.sector_size == 0)
        {
          // All of these modes have 2048 byte payloads. In MODE1/2352 and MODE2/2352
          // modes, the mode can actually be specified per sector to change the payload
          // size, but that reduces the ability to recover from errors when the disc
          // is damaged, so it's seldomly used, and when it is, it's mostly for audio
          // or video data where a blip or two probably won't be noticed by the user.
          // So, while we techincally support all of the following modes, we only do
          // so with 2048 byte payloads.
          // http://totalsonicmastering.com/cuesheetsyntax.htm
          // MODE1/2048 ? CDROM Mode1 Data (cooked) [no header, no footer]
          // MODE1/2352 ? CDROM Mode1 Data (raw)    [16 byte header, 288 byte footer]
          // MODE2/2336 ? CDROM-XA Mode2 Data       [8 byte header, 280 byte footer]
          // MODE2/2352 ? CDROM-XA Mode2 Data       [24 byte header, 280 byte footer]

          cdrom.sector_size = 2352;       // default to MODE1/2352
          cdrom.sector_header_size = 16;

          if (strncmp(ptr, "MODE2/2352", 10) == 0)
          {
            cdrom.sector_header_size = 24;
          }
          else if (strncmp(ptr, "MODE1/2048", 10) == 0)
          {
            cdrom.sector_size = 2048;
            cdrom.sector_header_size = 0;
          }
          else if (strncmp(ptr, "MODE2/2336", 10) == 0)
          {
            cdrom.sector_size = 2336;
            cdrom.sector_header_size = 8;
          }
        }

        return true;
      }
    }

    while (*ptr && *ptr != '\n')
      ++ptr;
  }

  return false;
}

static int cdrom_get_cd_names_m3u(const char* filename, char names[][128], int max_names, Logger* logger)
{
  int count = 0;
  char buffer[2048], *ptr, *start;
  FILE* fp = util::openFile(logger, filename, "r");
  if (fp)
  {
    size_t bytes = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    buffer[bytes] = '\0';
    ptr = buffer;
    do
    {
      start = ptr;
      while (*ptr && *ptr != '\n')
        ++ptr;

      if (ptr > start)
      {
        char* name = names[count];
        memcpy(name, start, ptr - start);
        name[ptr - start] = '\0';
        ++count;

        if (count == max_names)
          break;
      }

      if (!*ptr)
        break;

      ++ptr;
    } while (true);
  }

  return count;
}

int cdrom_get_cd_names(const char* filename, char names[][128], int max_names, Logger* logger)
{
  int len = strlen(filename);
  if (len > 4 && strcasecmp(&filename[len - 4], ".m3u") == 0)
    return cdrom_get_cd_names_m3u(filename, names, max_names, logger);

  std::string filename_path = util::fileNameWithExtension(filename);
  strcpy(names[0], filename_path.c_str());
  return 1;
}

static bool cdrom_open_m3u(cdrom_t& cdrom, const char* filename, int disc, int track, Logger* logger)
{
  char files[1][128];
  if (!cdrom_get_cd_names_m3u(filename, files, 1, logger))
    return false;

  std::string cueFileName = util::replaceFileName(filename, files[0]);
  return cdrom_open(cdrom, cueFileName.c_str(), disc, track, logger);
}

bool cdrom_open(cdrom_t& cdrom, const char* filename, int disc, int track, Logger* logger)
{
  int len = strlen(filename);
  if (len > 4)
  {
    if (strcasecmp(&filename[len - 4], ".m3u") == 0)
      return cdrom_open_m3u(cdrom, filename, disc, track, logger);
    if (strcasecmp(&filename[len - 4], ".cue") == 0)
      return cdrom_open_cue(cdrom, filename, track, logger);
  }

  memset(&cdrom, 0, sizeof(cdrom_t));

  cdrom.fp = util::openFile(logger, filename, "r");
  if (!cdrom.fp)
    return false;

  cdrom_determine_sector_size(cdrom);
  if (cdrom.sector_size == 0)
  {
    fclose(cdrom.fp);
    return false;
  }

  return true;
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
  return sector * cdrom.sector_size + cdrom.sector_header_size;
}

void cdrom_seek_sector(cdrom_t& cdrom, int sector)
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

  const char* slash = strrchr(filename, '\\');
  if (slash)
  {
    // find the directory record for the first part of the path
    std::string sPath(filename, slash - filename);
    if (!cdrom_seek_file(cdrom, sPath.c_str()))
      return false;

    filename += sPath.length() + 1;
    filename_length -= sPath.length() + 1;
  }
  else
  {
    // find the cd information (always 16 frames in)
    cdrom_seek_sector(cdrom, 16);
    fread(buffer, 1, 256, cdrom.fp);

    // the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
    // https://www.cdroller.com/htm/readdata.html
    sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);
    cdrom_seek_sector(cdrom, sector);
  }

  // process the table of contents
  cdrom_read(cdrom, buffer, sizeof(buffer));
  tmp = buffer;

  while (tmp < buffer + sizeof(buffer))
  {
    if (!*tmp)
      return false;

    // filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY"
    if ((tmp[33 + filename_length] == ';' || tmp[33 + filename_length] == '\0') && !strncasecmp((const char*)(tmp + 33), filename, filename_length))
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
