#include "About.h"

#include "SDLComponents/Dialog.h"

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
