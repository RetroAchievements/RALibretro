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

#include "Emulator.h"

#include "Util.h"

#include "components/Config.h"
#include "components/Logger.h"

#ifdef _WINDOWS
#include "components/Dialog.h"
#include <RAInterface/RA_Consoles.h>
#endif

#include <jsonsax/jsonsax.h>

#include <ctime>
#include <map>

#define TAG "[EMU] "

#if defined(_M_X64) || defined(__amd64__)
static const char* BUILDBOT_URL = "https://buildbot.libretro.com/nightly/windows/x86_64/latest/";
#else
static const char* BUILDBOT_URL = "https://buildbot.libretro.com/nightly/windows/x86/latest/";
#endif

struct CoreInfo
{
  std::string name;
  std::string filename;
  std::string extensions;
  std::string deprecationMessage;
  std::set<int> systems;
  time_t filetime;
  time_t servertime;
};

static std::vector<CoreInfo> s_coreInfos;

static const CoreInfo* getCoreInfo(const std::string& coreName, int system)
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

const std::string& getEmulatorName(const std::string& coreName, int system)
{
  const auto* coreInfo = getCoreInfo(coreName, system);
  return (coreInfo != NULL) ? coreInfo->name : coreName;
}

const char* getEmulatorExtensions(const std::string& coreName, int system)
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
    Logger* logger;
    bool inCore;
    bool inSystems;
  };

  Deserialize ud;
  ud.core = NULL;
  ud.config = config;
  ud.logger = logger;
  ud.inCore = false;
  ud.inSystems = false;

  jsonsax_parse((char*)data, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
  {
    auto ud = (Deserialize*)udata;

    switch (event)
    {
      case JSONSAX_KEY:
        ud->key = std::string(str, num);

        if (!ud->inCore)
        {
          std::string path = ud->config->getRootFolder();
          path += "Cores\\" + ud->key + ".dll";

          s_coreInfos.emplace_back();
          ud->core = &s_coreInfos.back();
          ud->core->filename = ud->key;
          ud->core->filetime = util::fileTime(path);
          ud->core->servertime = 0;
        }
        break;

      case JSONSAX_OBJECT:
        if (ud->core != NULL)
        {
          ud->inCore = (num == 1);
        }
        break;

      case JSONSAX_STRING:
        if (ud->inCore && ud->core != NULL)
        {
          if (ud->key == "name")
            ud->core->name = std::string(str, num);
          else if (ud->key == "extensions")
            ud->core->extensions = std::string(str, num);
          else if (ud->key == "deprecated")
            ud->core->deprecationMessage = std::string(str, num);
          else if (ud->key == "platforms")
          {
            std::string value = std::string(str, num);
#if defined(_M_X64) || defined(__amd64__)
            if (value.find("win64") == std::string::npos)
#else
            if (value.find("win32") == std::string::npos)
#endif
            {
              s_coreInfos.pop_back();
              ud->core = nullptr;
              ud->inCore = 0; /* setting core to null will prevent this from happening naturally at the end of the object */
            }
          }
        }
        break;

      case JSONSAX_ARRAY:
        if (ud->inCore && ud->core != NULL)
        {
          if (ud->key == "systems")
          {
            ud->inSystems = (num == 1);
          }
        }
        break;

      case JSONSAX_NUMBER:
        if (ud->inSystems && ud->core != NULL)
        {
          std::string system = std::string(str, num);
          ud->core->systems.insert(static_cast<int>(std::stoi(system)));
        }
        break;

      default:
        break;
    }

    return 0;
  });

  free(data);
  return true;
}

void getAvailableSystems(std::set<int>& systems)
{
  for (const auto& core : s_coreInfos)
  {
    if (core.filetime)
    {
      for (auto system : core.systems)
        systems.insert(system);
    }
  }
}

void getAvailableSystemCores(int system, std::set<std::string>& coreNames)
{
  for (const auto& core : s_coreInfos)
  {
    if (core.filetime && core.systems.find(system) != core.systems.end())
      coreNames.insert(core.name);
  }
}

bool doesCoreSupportSystem(const std::string& coreFilename, int system)
{
  for (const auto& core : s_coreInfos)
  {
    if (core.filename == coreFilename && core.systems.find(system) != core.systems.end())
      return true;
  }

  return false;
}

