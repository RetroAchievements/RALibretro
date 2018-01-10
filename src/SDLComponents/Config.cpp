#include "Config.h"

#include <string.h>

extern HWND g_mainWindow;

class DialogBuilder
{
public:
  DialogBuilder()
  {
    _template = NULL;
    _size = 0;
    _reserved = 0;
  }

  ~DialogBuilder()
  {
    free(_template);
  }

  void align(size_t alignment)
  {
    alignment--;
    _size = (_size + alignment) & ~alignment;
  }

  void write(void* data, size_t size)
  {
    if (_size + size > _reserved)
    {
      size_t r = _reserved + 4096;
      void* t = realloc(_template, r);

      if (t == NULL)
      {
        return;
      }

      _template = t;
      _reserved = r;
    }

    memcpy((uint8_t*)_template + _size, data, size);
    _size += size;
  }

  template<typename T> void write(T t)
  {
    write(&t, sizeof(t));
  }

  void writeStr(const char* str)
  {
    int needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

    if (needed != 0)
    {
      needed *= sizeof(WCHAR);
      WCHAR* wide = (WCHAR*)alloca(needed);
      MultiByteToWideChar(CP_UTF8, 0, str, -1, wide, needed);
      write((void*)wide, needed);
    }
  }

  void writeWide(const WCHAR* str)
  {
    write((void*)str, (lstrlenW(str) + 1) * sizeof(WCHAR));
  }

  LPCDLGTEMPLATE get()
  {
    return (LPCDLGTEMPLATE)_template;
  }

protected:
  void*  _template;
  size_t _size;
  size_t _reserved;
};

bool Config::init(libretro::LoggerComponent* logger)
{
  (void)logger;
  reset();
  return true;
}

void Config::reset()
{
  _preserveAspect = true;
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

void Config::showDialog()
{
  NONCLIENTMETRICSW ncm;
  ncm.cbSize = sizeof(ncm);

  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0) == 0)
  {
    return;
  }

  if (ncm.lfMessageFont.lfHeight < 0)
  {
    HDC hdc = GetDC(NULL);

    if (!hdc)
    {
      return;
    }

    ncm.lfMessageFont.lfHeight = -MulDiv(ncm.lfMessageFont.lfHeight, 72, GetDeviceCaps(hdc, LOGPIXELSY));
    ReleaseDC(NULL, hdc);
  }

  // https://blogs.msdn.microsoft.com/oldnewthing/20050429-00
  DialogBuilder db;
  const WORD WIDTH = 280;
  const WORD LINE = 15;
  WORD y = 5;

  db.write<WORD>(1);
  db.write<WORD>(0xffff);
  db.write<DWORD>(0);
  db.write<DWORD>(WS_EX_WINDOWEDGE);
  db.write<DWORD>(DS_3DLOOK | DS_CENTER | DS_MODALFRAME | DS_SHELLFONT | WS_CAPTION | WS_VISIBLE | WS_POPUP);
  db.write<WORD>(4 + _variables.size() * 2);
  db.write<WORD>(0);
  db.write<WORD>(0);
  db.write<WORD>(WIDTH);
  db.write<WORD>((2 + _variables.size()) * LINE + 10);
  db.writeWide(L"");
  db.writeWide(L"");
  db.writeWide(L"Settings");

  db.write<WORD>((WORD)ncm.lfMessageFont.lfHeight);
  db.write<WORD>((WORD)ncm.lfMessageFont.lfWeight);
  db.write<BYTE>(ncm.lfMessageFont.lfItalic);
  db.write<BYTE>(ncm.lfMessageFont.lfCharSet);
  db.writeWide(ncm.lfMessageFont.lfFaceName);

  db.align(sizeof(DWORD));
  db.write<DWORD>(0);
  db.write<DWORD>(0);
  db.write<DWORD>(BS_AUTOCHECKBOX | WS_TABSTOP | WS_CHILD | WS_VISIBLE);
  db.write<WORD>(5);
  db.write<WORD>(y);
  db.write<WORD>(WIDTH / 2 - 10);
  db.write<WORD>(8);
  db.write<DWORD>(51001);
  db.write<DWORD>(0x0080ffff);
  db.writeWide(L"Preserve aspect ratio");
  db.write<WORD>(0);

  db.align(sizeof(DWORD));
  db.write<DWORD>(0);
  db.write<DWORD>(0);
  db.write<DWORD>(BS_AUTOCHECKBOX | WS_TABSTOP | WS_CHILD | WS_VISIBLE);
  db.write<WORD>(WIDTH / 2 + 5);
  db.write<WORD>(y);
  db.write<WORD>(WIDTH / 2 - 10);
  db.write<WORD>(8);
  db.write<DWORD>(51002);
  db.write<DWORD>(0x0080ffff);
  db.writeWide(L"Linear filtering");
  db.write<WORD>(0);
  y += LINE;

  DWORD id = 0;

  for (auto var: _variables)
  {
    db.align(sizeof(DWORD));
    db.write<DWORD>(0);
    db.write<DWORD>(0);
    db.write<DWORD>(WS_CHILD | WS_VISIBLE);
    db.write<WORD>(5);
    db.write<WORD>(y);
    db.write<WORD>(WIDTH / 3 - 5);
    db.write<WORD>(8);
    db.write<DWORD>(-1);
    db.write<DWORD>(0x0082FFFF);
    db.writeStr(var._name.c_str());
    db.write<WORD>(0);

    db.align(sizeof(DWORD));
    db.write<DWORD>(0);
    db.write<DWORD>(0);
    db.write<DWORD>(CBS_DROPDOWNLIST | WS_TABSTOP | WS_CHILD | WS_VISIBLE);
    db.write<WORD>(WIDTH / 3 + 5);
    db.write<WORD>(y);
    db.write<WORD>(WIDTH - WIDTH / 3 - 10);
    db.write<WORD>(36);
    db.write<DWORD>(50000 + id);
    db.write<DWORD>(0x0085ffff);
    db.writeWide(L"");
    db.write<WORD>(0);

    y += LINE;
    id++;
  }

  db.align(sizeof(DWORD));
  db.write<DWORD>(0);
  db.write<DWORD>(0);
  db.write<DWORD>(WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP | BS_DEFPUSHBUTTON);
  db.write<WORD>(WIDTH - 55 - 55);
  db.write<WORD>(y);
  db.write<WORD>(50);
  db.write<WORD>(14);
  db.write<DWORD>(IDOK);
  db.write<DWORD>(0x0080ffff);
  db.writeWide(L"OK");
  db.write<WORD>(0);

  db.align(sizeof(DWORD));
  db.write<DWORD>(0);
  db.write<DWORD>(0);
  db.write<DWORD>(WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP | BS_DEFPUSHBUTTON);
  db.write<WORD>(WIDTH - 55);
  db.write<WORD>(y);
  db.write<WORD>(50);
  db.write<WORD>(14);
  db.write<DWORD>(IDCANCEL);
  db.write<DWORD>(0x0080ffff);
  db.writeWide(L"Cancel");
  db.write<WORD>(0);

  DialogBoxIndirectParam(NULL, db.get(), g_mainWindow, s_dialogProc, (LPARAM)this);
}

