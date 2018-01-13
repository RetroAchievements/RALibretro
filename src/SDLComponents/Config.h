#pragma once

#include "libretro/Components.h"

#include <windows.h>
#include <rapidjson/document.h>

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

  void setFromJson(const rapidjson::Value& json);
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
  static const char* getOption(int index, void* udata);
  
  struct Variable
  {
    std::string _key;
    std::string _name;
    int         _selected;

    std::vector<std::string> _options;
  };

  std::vector<Variable> _variables;

  bool _preserveAspect;
  bool _linearFilter;

  bool _updated;
};
