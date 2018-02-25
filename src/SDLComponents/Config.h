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

  std::string _rootFolder;
  std::string _assetsFolder;
  std::string _saveFolder;
  std::string _systemFolder;

  std::vector<Variable> _variables;

  bool _preserveAspect;
  bool _linearFilter;

  bool _updated;

  std::string _key;
};