INT_PTR CALLBACK Config::s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  Config* self;

  switch (msg)
  {
  case WM_INITDIALOG:
    self = (Config*)lparam;
    self->initControls(hwnd);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)lparam);
    return TRUE;
  
  case WM_SETFONT:
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)
    {
      if (LOWORD(wparam) == IDOK)
      {
        self = (Config*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        self->updateVariables(hwnd);
      }

      EndDialog(hwnd, LOWORD(wparam));
      return TRUE;
    }

    break;

  case WM_CLOSE:
    DestroyWindow(hwnd);
    return TRUE;
  }

  return FALSE;
}

void Config::initControls(HWND hwnd)
{
  DWORD id = 0;

  for (auto it = _variables.begin(); it != _variables.end(); ++it, id++)
  {
    Variable& var = *it;
    HWND combo = GetDlgItem(hwnd, 50000 + id);

    for (auto option: var._options)
    {
      WCHAR wide[256];

      if (MultiByteToWideChar(CP_UTF8, 0, option.c_str(), -1, wide, sizeof(wide) / sizeof(wide[0])) != 0)
      {
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wide);
      }
    }

    SendMessageW(combo, CB_SETCURSEL, var._selected, 0);
  }

  CheckDlgButton(hwnd, 51001, _preserveAspect ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(hwnd, 51002, _linearFilter ? BST_CHECKED : BST_UNCHECKED);
}

void Config::updateVariables(HWND hwnd)
{
  DWORD id = 0;

  for (auto it = _variables.begin(); it != _variables.end(); ++it, id++)
  {
    Variable& var = *it;
    HWND combo = GetDlgItem(hwnd, 50000 + id);

    int selected = SendMessage(combo, CB_GETCURSEL, 0, 0);
    _updated = _updated || selected != var._selected;
    var._selected = selected;
  }

  _preserveAspect = IsDlgButtonChecked(hwnd, 51001) == BST_CHECKED;
  _linearFilter = IsDlgButtonChecked(hwnd, 51002) == BST_CHECKED;
}
