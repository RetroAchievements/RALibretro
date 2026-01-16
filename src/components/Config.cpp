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

#include "Config.h"

#include "Util.h"
#include "jsonsax/jsonsax.h"
#include "RA_Interface.h"

#include <rcheevos/src/rc_libretro.h>
#include <rcheevos/include/rc_consoles.h>

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

  // these settings are global and should not be modified by reset()
  _audioWhileFastForwarding = true;
  _fastForwardRatio = 5;
  _backgroundInput = false;
  _showSpeedIndicator = true;
  _gameFocusCaptureMouse = false;

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
  _categories.clear();

  for (unsigned i = 0; i < count; variables++, i++)
  {
    Variable var;
    var._key = variables->key;

    const char* aux = strchr(variables->value, ';');
    var._name = std::string(variables->value, aux - variables->value);
    while (isspace(*++aux)) /* nothing */;

    var._selected = 0;
    var._hidden = 0;
    var._category = nullptr;

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
  _categories.clear();

  for (unsigned i = 0; i < count; options++, i++)
  {
    Variable var;
    var._key = options->key;
    var._name = options->desc;

    setOptions(var, options->values, options->default_value);

    _variables.push_back(var);
  }

  _updated = true;
}

void Config::setOptions(Variable& var, const retro_core_option_value* values, const char* default_value)
{
  var._selected = 0;
  var._hidden = 0;
  var._category = nullptr;

  bool hasLabels = false;
  for (unsigned j = 0; values[j].value != NULL; j++)
  {
    if (values[j].label)
    {
      hasLabels = true;
      break;
    }
  }

  for (unsigned j = 0; values[j].value != NULL; j++)
  {
    const char* value = values[j].value;
    if (hasLabels)
    {
      const char* label = values[j].label;
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
  else if (default_value != NULL)
  {
    for (size_t i = 0; i < var._options.size(); i++)
    {
      if (var._options[i] == default_value)
      {
        var._selected = i;
        _logger->info(TAG "Variable %s not found in selections, using default \"%s\"", var._key.c_str(), default_value);
        break;
      }
    }
  }
}

void Config::setVariables(const struct retro_core_option_v2_definition* options, unsigned count,
  const struct retro_core_option_v2_category* categories, unsigned category_count)
{
  _variables.clear();
  _categories.clear();

  for (unsigned i = 0; i < category_count; categories++, i++)
  {
    Category category;
    category._key = categories->key;
    category._name = categories->desc;
    if (categories->info)
      category._description = categories->info;

    _categories.push_back(category);
  }

  for (unsigned i = 0; i < count; options++, i++)
  {
    Variable var;
    var._key = options->key;

    if (options->desc_categorized)
      var._name = options->desc_categorized;
    else
      var._name = options->desc;

    setOptions(var, options->values, options->default_value);

    if (options->category_key)
    {
      for (auto& category : _categories)
      {
        if (category._key == options->category_key)
        {
          var._category = &category;
          break;
        }
      }
    }
    else
    {
      var._category = nullptr;
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

bool Config::validateSettingsForHardcore(const char* library_name, int console_id, bool prompt)
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

  if (RA_HardcoreModeIsActive() && !rc_libretro_is_system_allowed(library_name, console_id))
  {
    char message[256];
    if (prompt)
    {
      snprintf(message, sizeof(message), "earn achievements for %s using %s", rc_console_name(console_id), library_name);
      if (!RA_WarnDisableHardcore(message))
        return false;
    }
    else
    {
      snprintf(message, sizeof(message), "You cannot earn hardcore achievements for %s using %s", rc_console_name(console_id), library_name);
      MessageBox(g_mainWindow, message, "Error", MB_OK);
      return false;
    }
  }

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

std::string Config::serializeEmulatorSettings() const
{
  std::string json("{");

  json.append("\"audioWhileFastForwarding\":");
  json.append(_audioWhileFastForwarding ? "true" : "false");
  json.append(",");

  json.append("\"fastForwardRatio\":");
  json.append(std::to_string(_fastForwardRatio));
  json.append(",");

  json.append("\"backgroundInput\":");
  json.append(_backgroundInput ? "true" : "false");
  json.append(",");

  json.append("\"showSpeedIndicator\":");
  json.append(_showSpeedIndicator ? "true" : "false");
  json.append(",");

  json.append("\"gameFocusCaptureMouse\":");
  json.append(_gameFocusCaptureMouse ? "true" : "false");

  json.append("}");
  return json;
}

bool Config::deserializeEmulatorSettings(const char* json)
{
  struct Deserialize
  {
    Config* self;
    std::string key;
  };

  Deserialize ud;
  ud.self = this;

  jsonsax_result_t res = jsonsax_parse(json, &ud, [](void* udata, jsonsax_event_t event, const char* str, size_t num)
  {
    auto ud = (Deserialize*)udata;

    if (event == JSONSAX_KEY)
    {
      ud->key = std::string(str, num);
    }
    else if (event == JSONSAX_BOOLEAN)
    {
      if (ud->key == "audioWhileFastForwarding" || ud->key == "_audioWhileFastForwarding")
      {
        ud->self->_audioWhileFastForwarding = num != 0;
      }
      else if (ud->key == "backgroundInput")
      {
        ud->self->_backgroundInput = num != 0;
      }
      else if (ud->key == "showSpeedIndicator")
      {
        ud->self->_showSpeedIndicator = num != 0;
      }
      else if (ud->key == "gameFocusCaptureMouse")
      {
        ud->self->_gameFocusCaptureMouse = num != 0;
      }
    }
    else if (event == JSONSAX_NUMBER)
    {
      if (ud->key == "fastForwardRatio" || ud->key == "_fastForwardRatio")
      {
        auto value = strtoul(str, NULL, 10);
        if (value < 2)
          value = 2;
        else if (value > 10)
          value = 10;
        ud->self->_fastForwardRatio = value;
      }
    }

    return 0;
  });

  return (res == JSONSAX_OK);
}

#ifdef _WINDOWS

void Config::ConfigDialog::updateVariables()
{
  int id = 0;
  for (size_t i = 0; i < variables.size(); ++i)
  {
    auto& var = variables[i];
    if (var->_category != category)
      continue;

    HWND hLabel = GetDlgItem(hwnd, 50000 + id);
    std::wstring unicodeString = util::utf8ToUChar(var->_name);
    SetWindowTextW(hLabel, unicodeString.c_str());
    ShowWindow(hLabel, SW_SHOW);

    HWND hComboBox = GetDlgItem(hwnd, 51000 + id);
    SendMessageW(hComboBox, CB_RESETCONTENT, 0, 0);

    for (size_t j = 0; j < var->_options.size(); ++j)
    {
      unicodeString = util::utf8ToUChar(var->_options[j]);
      SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)unicodeString.c_str());
    }

    SendMessageW(hComboBox, CB_SETCURSEL, selections[i], 0);

    ShowWindow(hComboBox, SW_SHOW);
    ++id;
  }

  for (; id < maxCount; ++id)
  {
    HWND hLabel = GetDlgItem(hwnd, 50000 + id);
    ShowWindow(hLabel, SW_HIDE);

    HWND hComboBox = GetDlgItem(hwnd, 51000 + id);
    ShowWindow(hComboBox, SW_HIDE);
  }
}

void Config::ConfigDialog::retrieveData(HWND hwnd)
{
  int id = 0;
  for (size_t i = 0; i < variables.size(); ++i)
  {
    auto& var = variables[i];
    if (var->_category != category)
      continue;

    HWND hComboBox = GetDlgItem(this->hwnd, 51000 + id);
    int selection = SendMessageW(hComboBox, CB_GETCURSEL, 0, 0);
    if (selection != selections[i])
    {
      selections[i] = selection;
      updated = true;
    }

    ++id;
  }

  if (hwnd != nullptr)
    _updated = updated;
}

void Config::ConfigDialog::initControls(HWND hwnd)
{
  this->hwnd = hwnd;

  if (owner->_categories.size() > 0)
  {
    HWND hComboBox = GetDlgItem(hwnd, 49999);
    SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"General");

    std::wstring unicodeString;
    for (const auto& category : owner->_categories)
    {
      unicodeString = util::utf8ToUChar(category._name);
      SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)unicodeString.c_str());
    }

    SendMessageW(hComboBox, CB_SETCURSEL, 0, 0);
  }

  updateVariables();
}

