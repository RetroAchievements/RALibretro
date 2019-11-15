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
#include "Hash.h"
#include "Util.h"

#include <memory.h>
#include <string.h>

#define TAG "[HASH]"

//  Manages multi-disc games
extern void RA_ActivateDisc(unsigned char* pExe, size_t nExeSize);
extern void RA_DeactivateDisc();

static bool romLoadSnes(void* rom, size_t size)
{
  // if the file contains a header, ignore it
  uint8_t* raw = (uint8_t*)rom;
  uint32_t calc_size = (size / 0x2000) * 0x2000;
  if (size - calc_size == 512)
  {
    raw += 512;
    size -= 512;
  }

  RA_OnLoadNewRom(raw, size);
  return true;
}

static bool romLoadNes(void* rom, size_t size)
{
  // if the file contains a header, ignore it
  uint8_t* raw = (uint8_t*)rom;
  if (raw[0] == 'N' && raw[1] == 'E' && raw[2] == 'S' && raw[3] == 0x1A)
  {
    raw += 16;
    size -= 16;
  }

  RA_OnLoadNewRom(raw, size);
  return true;
}

static bool romLoadLynx(void* rom, size_t size)
{
  // if the file contains a header, ignore it
  uint8_t* raw = (uint8_t*)rom;
  if (!memcmp("LYNX", raw, 5))
  {
    raw += 0x0040;
    size -= 0x0040;
  }

  RA_OnLoadNewRom(raw, size);
  return true;
}

static bool romLoadPsx(Logger* logger, const std::string& path)
{
  cdrom_t cdrom;
  char buffer[2048], *tmp, *exe_name_start;
  std::string exe_name;
  uint8_t* exe_raw;
  int size, remaining;
  bool success = false;

  if (!cdrom_open(cdrom, path.c_str(), 1, 1, logger))
    return false;

  // extract the "BOOT=executable" line from the "SYSTEM.CNF" file, then hash the contents of "executable"
  if (!cdrom_seek_file(cdrom, "SYSTEM.CNF"))
  {
    if (cdrom_seek_file(cdrom, "PSX.EXE"))
      exe_name = "PSX.EXE";
    else
      logger->info(TAG "Failed to locate SYSTEM.CNF in %s", path.c_str());
  }
  else
  {
    cdrom_read(cdrom, buffer, sizeof(buffer));
    for (tmp = buffer; tmp < &buffer[sizeof(buffer)] && *tmp; ++tmp)
    {
      if (strncmp(tmp, "BOOT", 4) == 0)
      {
        exe_name_start = tmp + 4;
        while (isspace(*exe_name_start))
          ++exe_name_start;
        if (*exe_name_start == '=')
        {
          ++exe_name_start;
          while (isspace(*exe_name_start))
            ++exe_name_start;

          if (strncmp(exe_name_start, "cdrom:", 6) == 0)
            exe_name_start += 6;
          if (*exe_name_start == '\\')
            ++exe_name_start;

          tmp = exe_name_start;
          while (!isspace(*tmp) && *tmp != ';')
            ++tmp;

          exe_name.assign(exe_name_start, tmp - exe_name_start);
          break;
        }
      }

      while (*tmp && *tmp != '\n')
        ++tmp;
    }

    if (exe_name.empty())
    {
      logger->info(TAG "Failed to locate BOOT directive in SYSTEM.CNF in %s", path.c_str());
    }
    else if (!cdrom_seek_file(cdrom, exe_name.c_str()))
    {
      logger->info(TAG "Failed to locate %s in %s", exe_name.c_str(), path.c_str());
      exe_name.clear();
    }
  }

  if (!exe_name.empty())
  {
    cdrom_read(cdrom, buffer, sizeof(buffer));

    // the PSX-E header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
    // include the header itself. We want to include the header in the hash, so append another 2048 to that value.
    size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;

    // there's also a few games that are use a singular engine and only differ via their data files. luckily, they have
    // unique serial numbers, and use the serial number as the boot file in the standard way. include the boot file in the hash
    size += exe_name.length();

    exe_raw = (uint8_t*)malloc(size);
    tmp = (char*)exe_raw;

    memcpy(tmp, exe_name.c_str(), exe_name.length());
    remaining = size - exe_name.length();
    tmp += exe_name.length();

    do
    {
      memcpy(tmp, buffer, sizeof(buffer));
      remaining -= sizeof(buffer);

      if (remaining <= 0)
        break;

      tmp += sizeof(buffer);
      cdrom_read(cdrom, buffer, sizeof(buffer));
    } while (true);

    RA_ActivateDisc(exe_raw, size);
    free(exe_raw);

    success = true;
  }

  cdrom_close(cdrom);

  return success;
}

