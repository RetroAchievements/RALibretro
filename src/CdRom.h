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

#include <stdio.h>

struct cdrom_t
{
  FILE* fp;
  int sector_size;
  int sector_start;
  int sector_remaining;
  int sector_header_size;
};

bool cdrom_open(cdrom_t& cdrom, const char* filename, int disc, int track);
void cdrom_close(cdrom_t& cdrom);

bool cdrom_seek_file(cdrom_t& cdrom, const char* filename);
int cdrom_read(cdrom_t& cdrom, void* buffer, int num_bytes);

int cdrom_get_cd_names(const char* filename, char names[][128], int max_names);
