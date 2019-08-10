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
#include "Emulator.h"

#include "components\Config.h"
#include "components\Logger.h"

#include <jsonsax\jsonsax.h>

#define TAG "[EMU] "

//  Manages multi-disc games
extern void RA_ActivateDisc(unsigned char* pExe, size_t nExeSize);
extern void RA_DeactivateDisc();

struct CoreInfo
{
  std::string name;
  std::string filename;
  std::string extensions;
  std::set<System> systems;
};

static std::vector<CoreInfo> s_coreInfos;

static const CoreInfo* getCoreInfo(const std::string& coreName, System system)
{
  for (const auto& coreInfo : s_coreInfos)
  {
    if (coreInfo.filename == coreName)
    {
      if (coreInfo.systems.find(system) != coreInfo.systems.end())
        return &coreInfo;
    }
  }

  return NULL;
}

const std::string& getEmulatorName(const std::string& coreName, System system)
{
  const auto* coreInfo = getCoreInfo(coreName, system);
  return (coreInfo != NULL) ? coreInfo->name : coreName;
}

const char* getEmulatorExtensions(const std::string& coreName, System system)
{
  const auto* coreInfo = getCoreInfo(coreName, system);
  if (coreInfo != NULL && !coreInfo->extensions.empty())
    return coreInfo->extensions.c_str();

  return NULL;
}

bool loadCores(Config* config, Logger* logger)
{
  std::string path = config->getRootFolder();
  path += "Cores\\cores.json";

  logger->debug(TAG "Identifying available cores");

  size_t size;
  void* data = util::loadFile(logger, path, &size);

  if (data == NULL)
  {
    logger->error(TAG "Could not locate cores.json");
    return false;
  }

  struct Deserialize
  {
    CoreInfo* core;
    std::string key;
    Config* config;
  };

  Deserialize ud;
  ud.core = NULL;
  ud.config = config;

  jsonsax_parse((char*)data, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
  {
    auto ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);

      std::string path = ud->config->getRootFolder();
      path += "Cores\\" + ud->key + ".dll";

      FILE* file = fopen(path.c_str(), "r");
      if (file)
      {
        fclose(file);

        s_coreInfos.emplace_back();
        ud->core = &s_coreInfos.back();
        ud->core->filename = ud->key;
      }
      else
      {
        ud->core = NULL;
      }
    }
    else if (event == JSONSAX_OBJECT)
    {
      if (ud->core != NULL)
      {
        jsonsax_result_t res2 = jsonsax_parse((char*)str, ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
        {
          auto ud = (Deserialize*)udata;

          if (event == JSONSAX_KEY)
          {
            ud->key = std::string(str, num);
          }
          else if (event == JSONSAX_STRING)
          {
            if (ud->key == "name")
              ud->core->name = std::string(str, num);
            else if (ud->key == "extensions")
              ud->core->extensions = std::string(str, num);
          }
          else if (event == JSONSAX_ARRAY)
          {
            if (ud->key == "systems")
            {
              jsonsax_result_t res3 = jsonsax_parse((char*)str, ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
              {
                auto ud = (Deserialize*)udata;

                if (event == JSONSAX_NUMBER)
                {
                  std::string system = std::string(str, num);
                  ud->core->systems.insert(static_cast<System>(std::stoi(system)));
                }

                return 0;
              });
            }
          }

          return 0;
        });
      }
    }

    return 0;
  });

  free(data);
  return true;
}

void getAvailableSystems(std::set<System>& systems)
{
  for (const auto& core : s_coreInfos)
  {
    for (auto system : core.systems)
      systems.insert(system);
  }
}

void getSystemCores(System system, std::set<std::string>& coreNames)
{
  for (const auto& core : s_coreInfos)
  {
    if (core.systems.find(system) != core.systems.end())
      coreNames.insert(core.name);
  }
}

int encodeCoreName(const std::string& coreName, System system)
{
  for (int i = 0; i < s_coreInfos.size(); ++i)
  {
    if (s_coreInfos[i].name == coreName)
    {
      int j = 0;
      for (auto s : s_coreInfos[i].systems)
      {
        if (s == system)
          return (j * 100) + i;

        ++j;
      }
    }
  }

  return 0;
}

const std::string& getCoreName(int encoded, System& system)
{
  int i = encoded % 100;
  int j = encoded / 100;

  if (i > s_coreInfos.size())
    i = 0;

  if (j > s_coreInfos[i].systems.size())
    j = 0;

  for (auto s : s_coreInfos[i].systems)
  {
    if (j == 0)
    {
      system = s;
      break;
    }

    --j;
  }

  return s_coreInfos[i].filename;
}

