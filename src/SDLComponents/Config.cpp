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

bool Config::init(libretro::LoggerComponent* logger)
{
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

  logger->printf(RETRO_LOG_INFO, "Root folder:   %s", _rootFolder.c_str());
  logger->printf(RETRO_LOG_INFO, "Assets folder: %s", _assetsFolder.c_str());
  logger->printf(RETRO_LOG_INFO, "Save folder:   %s", _saveFolder.c_str());
  logger->printf(RETRO_LOG_INFO, "System folder: %s", _systemFolder.c_str());

  // TODO This should be done in main.cpp as soon as possible
  SetCurrentDirectory(_rootFolder.c_str());

  reset();
  return true;
}

void Config::reset()
{
  _preserveAspect = false;
  _linearFilter = false;

  _variables.clear();
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

    _variables.push_back(var);
  }
}

bool Config::varUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

const char* Config::getVariable(const char* variable)
{
  for (auto it = _variables.begin(); it != _variables.end(); ++it)
  {
    const Variable& var = *it;

    if (var._key == variable)
    {
      return var._options[var._selected].c_str();
    }
  }

  return NULL;
}

std::string Config::serialize()
{
  std::string json("{");

  for (auto& var: _variables)
  {
    json.append("\"");
    json.append(var._key);
    json.append("\":\"");
    json.append(var._options[var._selected]);
    json.append("\",");
  }

  json.append("\"_preserveAspect\":");
  json.append(_preserveAspect ? "true" : "false");
  json.append(",");

  json.append("\"_linearFilter\":");
  json.append(_linearFilter ? "true" : "false");

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
      std::string opt = std::string(str, num);

      for (auto& var: ud->self->_variables)
      {
        if (var._key == ud->key)
        {
          int selected = 0;

          for (const auto& option: var._options)
          {
            if (option == opt)
            {
              break;
            }

            selected++;
          }

          if ((size_t)selected >= var._options.size())
          {
            selected = 0;
          }

          ud->self->_updated = ud->self->_updated || selected != var._selected;
          var._selected = selected;
          break;
        }
      }
    }
    else if (event == JSONSAX_BOOLEAN)
    {
      if (ud->key == "_preserveAspect")
      {
        bool val = num != 0;
        ud->self->_updated = ud->self->_updated || ud->self->_preserveAspect != val;
        ud->self->_preserveAspect = val;
      }
      if (ud->key == "_linearFilter")
      {
        bool val = num != 0;
        ud->self->_updated = ud->self->_updated || ud->self->_linearFilter != val;
        ud->self->_linearFilter = val;
      }
    }

    return 0;
  });
}

void Config::showDialog()
{
  const WORD WIDTH = 280;
  const WORD LINE = 15;

  Dialog db;
  db.init("Settings");

  WORD y = 0;

  db.addCheckbox("Preserve aspect ratio", 51001, 0, y, WIDTH / 2 - 5, 8, &_preserveAspect);
  db.addCheckbox("Linear filtering", 51002, WIDTH / 2 + 5, y, WIDTH / 2 - 5, 8, &_linearFilter);
  y += LINE;

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
