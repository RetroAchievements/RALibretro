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
