#pragma once

#include "Emulator.h"
#include <string>
typedef const std::string const_string;

class Application;

class Fsm {
public:
  enum class State {
    CoreLoaded,
    FrameStep,
    GamePaused,
    GameRunning,
    GameTurbo,
    Quit,
    Start,
  };

  Fsm(Application& slave): ctx(slave), __state(State::Start) {}

  State currentState() const { return __state; }

  bool loadCore(Emulator core);
  bool loadGame(const_string path);
  bool normal();
  bool pauseGame();
  bool quit();
  bool resetGame();
  bool resumeGame();
  bool turbo();
  bool unloadCore();
  bool unloadGame();

protected:
  Application& ctx;
  State __state;
};
