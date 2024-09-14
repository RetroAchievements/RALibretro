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

#pragma once

#ifdef _WINDOWS
 #include "components/Input.h"
#endif

#include "libretro/Components.h"

#include <map>
#include <unordered_map>

class Config: public libretro::ConfigComponent
{
public:
  void initRootFolder();
  bool init(libretro::LoggerComponent* logger);
  void destroy() {}
  void reset();

  virtual const char* getCoreAssetsDirectory() override;
  virtual const char* getSaveDirectory() override;
  virtual const char* getSystemPath() override;

  virtual void setVariables(const struct retro_variable* variables, unsigned count) override;
  virtual void setVariables(const struct retro_core_option_definition* options, unsigned count) override;
  virtual void setVariables(const struct retro_core_option_v2_definition* options, unsigned count,
    const struct retro_core_option_v2_category* categories, unsigned category_count) override;
  virtual void setVariableDisplay(const struct retro_core_option_display* display) override;
  virtual bool varUpdated() override;
  virtual const char* getVariable(const char* variable) override;

  virtual bool getBackgroundInput() override { return _backgroundInput; }
  virtual void setBackgroundInput(bool value) override { _backgroundInput = value; }

  virtual bool getFastForwarding() override { return _fastForwarding; }
  virtual void setFastForwarding(bool value) override { _fastForwarding = value; }

  virtual bool getAudioWhileFastForwarding() override { return _audioWhileFastForwarding; }
  virtual int getFastForwardRatio() override { return _fastForwardRatio; }

  virtual bool getShowSpeedIndicator() override { return _showSpeedIndicator; }
  virtual void setShowSpeedIndicator(bool value) override { _showSpeedIndicator = value; }

  void setSaveDirectory(const std::string& path) { _saveFolder = path; }

  const char* getRootFolder()
  {
    return _rootFolder.c_str();
  }
  
  const char* getScreenshotsFolder()
  {
    return _screenshotsFolder.c_str();
  }

  std::string serialize();
  void deserialize(const char* json);

  bool validateSettingsForHardcore(const char* library_name, int console_id, bool prompt);

  std::string serializeEmulatorSettings() const;
  bool deserializeEmulatorSettings(const char* json);

#ifdef _WINDOWS
  void showDialog(const std::string& coreName, Input& input);
  void showEmulatorSettingsDialog();
#endif

protected:
  static const char* s_getOption(int index, void* udata);

  struct Category
  {
    std::string _key;
    std::string _name;
    std::string _description;
    int         _visibleCount;
  };

  struct Variable
  {
    std::string _key;
    std::string _name;
    int         _selected;
    bool        _hidden;

    Category*   _category;

    std::vector<std::string> _options;
    std::vector<std::string> _labels;
  };

  class ConfigDialog : public Dialog
  {
  public:
    std::vector<Variable*> variables;
    std::vector<int> selections;
    std::vector<std::string> categoryNames;
    Config* owner;
    int maxCount;
    Category* category;
    bool updated;

    void updateVariables();

  protected:
    void initControls(HWND hwnd) override;

    INT_PTR dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    void retrieveData(HWND hwnd) override;

    HWND hwnd;
  };

  static void initializeControllerVariable(Variable& variable, const char* name, const char* key, const std::map<std::string, unsigned>& names, unsigned selectedDevice);
  void setOptions(Variable& var, const retro_core_option_value* values, const char* default_value);

  libretro::LoggerComponent* _logger;

  std::string _rootFolder;
  std::string _assetsFolder;
  std::string _saveFolder;
  std::string _systemFolder;
  std::string _screenshotsFolder;

  std::vector<Category> _categories;
  std::vector<Variable> _variables;
  std::unordered_map<std::string, std::string> _selections;

  bool _updated;
  bool _fastForwarding;
  bool _hadDisallowedSetting;
  bool _audioWhileFastForwarding;
  bool _backgroundInput;
  bool _showSpeedIndicator;

  int _fastForwardRatio;

  std::string _key;
};
