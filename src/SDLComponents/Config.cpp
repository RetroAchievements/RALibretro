#include "Config.h"

#include "Dialog.h"

#include <string.h>

bool Config::init(libretro::LoggerComponent* logger)
{
  (void)logger;
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
  return "./";
}

const char* Config::getSaveDirectory()
{
  return "./Saves/";
}

const char* Config::getSystemPath()
{
  return "./";
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

void Config::setFromJson(const rapidjson::Value& json)
{
  for (auto& var: _variables)
  {
    const rapidjson::Value& value = json[var._key.c_str()];
    int selected = 0;

    for (const auto& option: var._options)
    {
      if (value.GetString() == option)
      {
        break;
      }

      selected++;
    }

    if ((size_t)selected >= var._options.size())
    {
      selected = 0;
    }

    _updated = _updated || selected != var._selected;
    var._selected = selected;
  }
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
    db.addCombobox(50000 + id, WIDTH / 3 + 5, y, WIDTH - WIDTH / 3 - 5, LINE, 5, getOption, (void*)&var._options, &var._selected);

    y += LINE;
    id++;
  }

  db.addButton("OK", IDOK, WIDTH - 55 - 50, y, 50, 14, true);
  db.addButton("Cancel", IDCANCEL, WIDTH - 50, y, 50, 14, false);

  _updated = db.show();
}

const char* Config::getOption(int index, void* udata)
{
  auto options = (std::vector<std::string>*)udata;

  if ((size_t)index < options->size())
  {
    return options->at(index).c_str();
  }

  return NULL;
}
