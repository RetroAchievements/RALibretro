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
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#define WINDOWS_IGNORE_PACKING_MISMATCH 1 // prevent "Windows headers require the default packing option" error - need to upgrade SDL library to fix it

#include <SDL.h>
#include <SDL_syswm.h>

#include "Application.h"
#include "Util.h"

#include <signal.h>

extern Application app;

bool isGameActive()
{
  return app.isGameActive();
}

void getGameName(char name[], size_t len)
{
  std::string fileName = util::fileName(app.gameName());
  strncpy(name, fileName.c_str(), len);
  name[len - 1] = '\0';
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
  // this is called when the user switches from non-hardcore to hardcore - validate the config settings
  if (app.validateHardcoreEnablement())
    app.resetGame();
}

void loadROM(const char* path)
{
  app.loadGame(path);
}

extern "C" void abort_handler(int signal_number)
{
  app.logger().error("[APP] abort() called");
  app.unloadCore();
  app.destroy();
}

int main(int argc, char* argv[])
{
  signal(SIGABRT, &abort_handler);

  bool ok = app.init("RALibRetro", 640, 480);
  ok &= app.handleArgs(argc, argv);

  if (ok)
  {
#if defined(MINGW) || defined(__MINGW32__) || defined(__MINGW64__)
    app.run();
#else
    __try
    {
      app.run();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      app.logger().error("[APP] Unhandled exception %08X", GetExceptionCode());
    }
#endif

    app.destroy();
  }

  return ok ? 0 : 1;
}
