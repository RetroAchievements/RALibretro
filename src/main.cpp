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
  }

  app.destroy();
  return ok ? 0 : 1;
}
