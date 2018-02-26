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

#include "About.h"

#include "components/Dialog.h"

void aboutDialog(const char* log)
{
  const WORD WIDTH = 280;
  const WORD LINE = 15;

  Dialog db;
  db.init("About");

  WORD y = 0;

  db.addLabel("RALibretro " VERSION " \u00A9 2017-2018 Andre Leiradella @leiradel", 0, y, WIDTH, 8);
  y += LINE;

  db.addEditbox(40000, 0, y, WIDTH, LINE, 12, (char*)log, 0, true);
  y += LINE * 12;

  db.addButton("OK", IDOK, WIDTH - 50, y, 50, 14, true);
  db.show();
}
