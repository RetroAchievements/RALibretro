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

#include "libretro/Components.h"

class Config: public libretro::ConfigComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy() {}
  void reset();

  virtual const char* getCoreAssetsDirectory() override;
  virtual const char* getSaveDirectory() override;
  virtual const char* getSystemPath() override;

  virtual void setVariables(const struct retro_variable* variables, unsigned count) override;
  virtual bool varUpdated() override;
  virtual const char* getVariable(const char* variable) override;

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
  void showDialog();

  bool preserveAspect()
  {
    return _preserveAspect;
  }

  bool linearFilter()
  {
    return _linearFilter;
  }

protected:
  static const char* s_getOption(int index, void* udata);

  struct Variable
  {
    std::string _key;
    std::string _name;
    int         _selected;

    std::vector<std::string> _options;
  };

  libretro::LoggerComponent* _logger;

  std::string _rootFolder;
  std::string _assetsFolder;
  std::string _saveFolder;
  std::string _systemFolder;
  std::string _screenshotsFolder;

  std::vector<Variable> _variables;

  bool _preserveAspect;
  bool _linearFilter;

  bool _updated;

  std::string _key;
};
