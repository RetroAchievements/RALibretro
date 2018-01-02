#pragma once

#include "libretro/Components.h"

#include <windows.h>

class Config: public libretro::ConfigComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy() {}

  virtual const char* getCoreAssetsDirectory() override;
  virtual const char* getSaveDirectory() override;
  virtual const char* getSystemPath() override;

  virtual void setVariables(const struct retro_variable* variables, unsigned count) override;
  virtual bool varUpdated() override;
  virtual const char* getVariable(const char* variable) override;

  void showDialog();

protected:
  struct Variable
  {
    std::string _key;
    std::string _name;
    int         _selected;

    std::vector<std::string> _options;
  };

  std::vector<Variable> _variables;

  static INT_PTR CALLBACK s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

  void addControls(HWND hwnd);
};
