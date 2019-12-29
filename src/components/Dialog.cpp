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

#include "Dialog.h"

#include <stdint.h>

extern HWND g_mainWindow;

// dynamically builds a DLGTEMPLATEEX to construct a dialog
// https://blogs.msdn.microsoft.com/oldnewthing/20050429-00
// https://docs.microsoft.com/en-us/windows/win32/dlgbox/dlgitemtemplateex

#define DIALOG_MARGIN 5


Dialog::Dialog()
{
  _template = NULL;
  _size = 0;
  _reserved = 0;
}

Dialog::~Dialog()
{
  free(_template);
}

void Dialog::init(const char* title)
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

  write<WORD>(1);
  write<WORD>(0xffff);
  write<DWORD>(0);
  write<DWORD>(WS_EX_WINDOWEDGE);
  write<DWORD>(DS_3DLOOK | DS_CENTER | DS_MODALFRAME | DS_SHELLFONT | WS_CAPTION | WS_VISIBLE | WS_POPUP);
  _numControls = (WORD*)(&((uint8_t*)_template)[16]);
  write<WORD>(0);
  write<WORD>(0);
  write<WORD>(0);
  _width = (WORD*)(&((uint8_t*)_template)[22]);
  write<WORD>(0);
  _height = (WORD*)(&((uint8_t*)_template)[24]);
  write<WORD>(0);
  writeWide(L"");
  writeWide(L"");
  writeStr(title);

  write<WORD>((WORD)ncm.lfMessageFont.lfHeight);
  write<WORD>((WORD)ncm.lfMessageFont.lfWeight);
  write<BYTE>(ncm.lfMessageFont.lfItalic);
  write<BYTE>(ncm.lfMessageFont.lfCharSet);
  writeWide(ncm.lfMessageFont.lfFaceName);
}

void Dialog::writeDlgItemTemplateEx(DWORD helpId, DWORD exStyle, DWORD style, WORD x, WORD y, WORD cx, WORD cy,
  DWORD id, DWORD windowClass, const char* title, WORD extraCount)
{
  align(sizeof(DWORD));
  write<DWORD>(helpId);
  write<DWORD>(exStyle);
  write<DWORD>(style);
  write<WORD>(x + DIALOG_MARGIN);
  write<WORD>(y + DIALOG_MARGIN);
  write<WORD>(cx);
  write<WORD>(cy);
  write<DWORD>(id);
  write<DWORD>(windowClass);
  writeStr(title);
  write<WORD>(extraCount);
}

void Dialog::addCheckbox(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool* checked)
{
  writeDlgItemTemplateEx(0, 0, BS_AUTOCHECKBOX | WS_TABSTOP | WS_CHILD | WS_VISIBLE, x, y, w, h, id, 0x0080FFFF, caption, 0);

  ControlData cd;
  cd._type = kCheckbox;
  cd._id = id;
  cd._checked = checked;
  _controlData.push_back(cd);

  update(x, y, w, h);
}

void Dialog::addLabel(const char* caption, WORD x, WORD y, WORD w, WORD h)
{
  addLabel(caption, -1, x, y, w, h);
}

void Dialog::addLabel(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h)
{
  writeDlgItemTemplateEx(0, 0, WS_CHILD | WS_VISIBLE, x, y, w, h, id, 0x0082FFFF, caption, 0);

  update(x, y, w, h);
}

void Dialog::addButton(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool isDefault)
{
  DWORD defStyle = isDefault ? BS_DEFPUSHBUTTON : 0;

  writeDlgItemTemplateEx(0, 0, WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP | defStyle, x, y, w, h, id, 0x0080FFFF, caption, 0);

  update(x, y, w, h);
}

void Dialog::addCombobox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD maxDropDownHeight, GetOption get_option, void* udata, int* selected)
{
  writeDlgItemTemplateEx(0, 0, CBS_DROPDOWNLIST | WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_VSCROLL, x, y, w, maxDropDownHeight, id, 0x0085FFFF, "", 0);

  ControlData cd;
  cd._type = kCombobox;
  cd._id = id;
  cd._getOption = get_option;
  cd._udata = udata;
  cd._selected = selected;
  _controlData.push_back(cd);

  update(x, y, w, h);
}

