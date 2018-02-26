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

#include <windows.h>
#include <vector>

class Dialog
{
public:
  typedef const char* (*GetOption)(int index, void* udata);

  Dialog();
  ~Dialog();

  void init(const char* title);
  void addCheckbox(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool* checked);
  void addLabel(const char* caption, WORD x, WORD y, WORD w, WORD h);
  void addButton(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool isDefault);
  void addCombobox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, GetOption get_option, void* udata, int* selected);
  void addEditbox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, char* contents, size_t maxSize, bool readOnly);

  bool show();

protected:
  enum Type
  {
    kCheckbox,
    kCombobox,
    kEditbox
  };

  struct ControlData
  {
    Type _type;
    DWORD _id;

    union
    {
      bool* _checked;

      struct
      {
        GetOption _getOption;
        void* _udata;
        int* _selected;
      };

      struct
      {
        char* _contents;
        size_t _maxSize;
      };
    };
  };

  void align(size_t alignment);

  void write(void* data, size_t size);
  void writeStr(const char* str);
  void writeWide(const WCHAR* str);

  template<typename T> void write(T t)
  {
    write(&t, sizeof(t));
  }

  void update(WORD x, WORD y, WORD w, WORD h);

  void initControls(HWND hwnd);
  void retrieveData(HWND hwnd);

  static INT_PTR CALLBACK s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

  void*  _template;
  size_t _size;
  size_t _reserved;

  WORD* _numControls;
  WORD* _width;
  WORD* _height;

  std::vector<ControlData> _controlData;
  bool _updated;
};