INT_PTR Config::ConfigDialog::dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  if (msg == WM_COMMAND && LOWORD(wparam) == 49999 && HIWORD(wparam) == CBN_SELCHANGE)
  {
    HWND hComboBox = GetDlgItem(hwnd, 49999);
    int selection = SendMessageW(hComboBox, CB_GETCURSEL, 0, 0);

    retrieveData(nullptr);

    if (selection == 0)
      category = nullptr;
    else
      category = &owner->_categories[selection - 1];

    updateVariables();
    return 0;
  }

  return Dialog::dialogProc(hwnd, msg, wparam, lparam);
}

void Config::showDialog(const std::string& coreName, Input& input)
{
  const WORD HEADER_WIDTH = 90;
  const WORD VALUE_WIDTH = 100;
  const WORD LINE_HEIGHT = 15;

  std::string title = coreName + " Settings";

  ConfigDialog db;
  db.init(title.c_str());
  db.owner = this;
  db.category = nullptr;
  db.updated = false;

  WORD x = 0;
  WORD y = 0;

  Variable controllerVariables[4];

  for (unsigned i = sizeof(controllerVariables) / sizeof(controllerVariables[0]); i > 0; --i)
  {
    auto& controllerVariable = controllerVariables[i - 1];
    controllerVariable._key = "__controller" + std::to_string(i);
    controllerVariable._name = "Controller " + std::to_string(i);
    controllerVariable._category = nullptr;

    controllerVariable._options.clear();
    input.getControllerNames(i - 1, controllerVariable._options, controllerVariable._selected);
    if (controllerVariable._options.size() > 1)
    {
      db.variables.insert(db.variables.begin(), &controllerVariable);
      db.selections.insert(db.selections.begin(), controllerVariable._selected);
    }
  }

  if (_variables.empty() && db.variables.empty())
  {
    db.addLabel("No settings", 0, 0, HEADER_WIDTH + VALUE_WIDTH, LINE_HEIGHT);
    y = LINE_HEIGHT;
  }
  else
  {
    const WORD MAX_ROWS = 16;
    WORD row = 0;
    WORD id = 0;
    int generalCount = db.variables.size();

    for (auto& category : _categories)
    {
      db.categoryNames.push_back(category._name);
      category._visibleCount = 0;
    }

    for (auto& var : _variables)
    {
      if (!var._hidden)
      {
        if (var._category)
          var._category->_visibleCount++;
        else
          generalCount++;

        db.variables.push_back(&var);
        db.selections.push_back(var._selected);
      }
    }

    db.maxCount = generalCount;
    if (_categories.size() > 0)
    {
      db.addCombobox(49999, x, y, VALUE_WIDTH + HEADER_WIDTH + 5, LINE_HEIGHT, 100, nullptr, nullptr, nullptr);
      y += 20;

      for (const auto& category : _categories)
      {
        if (category._visibleCount > db.maxCount)
          db.maxCount = category._visibleCount;
      }
    }

    // evenly distribute the variables across multiple columns so no more than MAX_ROWS rows exist
    WORD top = y;
    const WORD columns = ((WORD)db.maxCount + MAX_ROWS - 1) / MAX_ROWS;
    const WORD rows = ((WORD)db.maxCount + columns - 1) / columns;

    for (int i = 0; i < db.maxCount; ++i)
    {
      db.addLabel("Label", 50000 + id, x, y + 2, HEADER_WIDTH - 5, 8);
      db.addCombobox(51000 + id, x + HEADER_WIDTH + 5, y, VALUE_WIDTH, LINE_HEIGHT, 100,
        s_getOption, (void*)&db, &db.selections[i]);

      if (++id < db.maxCount)
      {
        if (++row == rows)
        {
          y = top;
          row = 0;
          x += HEADER_WIDTH + VALUE_WIDTH + 15;
        }
        else
        {
          y += LINE_HEIGHT;
        }
      }
    }

    y = top + rows * LINE_HEIGHT;
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
        for (unsigned i = 0; i < db.variables.size(); ++i)
        {
          auto& var = *db.variables[i];
          const char* value = var._options[db.selections[i]].c_str();
          if (!rc_libretro_is_setting_allowed(disallowed_settings, var._key.c_str(), value))
          {
            if (db.selections[i] < (int)var._labels.size())
              value = var._labels[db.selections[i]].c_str();

            std::string setting = "set " + var._name + " to " + value;
            if (!RA_WarnDisableHardcore(setting.c_str()))
              return;

            _hadDisallowedSetting = true;
          }
        }
      }
    }

    for (unsigned i = 0; i < db.variables.size(); ++i)
    {
      auto& var = *db.variables[i];

      if (var._key.length() == 13 && SDL_strncmp(var._key.c_str(), "__controller", 12) == 0)
      {
        const unsigned port = var._key[12] - '1';
        input.setSelectedControllerIndex(port, db.selections[i]);
      }
      else
      {
        // keep track of the selected item
        var._selected = db.selections[i];

        // and selected value
        _selections[var._key] = var._options[var._selected];
      }
    }
  }
}