int encodeCoreName(const std::string& coreName, int system)
{
  for (size_t i = 0; i < s_coreInfos.size(); ++i)
  {
    if (s_coreInfos[i].name == coreName)
    {
      int j = 0;
      for (auto s : s_coreInfos[i].systems)
      {
        if (s == system)
        {
          // the IDM resources allocated for selecting cores goes from 45000 to 49999, giving us 4999
          // unique identifiers to use. encode the core index and system subindex to fit in that range
          return (j * s_coreInfos.size()) + i;
        }

        ++j;
      }
    }
  }

  return 0;
}

const std::string& getCoreName(int encoded, int& system)
{
  const size_t i = encoded % s_coreInfos.size();
  size_t j = encoded / s_coreInfos.size();

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

const std::string* getCoreDeprecationMessage(const std::string& coreName)
{
  for (size_t i = 0; i < s_coreInfos.size(); ++i)
  {
    if (s_coreInfos[i].filename == coreName)
    {
      if (s_coreInfos[i].deprecationMessage.empty())
        return NULL;

      return &s_coreInfos[i].deprecationMessage;
    }
  }

  return NULL;
}

const char* getSystemName(int system)
{
  return rc_console_name(system);
}

const char* getSystemManufacturer(int system)
{
  switch (system)
  {
    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMECUBE:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_SUPER_NINTENDO:
    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
    case RC_CONSOLE_NINTENDO_3DS:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WII:
    case RC_CONSOLE_WII_U:
      return "Nintendo";

    case RC_CONSOLE_PLAYSTATION:
    case RC_CONSOLE_PLAYSTATION_2:
    case RC_CONSOLE_PSP:
      return "Sony";

    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_5200:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_ATARI_JAGUAR_CD:
    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_ATARI_ST:
      return "Atari";

    case RC_CONSOLE_PC_ENGINE:
    case RC_CONSOLE_PC_ENGINE_CD:
    case RC_CONSOLE_PC6000:
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_PC9800:
    case RC_CONSOLE_PCFX:
      return "NEC";

    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_PICO:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
    case RC_CONSOLE_DREAMCAST:
      return "Sega";

    case RC_CONSOLE_NEO_GEO_CD:
    case RC_CONSOLE_NEOGEO_POCKET:
      return "SNK";

    default:
      return "Other";
  }
}

#ifdef _WINDOWS

class CoreDialog : public Dialog
{
public:
  int systemIds[128];
  int numSystems = 0;
  std::vector<std::string> coreNames;
  Config* config;
  Logger* logger;
  const std::string* loadedCore;
  bool modified = false;

protected:

  void updateCores(HWND hwnd)
  {
    const HWND hComboBox = GetDlgItem(hwnd, 50000);
    const int selectedIndex = SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
    if (selectedIndex < 0)
      return;

    const int system = systemIds[selectedIndex];

    std::map<std::string, const CoreInfo*> systemCores;
    for (const auto& core : s_coreInfos)
    {
      if (!core.deprecationMessage.empty() && !core.filetime)
        continue;

      if (core.systems.find(system) != core.systems.end())
        systemCores.emplace(core.name, &core);
    }

    char buffer[64];
    size_t index = 0;
    for (const auto& pair : systemCores)
    {
      HWND hCoreName = GetDlgItem(hwnd, 51000 + index);
      HWND hLocalTime = GetDlgItem(hwnd, 51100 + index);
      HWND hServerTime = GetDlgItem(hwnd, 51200 + index);
      HWND hUpdate = GetDlgItem(hwnd, 51300 + index);
      HWND hDelete = GetDlgItem(hwnd, 51400 + index);

      ShowWindow(hCoreName, SW_SHOW);
      ShowWindow(hLocalTime, SW_SHOW);
      ShowWindow(hServerTime, SW_SHOW);
      ShowWindow(hUpdate, SW_SHOW);
      ShowWindow(hDelete, SW_SHOW);

      coreNames[index] = pair.second->filename;
      ++index;

      if (pair.second->deprecationMessage.empty())
      {
        SetWindowText(hCoreName, pair.first.c_str());
      }
      else
      {
        std::string deprecatedName = pair.first + " (Deprecated)";
        SetWindowText(hCoreName, deprecatedName.c_str());
      }

      if (pair.second->filetime)
      {
        struct tm tm;
        gmtime_s(&tm, &pair.second->filetime);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        SetWindowText(hLocalTime, buffer);
        SetWindowText(hUpdate, "Update");
        EnableWindow(hDelete, TRUE);
      }
      else
      {
        SetWindowText(hLocalTime, "n/a");
        SetWindowText(hUpdate, "Download");
        EnableWindow(hDelete, FALSE);
      }

      if (pair.second->servertime)
      {
        struct tm tm;
        gmtime_s(&tm, &pair.second->servertime);
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        SetWindowText(hServerTime, buffer);

        if (!pair.second->filetime)
        {
          EnableWindow(hUpdate, TRUE);
        }
        else
        {
          const auto roundedServerTime = pair.second->servertime / 86400;
          const auto roundedFileTime = pair.second->filetime / 86400;
          EnableWindow(hUpdate, (roundedServerTime > roundedFileTime));
        }
      }
      else
      {
        SetWindowText(hServerTime, "n/a");
        EnableWindow(hUpdate, FALSE);
      }
    }

    for (; index < coreNames.size(); ++index)
    {
      HWND hCoreName = GetDlgItem(hwnd, 51000 + index);
      HWND hLocalTime = GetDlgItem(hwnd, 51100 + index);
      HWND hServerTime = GetDlgItem(hwnd, 51200 + index);
      HWND hUpdate = GetDlgItem(hwnd, 51300 + index);
      HWND hDelete = GetDlgItem(hwnd, 51400 + index);

      ShowWindow(hCoreName, SW_HIDE);
      ShowWindow(hLocalTime, SW_HIDE);
      ShowWindow(hServerTime, SW_HIDE);
      ShowWindow(hUpdate, SW_HIDE);
      ShowWindow(hDelete, SW_HIDE);
    }
  }

  void deleteCore(HWND hwnd, const std::string& coreName)
  {
    std::string path = config->getRootFolder();
    path += "Cores\\" + coreName + ".dll";
    util::deleteFile(path);

    for (auto& core : s_coreInfos)
    {
      if (core.filename == coreName)
      {
        core.filetime = 0;
        modified = true;
        break;
      }
    }

    updateCores(hwnd);
  }

  void updateCore(HWND hwnd, const std::string& coreName)
  {
    std::string coreFile = coreName + ".dll";
    std::string path = config->getRootFolder();
    path += "Cores\\" + coreFile;
    std::string zipPath = path + ".zip";

    std::string url = BUILDBOT_URL;
    url += coreName + ".dll.zip";
    if (!util::downloadFile(logger, url, zipPath))
    {
      MessageBox(hwnd, "Download failed.", "Error", MB_OK);
    }

    if (!util::unzipFile(logger, zipPath, coreFile, path))
    {
      const auto unicodePath = util::utf8ToUChar(path);
      if (unicodePath.length() != path.length())
      {
        MessageBox(hwnd, "miniz does not support unicode paths. Please manually unzip the file in the Cores subdirectory and restart RALibRetro.", "Error", MB_OK);
      }
      else
      {
        MessageBox(hwnd, "Unzip failed. Please manually unzip the file in the Cores subdirectory and restart RALibRetro.", "Error", MB_OK);
      }
    }
    else
    {
      util::deleteFile(zipPath);
    }

    for (auto& core : s_coreInfos)
    {
      if (core.filename == coreName)
      {
        core.filetime = util::fileTime(path);
        modified = true;
      }
    }

    updateCores(hwnd);
  }

  void initControls(HWND hwnd) override
  {
    Dialog::initControls(hwnd);

    updateCores(hwnd);
  }

  INT_PTR dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
  {
    switch (msg)
    {
      case WM_COMMAND:
        if (HIWORD(wparam) == CBN_SELCHANGE)
        {
          updateCores(hwnd);
          return 0;
        }

        int cmd = LOWORD(wparam);
        if (cmd >= 51300 && cmd < 51500)
        {
          if (cmd >= 51400)
          {
            if (coreNames[cmd - 51400] == *loadedCore)
            {
              MessageBox(hwnd, "Cannot delete active core", "Error", MB_OK);
              return 0;
            }

            deleteCore(hwnd, coreNames[cmd - 51400]);
          }
          else
          {
            if (coreNames[cmd - 51300] == *loadedCore)
            {
              MessageBox(hwnd, "Cannot update active core", "Error", MB_OK);
              return 0;
            }

            HWND hUpdate = GetDlgItem(hwnd, cmd);
            SetWindowText(hUpdate, "Downloading...");
            EnableWindow(hwnd, FALSE);

            updateCore(hwnd, coreNames[cmd - 51300]);

            EnableWindow(hwnd, TRUE);
          }

          return 0;
        }
        break;
    }

    return Dialog::dialogProc(hwnd, msg, wparam, lparam);
  }
};

static const char* s_getCoreName(int index, void* udata)
{
  auto db = (CoreDialog*)udata;
  if (index < db->numSystems)
    return getSystemName(db->systemIds[index]);

  return NULL;
}

static void getCoreSystemTimes(Config* config, Logger* logger)
{
  std::string path = config->getRootFolder();
  path += "Cores\\index.txt";
  const time_t now = time(NULL);
  const time_t lastCheck = util::fileTime(path);
  if (now - lastCheck > 60 * 60 * 24) // 24 hours
  {
    std::string url = BUILDBOT_URL;
    url += ".index-extended";
    util::downloadFile(logger, url, path);
  }

  const std::string index = util::loadFile(logger, path);

  const char* dateStart = index.c_str();
  while (*dateStart)
  {
    const char* fileStart = dateStart + 20;
    const char* fileEnd = fileStart;
    while (*fileEnd && *fileEnd != '.')
      ++fileEnd;

    std::string coreName(fileStart, fileEnd - fileStart);
    for (auto& coreInfo : s_coreInfos)
    {
      if (coreInfo.filename == coreName)
      {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));

        int y, m, d;
        sscanf(dateStart, "%d-%d-%d", &y, &m, &d);
        tm.tm_year = y - 1900;
        tm.tm_mon = m - 1;
        tm.tm_mday = d;
        tm.tm_isdst = -1;

        coreInfo.servertime = mktime(&tm);
      }
    }

    dateStart = fileEnd;
    while (*dateStart && *dateStart != '\n')
      ++dateStart;
    if (*dateStart == '\n')
      ++dateStart;
  }
}

