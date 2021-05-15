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

#include "CdRom.h"

#include "Util.h"

#include <stdint.h>
#include <stdlib.h>

#include <string.h>

static int cdrom_get_cd_names_m3u(const char* filename, std::vector<std::string>* disc_paths, Logger* logger)
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
        std::string path(start, ptr - start);
        disc_paths->push_back(path);
        ++count;
      }

      if (!*ptr)
        break;

      ++ptr;
    } while (true);
  }

  return count;
}

int cdrom_get_cd_names(const char* filename, std::vector<std::string>* disc_paths, Logger* logger)
{
  disc_paths->clear();

  int len = strlen(filename);
  if (len > 4 && strcasecmp(&filename[len - 4], ".m3u") == 0)
    return cdrom_get_cd_names_m3u(filename, disc_paths, logger);

  std::string filename_path = util::fileNameWithExtension(filename);
  disc_paths->push_back(std::move(filename_path));
  return 1;
}