static const char* s_getFastForwardRatioOptions(int index, void* udata)
{
  switch (index)
  {
    case 0: return "2x";
    case 1: return "3x";
    case 2: return "4x";
    case 3: return "5x";
    case 4: return "6x";
    case 5: return "7x";
    case 6: return "8x";
    case 7: return "9x";
    case 8: return "10x";
    default: return NULL;
  }
}

void Config::showEmulatorSettingsDialog()
{
  const WORD WIDTH = 170;
  const WORD LINE = 15;

  Dialog db;
  db.init("Emulator Settings");

  WORD y = 0;

  int fastForwardRatio = _fastForwardRatio - 2;
  db.addLabel("Fast Forward Ratio", 51003, 0, y, 50, 8);
  db.addCombobox(51001, 55, y - 2, WIDTH - 55, 12, 100, s_getFastForwardRatioOptions, NULL, &fastForwardRatio);
  y += LINE;

  bool playAudio = _audioWhileFastForwarding;
  db.addCheckbox("Play Audio while Fast Forwarding", 51002, 0, y, WIDTH - 10, 8, &playAudio);
  y += LINE;

  bool showSpeedIndicator = _showSpeedIndicator;
  db.addCheckbox("Show Indicator when Paused or Fast Forwarding", 51004, 0, y, WIDTH - 10, 8, &showSpeedIndicator);
  y += LINE;

  bool gameFocusCaptureMouse = _gameFocusCaptureMouse;
  db.addCheckbox("Capture mouse in Game Focus mode", 51005, 0, y, WIDTH - 10, 8, &gameFocusCaptureMouse);
  y += LINE;

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  if (db.show())
  {
    _audioWhileFastForwarding = playAudio;
    _fastForwardRatio = fastForwardRatio + 2;
    _showSpeedIndicator = showSpeedIndicator;
    _gameFocusCaptureMouse = gameFocusCaptureMouse;
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