bool showCoresDialog(Config* config, Logger* logger, const std::string& loadedCore, int selectedSystem)
{
  CoreDialog db;
  db.init("Manage Cores");
  db.config = config;
  db.logger = logger;
  db.loadedCore = &loadedCore;

  getCoreSystemTimes(config, logger);

  std::map<std::string, int> allSystems;
  int systemCoreCounts[NumConsoleIDs];
  memset(systemCoreCounts, 0, sizeof(systemCoreCounts));
  int maxSystemCoreCount = 0;

  for (const auto& core : s_coreInfos)
  {
    if (!core.deprecationMessage.empty() && !core.filetime)
      continue;

    for (auto system : core.systems)
    {
      allSystems.emplace(getSystemName(system), system);
      if ((int)system < (int)NumConsoleIDs)
      {
        if (++systemCoreCounts[(int)system] > maxSystemCoreCount)
          maxSystemCoreCount = systemCoreCounts[(int)system];
      }
    }
  }

  int selectedCoreIndex = 0;

  // allSystems is a std::map keyed on the system name. iterating it will result in an alphabetically sorted list.
  for (const auto& pair : allSystems)
  {
    if (pair.second == selectedSystem)
      selectedCoreIndex = db.numSystems;

    db.systemIds[db.numSystems++] = pair.second;
  }

  db.coreNames.resize(maxSystemCoreCount);

  const DWORD WIDTH = 420;
  WORD y = 0;

  db.addCombobox(50000, 0, 0, 200, 16, 120, s_getCoreName, &db, &selectedCoreIndex);
  y += 20;

  db.addLabel("Local", 170, y, 50, 14);
  db.addLabel("Server", 230, y, 50, 14);
  y += 10;

  for (int i = 0; i < maxSystemCoreCount; ++i)
  {
    db.addLabel("Core Name", 51000 + i, 0, y + 3, 160, 14);
    db.addLabel("Local Time", 51100 + i, 170, y + 3, 50, 14);
    db.addLabel("Server Time", 51200 + i, 230, y + 3, 50, 14);
    db.addButton("Update", 51300 + i, 290, y, 60, 14, false);
    db.addButton("Delete", 51400 + i, 360, y, 60, 14, false);
    y += 18;
  }

  db.addButton("OK", IDOK, WIDTH - 50, y, 50, 14, true);

  db.show();

  return db.modified;
}

#endif
