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

#include "Config.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include <sys/stat.h>
#include <unordered_map>

bool Config::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

  HMODULE hModule = GetModuleHandleW(NULL);
  char path[MAX_PATH + 1];
  DWORD len = GetModuleFileNameA(hModule, path, sizeof(path) / sizeof(path[0]) - 1);
  path[len] = 0;

  char* bslash = strrchr(path, '\\');

  if (bslash != NULL) {
    bslash[1] = 0;
    _rootFolder = path;
  }
  else
  {
    _rootFolder = ".\\";
  }

  _assetsFolder = _rootFolder + "Assets\\";
  _saveFolder = _rootFolder + "Saves\\";
  _systemFolder = _rootFolder + "System\\";
  _screenshotsFolder = _rootFolder + "Screenshots\\";

#ifdef WIN32
 #define mkdir(path) CreateDirectory(path, NULL)
#endif

  mkdir(_assetsFolder.c_str());
  mkdir(_saveFolder.c_str());
  mkdir(_systemFolder.c_str());
  mkdir(_screenshotsFolder.c_str());

  _logger->printf(RETRO_LOG_INFO, "[CFG] Root folder:        %s", _rootFolder.c_str());
  _logger->printf(RETRO_LOG_INFO, "[CFG] Assets folder:      %s", _assetsFolder.c_str());
  _logger->printf(RETRO_LOG_INFO, "[CFG] Save folder:        %s", _saveFolder.c_str());
  _logger->printf(RETRO_LOG_INFO, "[CFG] System folder:      %s", _systemFolder.c_str());
  _logger->printf(RETRO_LOG_INFO, "[CFG] Screenshots folder: %s", _screenshotsFolder.c_str());

  // TODO This should be done in main.cpp as soon as possible
  SetCurrentDirectory(_rootFolder.c_str());

  reset();
  return true;
}

void Config::reset()
{
  _variables.clear();
  _selections.clear();
  _updated = false;
}

const char* Config::getCoreAssetsDirectory()
{
  return _assetsFolder.c_str();
}

const char* Config::getSaveDirectory()
{
  return _saveFolder.c_str();
}

const char* Config::getSystemPath()
{
  return _systemFolder.c_str();
}

void Config::setVariables(const struct retro_variable* variables, unsigned count)
{
  _variables.clear();

  for (unsigned i = 0; i < count; variables++, i++)
  {
    Variable var;
    var._key = variables->key;

    const char* aux = strchr(variables->value, ';');
    var._name = std::string(variables->value, aux - variables->value);
    while (isspace(*++aux)) /* nothing */;

    var._selected = 0;

    for (unsigned j = 0; aux != NULL; j++)
    {
      const char* pipe = strchr(aux, '|');
      std::string option;

      if (pipe != NULL)
      {
        option.assign(aux, pipe - aux);
        aux = pipe + 1;
      }
      else
      {
        option.assign(aux);
        aux = NULL;
      }

      var._options.push_back(option);
    }

    const auto& found = _selections.find(var._key);
    
    if (found != _selections.cend())
    {
      for (size_t i = 0; i < var._options.size(); i++)
      {
        if (var._options[i] == found->second)
        {
          var._selected = i;
          _logger->printf(RETRO_LOG_INFO, "[CFG] Variable %s found in selections, set to \"%s\"", var._key.c_str(), found->second.c_str());
          break;
        }
      }
    }

    _variables.push_back(var);
  }

  _updated = true;
}

bool Config::varUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

const char* Config::getVariable(const char* variable)
{
  const auto& found = _selections.find(variable);

  if (found != _selections.cend())
  {
    const char* value = found->second.c_str();
    _logger->printf(RETRO_LOG_INFO, "[CFG] Variable %s is \"%s\"", variable, value);
    return value;
  }

  _logger->printf(RETRO_LOG_ERROR, "[CFG] Variable %s not found", variable);
  return NULL;
}

std::string Config::serialize()
{
  std::string json("{");
  const char* comma = "";

  for (const auto& pair : _selections)
  {
    json.append(comma);
    comma = ",";

    json.append("\"");
    json.append(pair.first);
    json.append("\":\"");
    json.append(pair.second);
    json.append("\"");
  }

  json.append("}");
  return json;
}

void Config::deserialize(const char* json)
{
  struct Deserialize
  {
    Config* self;
    std::string key;
  };

  Deserialize ud;
  ud.self = this;

  jsonsax_parse(json, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num) {
    auto ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);
    }
    else if (event == JSONSAX_STRING)
    {
      std::string option = std::string(str, num);
      ud->self->_selections[ud->key] = option;
      ud->self->_logger->printf(RETRO_LOG_INFO, "[CFG] Selection %s deserialized as \"%s\"", ud->key.c_str(), option.c_str());
    }

    return 0;
  });

  for (auto& var : _variables)
  {
    const auto& found = _selections.find(var._key);

    if (found != _selections.cend())
    {
      for (size_t i = 0; i < var._options.size(); i++)
      {
        if (var._options[i] == found->second)
        {
          var._selected = i;
          break;
        }
      }
    }
  }

  _updated = true;
}

void Config::showDialog()
{
  const WORD WIDTH = 280;
  const WORD LINE = 15;

  Dialog db;
  db.init("Emulation Settings");

  WORD y = 0;
  DWORD id = 0;

  for (auto& var: _variables)
  {
    db.addLabel(var._name.c_str(), 0, y, WIDTH / 3 - 5, 8);
    db.addCombobox(50000 + id, WIDTH / 3 + 5, y, WIDTH - WIDTH / 3 - 5, LINE, 5, s_getOption, (void*)&var._options, &var._selected);

    y += LINE;
    id++;
  }

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  _updated = db.show();

  if (_updated)
  {
    for (auto& var : _variables)
    {
      _selections[var._key] = var._options[var._selected];
    }
  }
}

const char* Config::s_getOption(int index, void* udata)
{
  auto options = (std::vector<std::string>*)udata;

  if ((size_t)index < options->size())
  {
    return options->at(index).c_str();
  }

  return NULL;
}
