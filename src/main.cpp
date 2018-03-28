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

#include <SDL.h>
#include <SDL_syswm.h>

#include <Application.h>

extern Application app;

bool isGameActive()
{
  return app.isGameActive();
}

void pause()
{
  app.pauseGame(true);
}

void resume()
{
  app.pauseGame(false);
}

void reset()
{
  app.resetGame();
}

void loadROM(const char* path)
{
  app.loadGame(path);
}

int main(int argc, char* argv[])
{
  bool ok = app.init("RALibretro", 640, 480);

  if (ok)
  {
    app.run();
	app.destroy();
  }

  return ok ? 0 : 1;
}
