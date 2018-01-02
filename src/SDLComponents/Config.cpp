#include "Config.h"

#include <string.h>

extern HWND g_mainWindow;

bool Config::init(libretro::LoggerComponent* logger)
{
  (void)logger;
  return true;
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
    while (isspace(*++aux)) /* nothing */;

    var._name = std::string(variables->value, aux - variables->value);

    while (isspace(*aux))
    {
      aux++;
    }

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
  return false;
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

void Config::showDialog()
{
  DialogBoxParam(NULL, "SETTINGS", g_mainWindow, s_dialogProc, (LPARAM)this);
}

INT_PTR CALLBACK Config::s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  Config* self;

  switch (msg)
  {
  case WM_INITDIALOG:
    self = (Config*)lparam;
    self->addControls(hwnd);
    return TRUE;
  
  case WM_SETFONT:
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)
    {
      EndDialog(hwnd, LOWORD(wparam) == IDOK);
      return TRUE;
    }

    break;

  case WM_CLOSE:
    DestroyWindow(hwnd);
    return TRUE;
  }

  return FALSE;
}

void Config::addControls(HWND hwnd)
{
  //  LTEXT           "Option 1", 0, 9, 36, 28, 9, SS_LEFT, WS_EX_LEFT
  //  COMBOBOX        0, 105, 35, 171, 12, CBS_DROPDOWN | CBS_HASSTRINGS, WS_EX_LEFT

  int y = 9;

  for (auto it = _variables.begin(); it != _variables.end(); ++it, y += 18)
  {
    HWND label = CreateWindowEx(
      WS_EX_LEFT,        // dwExStyle,
      NULL,              // lpClassName,
      it->_name.c_str(), // lpWindowName,
      SS_LEFT,           // dwStyle,
      9,                 // x
      y,                 // y,
      28,                // nWidth,
      9,                 // nHeight,
      hwnd,              // hWndParent,
      NULL,              // hMenu,
      NULL,              // hInstance,
      NULL               // lpParam
    );
  }
}
