#include <SDL.h>
#include <SDL_syswm.h>

#include <Application.h>

extern Application app;

#if 0
void showAbout()
{
  const WORD WIDTH = 280;
  const WORD LINE = 15;

  Dialog db;
  db.init("About");

  WORD y = 0;

  db.addLabel("RALibretro " VERSION " \u00A9 2017-2018 Andre Leiradella @leiradel", 0, y, WIDTH, 8);
  y += LINE;

  db.addEditbox(40000, 0, y, WIDTH, LINE, 12, (char*)getLogContents().c_str(), 0, true);
  y += LINE * 12;

  db.addButton("OK", IDOK, WIDTH - 50, y, 50, 14, true);
  db.show();
}

void shortcut(const SDL_Event* event, bool* done)
{
  unsigned extra;

  switch (_keybinds.translate(event, &extra))
  {
  case KeyBinds::Action::kNothing:      break;
  // Joypad buttons
  case KeyBinds::Action::kButtonUp:     _input.buttonEvent(Input::Button::kUp, extra != 0); break;
  case KeyBinds::Action::kButtonDown:   _input.buttonEvent(Input::Button::kDown, extra != 0); break;
  case KeyBinds::Action::kButtonLeft:   _input.buttonEvent(Input::Button::kLeft, extra != 0); break;
  case KeyBinds::Action::kButtonRight:  _input.buttonEvent(Input::Button::kRight, extra != 0); break;
  case KeyBinds::Action::kButtonX:      _input.buttonEvent(Input::Button::kX, extra != 0); break;
  case KeyBinds::Action::kButtonY:      _input.buttonEvent(Input::Button::kY, extra != 0); break;
  case KeyBinds::Action::kButtonA:      _input.buttonEvent(Input::Button::kA, extra != 0); break;
  case KeyBinds::Action::kButtonB:      _input.buttonEvent(Input::Button::kB, extra != 0); break;
  case KeyBinds::Action::kButtonL:      _input.buttonEvent(Input::Button::kL, extra != 0); break;
  case KeyBinds::Action::kButtonR:      _input.buttonEvent(Input::Button::kR, extra != 0); break;
  case KeyBinds::Action::kButtonL2:     _input.buttonEvent(Input::Button::kL2, extra != 0); break;
  case KeyBinds::Action::kButtonR2:     _input.buttonEvent(Input::Button::kR2, extra != 0); break;
  case KeyBinds::Action::kButtonL3:     _input.buttonEvent(Input::Button::kL3, extra != 0); break;
  case KeyBinds::Action::kButtonR3:     _input.buttonEvent(Input::Button::kR3, extra != 0); break;
  case KeyBinds::Action::kButtonSelect: _input.buttonEvent(Input::Button::kSelect, extra != 0); break;
  case KeyBinds::Action::kButtonStart:  _input.buttonEvent(Input::Button::kStart, extra != 0); break;
  // State management
  case KeyBinds::Action::kSetStateSlot: _slot = extra; break;
  case KeyBinds::Action::kSaveState:    saveState(_slot); break;
  case KeyBinds::Action::kLoadState:    loadState(_slot); break;
  // Emulation speed
  case KeyBinds::Action::kStep:
    if (RA_HardcoreModeIsActive())
    {
      _logger.printf(RETRO_LOG_INFO, "Hardcore mode is active, can't run step-by-step");
    }
    else
    {
      _state = State::kGamePaused;
      _step = true;
    }

    break;

  case KeyBinds::Action::kPauseToggle:
    pauseGame(_state != State::kGamePaused);
    break;

  case KeyBinds::Action::kFastForward:
    if (extra != 0 && _state == State::kGameRunning)
    {
      if (_state == State::kGamePaused)
      {
        RA_SetPaused(false);
      }

      _state = State::kGameTurbo;
      updateMenu(_state, _emulator);
    }
    else if (extra == 0 && _state == State::kGameTurbo)
    {
      _state = State::kGameRunning;
      updateMenu(_state, _emulator);
    }

    break;
  }
}
#endif

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
