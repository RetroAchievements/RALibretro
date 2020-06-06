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

#include "Util.h"
#include "jsonsax/jsonsax.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include <sys/stat.h>
#include <unordered_map>

#define TAG "[CFG] "

bool Config::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

#ifdef _WINDOWS
  HMODULE hModule = GetModuleHandleW(NULL);
  WCHAR path[MAX_PATH + 1];
  DWORD len = GetModuleFileNameW(hModule, path, sizeof(path) / sizeof(path[0]) - 1);
  path[len] = 0;

  _rootFolder = util::ucharToUtf8(path);
#endif

  size_t index = _rootFolder.find_last_of('\\');
  if (index != std::string::npos)
  {
    _rootFolder.resize(index + 1);
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

  _logger->info(TAG "Root folder:        %s", _rootFolder.c_str());
  _logger->info(TAG "Assets folder:      %s", _assetsFolder.c_str());
  _logger->info(TAG "Save folder:        %s", _saveFolder.c_str());
  _logger->info(TAG "System folder:      %s", _systemFolder.c_str());
  _logger->info(TAG "Screenshots folder: %s", _screenshotsFolder.c_str());

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
          _logger->info(TAG "Variable %s found in selections, set to \"%s\"", var._key.c_str(), found->second.c_str());
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
    _logger->info(TAG "Variable %s is \"%s\"", variable, value);
    return value;
  }

  for (const auto& var : _variables)
  {
    if (var._key == variable)
    {
      const char* value = var._options[var._selected].c_str();
      _logger->warn(TAG "Variable %s not found in the selections, returning \"%s\"", variable, value);
      return value;
    }
  }

  _logger->error(TAG "Variable %s not found", variable);
  return NULL;
}

std::string Config::serialize()
{
  for (const auto& var : _variables)
  {
    const auto found = _selections.find(var._key);

    if (found == _selections.cend())
    {
      _selections[var._key] = var._options[var._selected];
    }
  }

  std::string json("{");
  const char* comma = "";

  for (const auto& pair : _selections)
  {
    if (pair.first[0] == '_' && pair.first[1] == '_')
      continue;

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
      ud->self->_logger->info(TAG "Selection %s deserialized as \"%s\"", ud->key.c_str(), option.c_str());
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

void Config::initializeControllerVariable(Variable& variable, const char* name, const char* key, const std::map<std::string, unsigned>& names, unsigned selectedDevice)
{
  variable._name = name;
  variable._key = key;
  variable._selected = 0;

  for (const auto& pair : names)
  {
    if (pair.second == selectedDevice)
      variable._selected = variable._options.size();

    variable._options.push_back(pair.first);
  }
}

#ifdef _WINDOWS
void Config::showDialog(const std::string& coreName, Input& input)
{
  const WORD HEADER_WIDTH = 90;
  const WORD VALUE_WIDTH = 100;
  const WORD LINE_HEIGHT = 15;

  std::string title = coreName + " Settings";

  Dialog db;
  db.init(title.c_str());

  WORD x = 0;
  WORD y = 0;

  std::vector<Variable> variables;
  Variable controllerVariable;

  for (unsigned i = 2; i > 0; --i)
  {
    controllerVariable._key = "__controller" + std::to_string(i);
    controllerVariable._name = "Controller " + std::to_string(i);

    controllerVariable._options.clear();
    input.getControllerNames(i - 1, controllerVariable._options, controllerVariable._selected);
    if (controllerVariable._options.size() > 1)
      variables.insert(variables.begin(), controllerVariable);
  }

  if (_variables.empty() && variables.empty())
  {
    db.addLabel("No settings", 0, 0, HEADER_WIDTH + VALUE_WIDTH, LINE_HEIGHT);
    y = LINE_HEIGHT;
  }
  else
  {
    const WORD MAX_ROWS = 16;
    WORD row = 0;
    WORD id = 0;

    variables.insert(variables.end(), _variables.begin(), _variables.end());

    // evenly distribute the variables across multiple columns so no more than MAX_ROWS rows exist
    const WORD columns = ((WORD)variables.size() + MAX_ROWS - 1) / MAX_ROWS;
    const WORD rows = ((WORD)variables.size() + columns - 1) / columns;

    for (auto& var : variables)
    {
      db.addLabel(var._name.c_str(), x, y + 2, HEADER_WIDTH - 5, 8);
      db.addCombobox(50000 + id, x + HEADER_WIDTH + 5, y, VALUE_WIDTH, LINE_HEIGHT, 100,
        s_getOption, (void*)& var._options, &var._selected);

      if (++id < variables.size())
      {
        if (++row == rows)
        {
          y = 0;
          row = 0;
          x += HEADER_WIDTH + VALUE_WIDTH + 15;
        }
        else
        {
          y += LINE_HEIGHT;
        }
      }
    }

    y = rows * LINE_HEIGHT;
  }

  x += HEADER_WIDTH + VALUE_WIDTH + 15;

  db.addButton("OK", IDOK, x - 60 - 55, y + 4, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, x - 60, y + 4, 50, 14, false);

  _updated = db.show();

  if (_updated)
  {
    for (auto& var : variables)
    {
      if (var._key.length() == 13 && SDL_strncmp(var._key.c_str(), "__controller", 12) == 0)
      {
        const unsigned port = var._key[12] - '1';
        input.setSelectedControllerIndex(port, var._selected);
      }
      else
      {
        _selections[var._key] = var._options[var._selected];

        // remember the selection for next time
        for (auto& actualVar : _variables)
        {
          if (actualVar._key == var._key)
          {
            actualVar._selected = var._selected;
            break;
          }
        }
      }
    }
  }
}
#endif

const char* Config::s_getOption(int index, void* udata)
{
  auto options = (std::vector<std::string>*)udata;

  if ((size_t)index < options->size())
  {
    return options->at(index).c_str();
  }

  return NULL;
}
