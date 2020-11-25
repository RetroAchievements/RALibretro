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
#include "cheevos_libretro.h"
#include "RA_Interface.h"

#ifdef _WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern HWND g_mainWindow;
#endif

#include <string.h>
#include <sys/stat.h>
#include <unordered_map>

#define TAG "[CFG] "

void Config::initRootFolder()
{
#ifdef _WINDOWS
  WCHAR path[MAX_PATH + 1];
  DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
  path[len] = 0;

  WCHAR fullpath[MAX_PATH + 1];
  len = GetFullPathNameW(path, MAX_PATH, fullpath, NULL);

  _rootFolder = util::ucharToUtf8(fullpath);
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
}

bool Config::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

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
  _hadDisallowedSetting = false;
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
    var._hidden = 0;

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

void Config::setVariables(const struct retro_core_option_definition* options, unsigned count)
{
  _variables.clear();

  for (unsigned i = 0; i < count; options++, i++)
  {
    Variable var;
    var._key = options->key;
    var._name = options->desc;
    var._selected = 0;
    var._hidden = 0;

    bool hasLabels = false;
    for (unsigned j = 0; options->values[j].value != NULL; j++)
    {
      if (options->values[j].label)
      {
        hasLabels = true;
        break;
      }
    }

    for (unsigned j = 0; options->values[j].value != NULL; j++)
    {
      const char* value = options->values[j].value;
      if (hasLabels)
      {
        const char* label = options->values[j].label;
        if (label == NULL)
          label = value;

        var._labels.push_back(label);
      }

      var._options.push_back(value);
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
    else if (options->default_value != NULL)
    {
      for (size_t i = 0; i < var._options.size(); i++)
      {
        if (var._options[i] == options->default_value)
        {
          var._selected = i;
          _logger->info(TAG "Variable %s not found in selections, using default \"%s\"", var._key.c_str(), options->default_value);
          break;
        }
      }
    }

    _variables.push_back(var);
  }

  _updated = true;
}

void Config::setVariableDisplay(const struct retro_core_option_display* display)
{
  for (size_t i = 0; i < _variables.size(); i++)
  {
    if (_variables[i]._key == display->key)
    {
      _variables[i]._hidden = !display->visible;
      _logger->info(TAG "Setting visibility on variable %s to %s", display->key, display->visible ? "visible" : "hidden");
      return;
    }
  }

  _logger->info(TAG "Could not set visibility on unknown variable %s", display->key);
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

bool Config::validateSettingsForHardcore(const char* library_name, bool prompt)
{
#ifdef _WINDOWS
  // previously detected a disallowed setting and not trying to enable hardcore, return success
  if (_hadDisallowedSetting && !RA_HardcoreModeIsActive())
    return true;


  const rc_disallowed_setting_t* disallowed_settings = rc_libretro_get_disallowed_settings(library_name);
  if (disallowed_settings)
  {
    bool hadDisallowedSetting = _hadDisallowedSetting;
    for (auto& var : _variables)
    {
      const char* value = var._options[var._selected].c_str();
      if (!rc_libretro_is_setting_allowed(disallowed_settings, var._key.c_str(), value))
      {
        if (RA_HardcoreModeIsActive())
        {
          // hardcore mode - setting is not allowed. if prompt is true, give the user the chance
          // to disable hardcore mode. if they choose not to, or prompt is false, return false
          if (var._selected < (int)var._labels.size())
            value = var._labels[var._selected].c_str();

          if (prompt)
          {
            std::string setting = "set " + var._name + " to " + value;
            if (!RA_WarnDisableHardcore(setting.c_str()))
              return false;
          }
          else
          {
            std::string error = var._name + " cannot be set to " + value + " in hardcore mode.";
            MessageBox(g_mainWindow, error.c_str(), "Error", MB_OK);
            return false;
          }
        }

        // non hardcore mode - just track the fact that a disallowed setting was set
        _hadDisallowedSetting = true;
        break;
      }
    }

    for (auto& var : _selections)
    {
      if (!rc_libretro_is_setting_allowed(disallowed_settings, var.first.c_str(), var.second.c_str()))
      {
        if (RA_HardcoreModeIsActive())
        {
          if (prompt)
          {
            std::string setting = "set " + var.first + " to " + var.second;
            if (!RA_WarnDisableHardcore(setting.c_str()))
              return false;
          }
          else
          {
            std::string error = var.first + " cannot be set to " + var.second + " in hardcore mode.";
            MessageBox(g_mainWindow, error.c_str(), "Error", MB_OK);
            return false;
          }
        }

        // non hardcore mode - just track the fact that a disallowed setting was set
        _hadDisallowedSetting = true;
        break;
      }
    }

    if (hadDisallowedSetting && RA_HardcoreModeIsActive())
    {
      // no current disallowed setting, but one was previously set, prevent switch to hardcore
      MessageBox(g_mainWindow, "You must reload the game to enable hardcore mode due to a core configuration change", "Error", MB_OK);
      return false;
    }
  }
#endif

  return true;
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

  std::vector<Variable*> variables;
  std::vector<int> selections;
  Variable controllerVariables[4];

  for (unsigned i = sizeof(controllerVariables) / sizeof(controllerVariables[0]); i > 0; --i)
  {
    auto& controllerVariable = controllerVariables[i - 1];
    controllerVariable._key = "__controller" + std::to_string(i);
    controllerVariable._name = "Controller " + std::to_string(i);

    controllerVariable._options.clear();
    input.getControllerNames(i - 1, controllerVariable._options, controllerVariable._selected);
    if (controllerVariable._options.size() > 1)
      variables.insert(variables.begin(), &controllerVariable);
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

    for (auto& var : _variables)
    {
      if (!var._hidden)
        variables.push_back(&var);
    }

    selections.reserve(variables.size());

    // evenly distribute the variables across multiple columns so no more than MAX_ROWS rows exist
    const WORD columns = ((WORD)variables.size() + MAX_ROWS - 1) / MAX_ROWS;
    const WORD rows = ((WORD)variables.size() + columns - 1) / columns;

    for (unsigned i = 0; i < variables.size(); ++i)
    {
      auto& var = *variables[i];
      selections.push_back(var._selected);

      const auto* options = &var._options;
      if (!var._labels.empty())
        options = &var._labels;

      db.addLabel(var._name.c_str(), x, y + 2, HEADER_WIDTH - 5, 8);
      db.addCombobox(50000 + id, x + HEADER_WIDTH + 5, y, VALUE_WIDTH, LINE_HEIGHT, 100,
        s_getOption, (void*)options, &selections[i]);

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
    if (RA_HardcoreModeIsActive())
    {
      const rc_disallowed_setting_t* disallowed_settings = rc_libretro_get_disallowed_settings(coreName.c_str());
      if (disallowed_settings)
      {
        for (unsigned i = 0; i < variables.size(); ++i)
        {
          auto& var = *variables[i];
          const char* value = var._options[selections[i]].c_str();
          if (!rc_libretro_is_setting_allowed(disallowed_settings, var._key.c_str(), value))
          {
            if (selections[i] < (int)var._labels.size())
              value = var._labels[selections[i]].c_str();

            std::string setting = "set " + var._name + " to " + value;
            if (!RA_WarnDisableHardcore(setting.c_str()))
              return;

            _hadDisallowedSetting = true;
          }
        }
      }
    }

    for (unsigned i = 0; i < variables.size(); ++i)
    {
      auto& var = *variables[i];

      if (var._key.length() == 13 && SDL_strncmp(var._key.c_str(), "__controller", 12) == 0)
      {
        const unsigned port = var._key[12] - '1';
        input.setSelectedControllerIndex(port, selections[i]);
      }
      else
      {
        // keep track of the selected item
        var._selected = selections[i];

        // and selected value
        _selections[var._key] = var._options[var._selected];
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