void Dialog::addEditbox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, char* contents, size_t maxSize, bool readOnly)
{
  DWORD multiline = lines > 1 ? ES_MULTILINE|WS_VSCROLL : 0;
  DWORD ro = readOnly ? ES_READONLY : 0;

  writeDlgItemTemplateEx(0, 0, WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE | multiline | ro | WS_TABSTOP | WS_CHILD | WS_VISIBLE,
    x, y, w, h * lines, id, 0x0081FFFF, "", 0);

  ControlData cd;
  cd._type = kEditbox;
  cd._id = id;
  cd._contents = contents;
  cd._maxSize = maxSize;
  _controlData.push_back(cd);

  update(x, y, w, h * lines);
}

bool Dialog::show()
{
  _updated = false;
  DialogBoxIndirectParam(NULL, (LPCDLGTEMPLATE)_template, g_mainWindow, s_dialogProc, (LPARAM)this);
  return _updated;
}

void Dialog::align(size_t alignment)
{
  alignment--;
  _size = (_size + alignment) & ~alignment;
}

void Dialog::write(void* data, size_t size)
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

    _numControls = (WORD*)(&((uint8_t*)_template)[16]);
    _width = (WORD*)(&((uint8_t*)_template)[22]);
    _height = (WORD*)(&((uint8_t*)_template)[24]);
  }

  memcpy((uint8_t*)_template + _size, data, size);
  _size += size;
}

void Dialog::writeStr(const char* str)
{
  int needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);

  if (needed != 0)
  {
    WCHAR* wide = (WCHAR*)alloca(needed * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wide, needed);
    write((void*)wide, needed * sizeof(WCHAR));
  }
}

void Dialog::writeWide(const WCHAR* str)
{
  write((void*)str, (lstrlenW(str) + 1) * sizeof(WCHAR));
}

void Dialog::update(WORD x, WORD y, WORD w, WORD h)
{
  (*_numControls)++;

  x += w + 10;

  if (x > *_width)
  {
    *_width = x;
  }

  y += h + 10;

  if (y > *_height)
  {
    *_height = y;
  }
}

void Dialog::initControls(HWND hwnd)
{
  HWND control;

  for (const auto& cd: _controlData)
  {
    switch (cd._type)
    {
    case kCheckbox:
      CheckDlgButton(hwnd, cd._id, *cd._checked ? BST_CHECKED : BST_UNCHECKED);
      break;
    
    case kCombobox:
      control = GetDlgItem(hwnd, cd._id);

      for (int index = 0;; index++)
      {
        const char* option = cd._getOption(index, cd._udata);

        if (option == NULL)
        {
          break;
        }

        WCHAR wide[256];

        if (MultiByteToWideChar(CP_UTF8, 0, option, -1, wide, sizeof(wide) / sizeof(wide[0])) != 0)
        {
          SendMessageW(control, CB_ADDSTRING, 0, (LPARAM)wide);
        }
      }

      SendMessageW(control, CB_SETCURSEL, *cd._selected, 0);
      break;
    
    case kEditbox:
      SetDlgItemText(hwnd, cd._id, cd._contents);
      break;
    }
  }
}

void Dialog::retrieveData(HWND hwnd)
{
  HWND control;
  bool b;
  int i;

  for (const auto& cd: _controlData)
  {
    switch (cd._type)
    {
    case kCheckbox:
      b = IsDlgButtonChecked(hwnd, cd._id) == BST_CHECKED;
      _updated = _updated || b != *cd._checked;
      *cd._checked = b;
      break;
    
    case kCombobox:
      control = GetDlgItem(hwnd, cd._id);
      i = SendMessage(control, CB_GETCURSEL, 0, 0);
      _updated = _updated || i != *cd._selected;
      *cd._selected = i;
      break;
    
    case kEditbox:
      if (cd._maxSize > 0)
      {
        GetDlgItemText(hwnd, cd._id, cd._contents, cd._maxSize);
      }

      break;
    }
  }
}

INT_PTR CALLBACK Dialog::s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  Dialog* self;

  switch (msg)
  {
  case WM_INITDIALOG:
    self = (Dialog*)lparam;
    self->initControls(hwnd);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)lparam);
    return TRUE;
  
  case WM_SETFONT:
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)
    {
      self = (Dialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

      if (LOWORD(wparam) == IDOK)
        self->retrieveData(hwnd);

      self->markClosed(hwnd);
      EndDialog(hwnd, LOWORD(wparam));
      return TRUE;
    }

    break;

  case WM_CLOSE:
    self = (Dialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    self->markClosed(hwnd);
    DestroyWindow(hwnd);
    return TRUE;
  }

  self = (Dialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (self)
    return self->dialogProc(hwnd, msg, wparam, lparam);

  return FALSE;
}

void Dialog::markClosed(HWND hwnd)
{
  SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
}