const char* getSystemName(System system)
{
  switch (system)
  {
  case System::kAtari2600:      return "Atari 2600";
  case System::kAtariLynx:      return "Atari Lynx";
  case System::kMasterSystem:   return "Master System";
  case System::kMegaDrive:      return "Sega Genesis";
  case System::kNintendo:       return "Nintendo Entertainment System";
  case System::kPCEngine:       return "TurboGrafx-16";
  case System::kSuperNintendo:  return "Super Nintendo Entertainment System";
  case System::kGameBoy:        return "Game Boy";
  case System::kGameBoyColor:   return "Game Boy Color";
  case System::kGameBoyAdvance: return "Game Boy Advance";
  case System::kPlayStation1:   return "PlayStation";
  case System::kNeoGeoPocket:   return "Neo Geo Pocket";
  case System::kVirtualBoy:     return "Virtual Boy";
  case System::kGameGear:       return "Game Gear";
  case System::kArcade:         return "Arcade";
  case System::kAtari7800:      return "Atari 7800";
  default:                      break;
  }
  
  return "?";
}

static bool romLoadedWithPadding(void* rom, size_t size, size_t max_size, int fill)
{
  uint8_t* data = (uint8_t*)malloc(max_size);

  if (data != NULL)
  {
    if (size < max_size)
    {
      memcpy(data, rom, size);
      memset(data + size, fill, max_size - size);
    }
    else
    {
      memcpy(data, rom, max_size);
    }

    RA_OnLoadNewRom(data, max_size);
    free(data);
    return true;
  }

  return false;
}

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

static bool romLoadPsx(Logger* logger, const std::string& path)
{
  cdrom_t cdrom;
  char buffer[2048], *tmp, *exe_name;
  uint8_t* exe_raw;
  int size, remaining;

  if (!cdrom_open(cdrom, path.c_str(), 1, 1))
    return false;

  exe_name = NULL;

  // extract the "BOOT=executable" line from the "SYSTEM.CNF" file, then hash the contents of "executable"
  if (!cdrom_seek_file(cdrom, "SYSTEM.CNF"))
  {
    logger->debug(TAG "Failed to locate SYSTEM.CNF in %s", path.c_str());
  }
  else
  {
    cdrom_read(cdrom, buffer, sizeof(buffer));
    for (tmp = buffer; tmp < &buffer[sizeof(buffer)] && *tmp; ++tmp)
    {
      if (strncmp(tmp, "BOOT", 4) == 0)
      {
        exe_name = tmp + 4;
        while (isspace(*exe_name))
          ++exe_name;
        if (*exe_name == '=')
        {
          ++exe_name;
          while (isspace(*exe_name))
            ++exe_name;

          if (strncmp(exe_name, "cdrom:", 6) == 0)
            exe_name += 6;
          if (*exe_name == '\\')
            ++exe_name;
          break;
        }
      }

      while (*tmp && *tmp != '\n')
        ++tmp;
    }

    if (!exe_name)
    {
      logger->debug(TAG "Failed to locate BOOT directive in SYSTEM.CNF in %s", path.c_str());
    }
    else
    {
      tmp = exe_name;
      while (*tmp != '\n' && *tmp != ';' && *tmp != ' ')
        ++tmp;
      strncpy(buffer, exe_name, tmp - exe_name);
      buffer[tmp - exe_name] = '\0';

      if (!cdrom_seek_file(cdrom, buffer))
      {
        logger->debug(TAG "Failed to locate %s in %s", buffer, path.c_str());
      }
      else
      {
        cdrom_read(cdrom, buffer, sizeof(buffer));

        // the PSX-E header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
        // include the header itself. We want to include the header in the hash, so append another 2048 to that value.
        remaining = size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;
        exe_raw = (uint8_t*)malloc(size);
        tmp = (char*)exe_raw;

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
      }
    }
  }

  cdrom_close(cdrom);

  return (exe_name != NULL);
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
  case System::kPCEngine:
  case System::kGameBoy:
  case System::kGameBoyColor:
  case System::kGameBoyAdvance:
  case System::kNeoGeoPocket:
  case System::kMasterSystem:
  case System::kMegaDrive:
  case System::kAtari7800:
  default:
    RA_OnLoadNewRom((BYTE*)rom, size);
    ok = true;
    break;

  case System::kSuperNintendo:
    ok = romLoadSnes(rom, size);
    break;

  case System::kNintendo:
    ok = romLoadNes(rom, size);
    break;
  
  case System::kAtariLynx:
    if (!memcmp("LYNX", (void *)rom, 5))
    {
      RA_OnLoadNewRom((BYTE*)rom + 0x0040, size - 0x0040);
    }
    else
    {
      RA_OnLoadNewRom((BYTE*)rom, size);
    }

    ok = true;
    break;
  
  case System::kPlayStation1:
    ok = romLoadPsx(logger, path);
    break;
  
  case System::kArcade:
    ok = romLoadArcade(path);
    break;
  }

  if (ok && system != System::kPlayStation1)
    RA_DeactivateDisc();

  return ok;
}

void romUnloaded(Logger* logger)
{
  RA_DeactivateDisc();
  RA_ActivateGame(0);
}