static bool romLoadSegaCd(Logger* logger, const std::string& path)
{
  cdrom_t cdrom;
  unsigned char buffer[512];

  if (!cdrom_open(cdrom, path.c_str(), 1, 1, logger))
    return false;

  // the first 512 bytes of sector 0 are a volume header and ROM header that uniquely identify the game.
  // After that is an arbitrary amount of code that ensures the game is being run in the correct region.
  // Then more arbitrary code follows that actually starts the boot process. Somewhere in there, the
  // primary executable is loaded. In many cases, a single game will have multiple executables, so even
  // if we could determine the primary one, it's just the tip of the iceberg. As such, we've decided that
  // hashing the volume and ROM headers is sufficient for identifying the game, and we'll have to trust
  // that our players aren't modifying anything else on the disc.
  cdrom_seek_sector(cdrom, 0);
  cdrom_read(cdrom, buffer, sizeof(buffer));
  cdrom_close(cdrom);

  RA_ActivateDisc(buffer, sizeof(buffer));

  return true;
}

static bool romLoadPcEngineCd(Logger* logger, const std::string& path)
{
  cdrom_t cdrom;
  unsigned char buffer[128];

  if (!cdrom_open(cdrom, path.c_str(), 1, 0, logger))
    return false;

  // the PC-Engine uses the second sector to specify boot information and program name.
  // the string "PC Engine CD-ROM SYSTEM" should exist at 32 bytes into the sector
  // http://shu.sheldows.com/shu/download/pcedocs/pce_cdrom.html
  cdrom_seek_sector(cdrom, 1);
  cdrom_read(cdrom, buffer, sizeof(buffer));

  if (strncmp("PC Engine CD-ROM SYSTEM", (const char*)&buffer[32], 23) != 0)
    return false;

  // the first three bytes specify the sector of the program data, and the fourth byte
  // is the number of sectors.
  const unsigned int first_sector = buffer[0] * 65536 + buffer[1] * 256 + buffer[2];
  const unsigned int num_sectors = buffer[3];
  const unsigned int num_bytes = num_sectors * 2048;

  unsigned char* program = (unsigned char*)malloc(num_bytes + 22);
  memcpy(program, &buffer[106], 22);
  cdrom_seek_sector(cdrom, first_sector);
  cdrom_read(cdrom, program + 22, num_bytes);
  cdrom_close(cdrom);

  RA_ActivateDisc(program, num_bytes + 22);

  free(program);
  return true;
}

static bool romLoadArcade(const std::string& path)
{
  std::string name = util::fileName(path);
  RA_OnLoadNewRom((BYTE*)name.c_str(), name.length());
  return true;
}

bool romLoaded(Logger* logger, System system, const std::string& path, void* rom, size_t size)
{
  bool ok = false;

  switch (system)
  {
    case System::kAtari2600:
    case System::kAtari7800:
    case System::kAtariJaguar:
    case System::kColecovision:
    case System::kPokemonMini:
    case System::kGameBoy:
    case System::kGameBoyColor:
    case System::kGameBoyAdvance:
    case System::kNintendo64:
    case System::kVirtualBoy:
    case System::kNeoGeoPocket:
    case System::kMasterSystem:
    case System::kMegaDrive:
    case System::kSega32X:
    case System::kGameGear:
    case System::kSG1000:
    default:
      RA_OnLoadNewRom((BYTE*)rom, size);
      ok = true;
      break;

    case System::kPCEngine:
      if (strcasecmp(util::extension(path).c_str(), ".cue") == 0)
      {
        ok = romLoadPcEngineCd(logger, path);
      }
      else
      {
        RA_OnLoadNewRom((BYTE*)rom, size);
        ok = true;
      }
      break;

    case System::kSuperNintendo:
      ok = romLoadSnes(rom, size);
      break;

    case System::kNintendo:
      ok = romLoadNes(rom, size);
      break;

    case System::kAtariLynx:
      ok = romLoadLynx(rom, size);
      break;

    case System::kPlayStation1:
      ok = romLoadPsx(logger, path);
      break;

    case System::kSegaCD:
    case System::kSaturn:
      ok = romLoadSegaCd(logger, path);
      break;

    case System::kArcade:
      ok = romLoadArcade(path);
      break;
  }

  if (ok)
  {
    switch (system)
    {
      case System::kPlayStation1:
      case System::kSaturn:
      case System::kSegaCD:
        // these systems call RA_ActivateDisc and may be multi-disc systems
        break;

      default:
        // all other systems should deactive any previously active disc
        RA_DeactivateDisc();
        break;
    }
  }

  return ok;
}

void romUnloaded(Logger* logger)
{
  RA_DeactivateDisc();
  RA_ActivateGame(0);
}
