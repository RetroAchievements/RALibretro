#include "Config.h"

#include "Dialog.h"
#include "jsonsax/jsonsax.h"

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

void Config::unserialize(const char* json)
{
  jsonsax_handlers_t handlers;
  memset(&handlers, 0, sizeof(handlers));
  handlers.key = s_key;
  handlers.string = s_string;
  handlers.boolean = s_boolean;

  jsonsax_parse(json, &handlers, this);

  _key.clear();
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

int Config::s_key(void* userdata, const char* name, size_t length)
{
  auto self = (Config*)userdata;
  self->_key = std::string(name, length);
  return 0;
}

int Config::s_string(void* userdata, const char* string, size_t length)
{
  auto self = (Config*)userdata;
  std::string opt(string, length);

  for (auto& var: self->_variables)
  {
    if (var._key == self->_key)
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

      self->_updated = self->_updated || selected != var._selected;
      var._selected = selected;
      break;
    }
  }

  return 0;
}

int Config::s_boolean(void* userdata, int istrue)
{
  auto self = (Config*)userdata;

  if (self->_key == "_preserveAspect")
  {
    bool value = istrue != 0;
    self->_updated = self->_updated || self->_preserveAspect != value;
    self->_preserveAspect = value;
  }
  else if (self->_key == "_linearFilter")
  {
    bool value = istrue != 0;
    self->_updated = self->_updated || self->_linearFilter != value;
    self->_linearFilter = value;
  }

  return 0;
}
